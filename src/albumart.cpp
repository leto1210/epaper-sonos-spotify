#include "albumart.h"

#include <Arduino.h>
#include <JPEGDEC.h>
#include <WiFiClient.h>

#include <algorithm>
#include <new>

namespace albumart {
namespace {

constexpr uint16_t kSonosPort = 1400;
constexpr uint32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kReadTimeoutMs = 8000;

// Une pochette Spotify servie par l'enceinte fait environ 200 Ko. Le plafond
// protège la PSRAM d'une réponse inattendue sans rejeter les images normales.
constexpr size_t kMaxJpegBytes = 600 * 1024;

// Tampons persistants, en PSRAM : les réallouer à chaque morceau fragmenterait
// le tas pour rien, la taille ne changeant qu'au changement de disposition.
uint8_t* g_jpeg = nullptr;
size_t g_jpeg_capacity = 0;
dither::Rgb* g_rgb = nullptr;
size_t g_rgb_capacity = 0;
dither::Rgb* g_small = nullptr;
size_t g_small_capacity = 0;
dither::Ink* g_inks = nullptr;
size_t g_inks_capacity = 0;

template <typename T>
T* ensure(T*& buffer, size_t& capacity, size_t needed) {
  if (buffer != nullptr && capacity >= needed) return buffer;
  if (buffer != nullptr) free(buffer);
  buffer = static_cast<T*>(ps_malloc(needed * sizeof(T)));
  capacity = buffer != nullptr ? needed : 0;
  return buffer;
}

// Contexte du décodeur : JPEGDEC restitue l'image par blocs, dans un ordre qui
// n'est pas celui des lignes.
struct DecodeTarget {
  dither::Rgb* pixels = nullptr;
  int width = 0;
  int height = 0;
};

int onBlockDecoded(JPEGDRAW* draw) {
  auto* target = static_cast<DecodeTarget*>(draw->pUser);
  if (target == nullptr || target->pixels == nullptr) return 0;

  const uint16_t* src = draw->pPixels;
  for (int y = 0; y < draw->iHeight; ++y) {
    const int dy = draw->y + y;
    if (dy < 0 || dy >= target->height) continue;

    for (int x = 0; x < draw->iWidth; ++x) {
      const int dx = draw->x + x;
      if (dx < 0 || dx >= target->width) continue;

      // RGB565 -> RGB888. La perte est sans conséquence : l'étape suivante
      // réduit de toute façon à six teintes.
      const uint16_t p = src[y * draw->iWidth + x];
      target->pixels[static_cast<size_t>(dy) * target->width + dx] = {
          static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31),
          static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63),
          static_cast<uint8_t>((p & 0x1F) * 255 / 31),
      };
    }
  }
  return 1;
}

// Télécharge le corps de la réponse. Renvoie 0 en cas d'échec.
size_t fetch(const std::string& url) {
  // http://<ip>:1400/<chemin>
  const size_t host_start = url.find("://");
  if (host_start == std::string::npos) return 0;
  const size_t host = host_start + 3;
  const size_t colon = url.find(':', host);
  const size_t slash = url.find('/', host);
  if (colon == std::string::npos || slash == std::string::npos) return 0;

  const std::string ip = url.substr(host, colon - host);
  const std::string path = url.substr(slash);

  WiFiClient client;
  if (!client.connect(ip.c_str(), kSonosPort, kConnectTimeoutMs)) {
    Serial.printf("[pochette] connexion impossible a %s\n", ip.c_str());
    return 0;
  }

  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "HOST: " + ip + ":1400\r\n";
  request += "CONNECTION: close\r\n\r\n";
  client.print(request.c_str());

  const uint32_t deadline = millis() + kReadTimeoutMs;
  int status = 0;
  bool chunked = false;
  size_t written = 0;

  // En-têtes.
  while (client.connected() || client.available()) {
    if (millis() > deadline) {
      Serial.println("[pochette] delai depasse dans les en-tetes");
      client.stop();
      return 0;
    }
    if (!client.available()) {
      delay(5);
      continue;
    }
    const String line = client.readStringUntil('\n');
    if (status == 0 && line.startsWith("HTTP/")) status = line.substring(9, 12).toInt();
    // L'enceinte annonce toujours l'image en Transfer-Encoding: chunked.
    if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
      chunked = true;
    }
    if (line.length() <= 1) break;
  }

  // 404 courant : l'enceinte ne connaît pas de pochette pour cette source
  // (entrée TV, radio sans image).
  if (status != 200) {
    Serial.printf("[pochette] HTTP %d\n", status);
    client.stop();
    return 0;
  }
  if (ensure(g_jpeg, g_jpeg_capacity, kMaxJpegBytes) == nullptr) {
    Serial.println("[pochette] PSRAM insuffisante");
    client.stop();
    return 0;
  }

  // Corps. En mode chunked, chaque bloc est précédé de sa taille en
  // hexadécimal et suivi d'un CRLF : recopier le flux tel quel insérait ces
  // en-têtes au milieu de l'image, que le décodeur refusait alors.
  size_t remaining_in_chunk = chunked ? 0 : kMaxJpegBytes;

  while (client.connected() || client.available()) {
    if (millis() > deadline) {
      Serial.println("[pochette] delai depasse");
      break;
    }
    if (!client.available()) {
      delay(5);
      continue;
    }

    if (chunked && remaining_in_chunk == 0) {
      const String header = client.readStringUntil('\n');
      const long size = strtol(header.c_str(), nullptr, 16);
      if (size <= 0) break;  // bloc de fin
      remaining_in_chunk = static_cast<size_t>(size);
      continue;
    }

    const size_t room = kMaxJpegBytes - written;
    if (room == 0) {
      Serial.println("[pochette] image trop volumineuse, tronquee");
      break;
    }
    size_t wanted = std::min(static_cast<size_t>(client.available()), room);
    wanted = std::min(wanted, remaining_in_chunk);
    if (wanted == 0) {
      delay(5);
      continue;
    }

    const int read = client.read(g_jpeg + written, wanted);
    if (read <= 0) continue;
    written += read;
    remaining_in_chunk -= read;

    // Consomme le CRLF qui ferme le bloc.
    if (chunked && remaining_in_chunk == 0) {
      while (client.available() < 2 && millis() < deadline) delay(2);
      client.read();
      client.read();
    }
  }

  client.stop();
  return written;
}

}  // namespace

Bitmap load(const std::string& url, int target_size) {
  Bitmap bitmap;
  if (url.empty() || target_size <= 0) return bitmap;

  const uint32_t started = millis();
  const size_t bytes = fetch(url);
  if (bytes == 0) return bitmap;

  // Sur le tas, jamais sur la pile : l'objet JPEGDEC embarque ses tables de
  // Huffman et ses tampons de blocs, plusieurs dizaines de kilo-octets, quand
  // la tâche loop d'Arduino n'a que 8 Ko. Le déclarer localement faisait
  // déborder la pile et redémarrer le boîtier à chaque morceau.
  auto* jpeg = new (std::nothrow) JPEGDEC();
  if (jpeg == nullptr) {
    Serial.println("[pochette] memoire insuffisante pour le decodeur");
    return bitmap;
  }
  const auto cleanup = [&jpeg]() {
    jpeg->close();
    delete jpeg;
  };

  if (!jpeg->openRAM(g_jpeg, bytes, onBlockDecoded)) {
    Serial.println("[pochette] JPEG illisible");
    delete jpeg;
    return bitmap;
  }

  // Décodage à demi-résolution : une pochette de 640x640 devient 320x320, soit
  // 300 Ko au lieu de 1,2 Mo, tout en restant au-dessus des 200 à 360 px de
  // l'emplacement. Rien ne justifie de décoder plus finement que l'affichage.
  const int full_width = jpeg->getWidth();
  const int full_height = jpeg->getHeight();
  const bool half = full_width / 2 >= target_size && full_height / 2 >= target_size;
  const int width = half ? full_width / 2 : full_width;
  const int height = half ? full_height / 2 : full_height;

  DecodeTarget target;
  target.width = width;
  target.height = height;
  target.pixels = ensure(g_rgb, g_rgb_capacity, static_cast<size_t>(width) * height);
  if (target.pixels == nullptr) {
    Serial.println("[pochette] PSRAM insuffisante pour le decodage");
    cleanup();
    return bitmap;
  }

  jpeg->setUserPointer(&target);
  jpeg->setPixelType(RGB565_LITTLE_ENDIAN);
  const bool decoded = jpeg->decode(0, 0, half ? JPEG_SCALE_HALF : 0) == 1;
  cleanup();

  if (!decoded) {
    Serial.println("[pochette] echec du decodage");
    return bitmap;
  }

  // Recadrage carré au centre : une pochette non carrée existe (compilations,
  // podcasts), et l'étirer déformerait les visages.
  const int side = std::min(width, height);
  const int off_x = (width - side) / 2;
  const int off_y = (height - side) / 2;
  const dither::Rgb* square = target.pixels + static_cast<size_t>(off_y) * width + off_x;

  dither::Rgb* small =
      ensure(g_small, g_small_capacity, static_cast<size_t>(target_size) * target_size);
  dither::Ink* inks =
      ensure(g_inks, g_inks_capacity, static_cast<size_t>(target_size) * target_size);
  if (small == nullptr || inks == nullptr) {
    Serial.println("[pochette] PSRAM insuffisante pour le tramage");
    return bitmap;
  }

  // `downscale` suppose des lignes contiguës : sur une image non carrée, on lui
  // donne la largeur d'origine et un pointeur décalé ne suffirait pas. On ne
  // recadre donc que si l'image est déjà carrée, sinon on réduit tout.
  const bool square_source = (width == height);
  const dither::Rgb* source = square_source ? square : target.pixels;
  const int source_w = square_source ? side : width;
  const int source_h = square_source ? side : height;

  if (!dither::downscale(source, source_w, source_h, small, target_size, target_size)) {
    return bitmap;
  }
  if (!dither::floydSteinberg(small, target_size, target_size, inks)) {
    return bitmap;
  }

  bitmap.pixels = inks;
  bitmap.size = target_size;
  Serial.printf("[pochette] %dx%d -> %dpx en %lu ms (%u ko)\n", full_width, full_height,
                target_size, millis() - started, static_cast<unsigned>(bytes / 1024));
  return bitmap;
}

void release() {
  for (void** buffer : {reinterpret_cast<void**>(&g_jpeg), reinterpret_cast<void**>(&g_rgb),
                        reinterpret_cast<void**>(&g_small),
                        reinterpret_cast<void**>(&g_inks)}) {
    if (*buffer != nullptr) {
      free(*buffer);
      *buffer = nullptr;
    }
  }
  g_jpeg_capacity = g_rgb_capacity = g_small_capacity = g_inks_capacity = 0;
}

}  // namespace albumart
