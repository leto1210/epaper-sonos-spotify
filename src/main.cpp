// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 6 : affichage du morceau en cours. Pochettes et capteurs suivent.
#include <Arduino.h>
#include <WiFi.h>

#include <vector>

#include "albumart.h"
#include "config.h"
#include "core/version.h"
#include "core/layout_plan.h"
#include "core/zone_picker.h"
#include "display.h"
#include "sonos_client.h"
#include "wifi_mgr.h"

namespace {

const std::vector<std::string> kZonePriority = SONOS_ZONE_PRIORITY;

const char* stateLabel(sonos::TransportState state) {
  switch (state) {
    case sonos::TransportState::kPlaying: return "joue";
    case sonos::TransportState::kPaused: return "en pause";
    case sonos::TransportState::kStopped: return "arrete";
    case sonos::TransportState::kTransitioning: return "transition";
    case sonos::TransportState::kUnknown: break;
  }
  return "inconnu";
}

// Empreinte de ce qui est affiché. Un rafraîchissement coûte 37 s : on ne
// redessine que si l'un de ces éléments change réellement.
std::string g_shown_fingerprint;
std::string g_last_zone;

void pollAndRender() {
  std::vector<sonos::ZoneStatus> zones;
  const uint32_t started = millis();

  if (!sonos_client::fetchZones(SONOS_SEED_IP, zones)) {
    Serial.println("[sonos] topologie indisponible");
    return;
  }

  Serial.printf("[sonos] %u zone(s) en %lu ms\n", static_cast<unsigned>(zones.size()),
                millis() - started);
  for (const sonos::ZoneStatus& zone : zones) {
    Serial.printf("[sonos]   %-24s %-15s %s\n", zone.name.c_str(), zone.ip.c_str(),
                  stateLabel(zone.state));
  }

  // On descend le classement jusqu'à une zone qui sait vraiment ce qu'elle
  // joue. Après un basculement Spotify Connect, l'enceinte précédente garde un
  // état PLAYING résiduel sans métadonnées : s'arrêter au premier candidat
  // laissait l'écran figé sur cette zone fantôme.
  const std::vector<sonos::Choice> ranked =
      sonos::rankZones(zones, kZonePriority, g_last_zone);
  if (ranked.empty()) {
    Serial.println("[sonos] aucune zone a afficher");
    return;
  }

  sonos::Choice choice;
  sonos::TrackInfo track;
  for (const sonos::Choice& candidate : ranked) {
    track = sonos_client::fetchPositionInfo(candidate.ip);
    if (track.has_metadata) {
      choice = candidate;
      break;
    }
    const char* raison = "ne sait rien";
    switch (sonos::sourceKind(track.track_uri)) {
      case sonos::SourceKind::kTvInput: raison = "diffuse la television"; break;
      case sonos::SourceKind::kLineIn: raison = "est sur son entree ligne"; break;
      case sonos::SourceKind::kSlave: raison = "est esclave d'un groupe"; break;
      default: break;
    }
    Serial.printf("[sonos] %s ecarte : %s\n", candidate.name.c_str(), raison);
  }

  if (!choice.found) {
    // Aucune zone ne sait quoi que ce soit : on conserve la dernière fiche
    // affichée plutôt que d'effacer l'écran.
    Serial.println("[sonos] aucune metadonnee nulle part, ecran inchange");
    return;
  }
  g_last_zone = choice.name;

  Serial.printf("[sonos] zone retenue : %s (%s), %s\n", choice.name.c_str(),
                choice.ip.c_str(), choice.playing ? "en lecture" : "en pause");

  Serial.printf("[sonos] %s - %s [%s] %d/%d s\n", track.artist.c_str(),
                track.title.c_str(), track.album.c_str(), track.position_s,
                track.duration_s);

  const std::string fingerprint =
      track.track_uri + "|" + track.title + "|" + choice.name +
      (choice.playing ? "|1" : "|0");
  if (fingerprint == g_shown_fingerprint) {
    Serial.println("[ecran] inchange, pas de rafraichissement");
    return;
  }

  // La pochette n'est chargée qu'une fois le redessin décidé : c'est
  // 200 Ko de réseau et une seconde de décodage, inutiles si l'écran ne
  // change pas.
  const int art_size = layout::planTrack(track.title, track.artist, track.album,
                                         display::measureText)
                           .art_size_px;
  const albumart::Bitmap art =
      albumart::load(sonos::albumArtUrl(choice.ip, track), art_size);
  if (!art.valid()) {
    Serial.println("[pochette] indisponible, emplacement de repli");
  }

  display::Status status;
  status.zone = choice.name;
  status.playing = choice.playing;
  // Capteurs et batterie arrivent à la livraison 9.
  status.indoor_temperature_c = 0.0f;
  status.indoor_humidity_pct = 0;

  display::showTrack(track, status, art);
  g_shown_fingerprint = fingerprint;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);  // laisse le temps au pont CH340 de s'ouvrir
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());

  display::begin();

  const bool online = wifi_mgr::connect();
  if (online) {
    wifi_mgr::syncTime();
  } else {
    display::showBootScreen("Wi-Fi indisponible");
  }

  Serial.printf("[boot] termine, %lu rafraichissement(s)\n",
                static_cast<unsigned long>(display::refreshCount()));
}

void loop() {
  wifi_mgr::loop();

  // Premier sondage immédiat, puis toutes les SONOS_POLL_INTERVAL_S secondes.
  static uint32_t last_poll_ms = 0;
  static bool first_poll_done = false;
  if (wifi_mgr::isConnected() && !first_poll_done) {
    first_poll_done = true;
    last_poll_ms = millis();
    pollAndRender();
    return;
  }

  if (wifi_mgr::isConnected() &&
      millis() - last_poll_ms >= SONOS_POLL_INTERVAL_S * 1000UL) {
    last_poll_ms = millis();
    pollAndRender();
  }

  delay(200);
}
