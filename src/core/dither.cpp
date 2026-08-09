#include "core/dither.h"

#include <algorithm>
#include <cmath>

namespace dither {
namespace {

// Mesurées sur des panneaux Spectra 6 : les encres sont nettement moins
// saturées que les primaires. Utiliser #FF0000 pour le rouge conduirait le
// calcul d'erreur à croire qu'il vient de poser une couleur bien plus vive
// qu'en réalité, et à compenser en assombrissant tout le voisinage.
constexpr Rgb kPalette[] = {
    {30, 30, 30},     // noir
    {235, 235, 230},  // blanc
    {170, 55, 45},    // rouge
    {60, 120, 70},    // vert
    {50, 70, 140},    // bleu
    {215, 185, 60},   // jaune
};

constexpr int kInkCount = sizeof(kPalette) / sizeof(kPalette[0]);

inline uint8_t clamp8(float value) {
  if (value <= 0.0f) return 0;
  if (value >= 255.0f) return 255;
  return static_cast<uint8_t>(value + 0.5f);
}

}  // namespace

const Rgb& inkColor(Ink ink) {
  return kPalette[static_cast<uint8_t>(ink)];
}

Ink nearestInk(int r, int g, int b) {
  int best = 0;
  long best_distance = -1;

  for (int i = 0; i < kInkCount; ++i) {
    // Distance euclidienne pondérée : l'œil est bien plus sensible au vert
    // qu'au bleu, et une distance brute choisit des encres qui paraissent
    // fausses là où le calcul les dit proches.
    const long dr = r - kPalette[i].r;
    const long dg = g - kPalette[i].g;
    const long db = b - kPalette[i].b;
    const long distance = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
    if (best_distance < 0 || distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return static_cast<Ink>(best);
}

std::vector<Ink> floydSteinberg(const std::vector<Rgb>& pixels, int width, int height) {
  std::vector<Ink> out;
  if (width <= 0 || height <= 0 ||
      pixels.size() < static_cast<size_t>(width) * height) {
    return out;
  }
  out.resize(static_cast<size_t>(width) * height);

  // Deux lignes d'erreur seulement : la ligne courante et la suivante.
  std::vector<float> current(static_cast<size_t>(width) * 3, 0.0f);
  std::vector<float> next(static_cast<size_t>(width) * 3, 0.0f);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t src = static_cast<size_t>(y) * width + x;
      const size_t e = static_cast<size_t>(x) * 3;

      const float r = pixels[src].r + current[e];
      const float g = pixels[src].g + current[e + 1];
      const float b = pixels[src].b + current[e + 2];

      const Ink ink = nearestInk(clamp8(r), clamp8(g), clamp8(b));
      out[src] = ink;

      const Rgb& chosen = inkColor(ink);
      const float er = r - chosen.r;
      const float eg = g - chosen.g;
      const float eb = b - chosen.b;

      // Répartition classique : 7/16 à droite, 3/16 en bas-gauche,
      // 5/16 en bas, 1/16 en bas-droite.
      const auto spread = [&](std::vector<float>& row, int px, float factor) {
        if (px < 0 || px >= width) return;
        const size_t i = static_cast<size_t>(px) * 3;
        row[i] += er * factor;
        row[i + 1] += eg * factor;
        row[i + 2] += eb * factor;
      };

      spread(current, x + 1, 7.0f / 16.0f);
      spread(next, x - 1, 3.0f / 16.0f);
      spread(next, x, 5.0f / 16.0f);
      spread(next, x + 1, 1.0f / 16.0f);
    }

    current.swap(next);
    std::fill(next.begin(), next.end(), 0.0f);
  }

  return out;
}

std::vector<Rgb> downscale(const std::vector<Rgb>& pixels, int src_width, int src_height,
                           int dst_width, int dst_height) {
  std::vector<Rgb> out;
  if (src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0 ||
      pixels.size() < static_cast<size_t>(src_width) * src_height) {
    return out;
  }
  out.resize(static_cast<size_t>(dst_width) * dst_height);

  for (int y = 0; y < dst_height; ++y) {
    const int y0 = y * src_height / dst_height;
    const int y1 = std::max(y0 + 1, (y + 1) * src_height / dst_height);

    for (int x = 0; x < dst_width; ++x) {
      const int x0 = x * src_width / dst_width;
      const int x1 = std::max(x0 + 1, (x + 1) * src_width / dst_width);

      unsigned long r = 0, g = 0, b = 0, count = 0;
      for (int sy = y0; sy < y1 && sy < src_height; ++sy) {
        for (int sx = x0; sx < x1 && sx < src_width; ++sx) {
          const Rgb& p = pixels[static_cast<size_t>(sy) * src_width + sx];
          r += p.r;
          g += p.g;
          b += p.b;
          ++count;
        }
      }
      if (count == 0) count = 1;
      out[static_cast<size_t>(y) * dst_width + x] = {
          static_cast<uint8_t>(r / count),
          static_cast<uint8_t>(g / count),
          static_cast<uint8_t>(b / count),
      };
    }
  }

  return out;
}

}  // namespace dither
