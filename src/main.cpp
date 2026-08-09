// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 6 : affichage du morceau en cours. Pochettes et capteurs suivent.
#include <Arduino.h>
#include <WiFi.h>

#include <vector>

#include "config.h"
#include "core/version.h"
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

  const sonos::Choice choice = sonos::pickZone(zones, kZonePriority, g_last_zone);
  if (!choice.found) {
    Serial.println("[sonos] aucune zone a afficher");
    return;
  }
  g_last_zone = choice.name;

  Serial.printf("[sonos] zone retenue : %s (%s), %s\n", choice.name.c_str(),
                choice.ip.c_str(), choice.playing ? "en lecture" : "inactive");

  const sonos::TrackInfo track = sonos_client::fetchPositionInfo(choice.ip);
  if (!track.has_metadata) {
    // Attendu en Spotify Connect dès que la lecture n'est pas active. On
    // conserve la dernière fiche affichée plutôt que d'effacer l'écran.
    Serial.println("[sonos] pas de metadonnees, ecran inchange");
    return;
  }

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

  display::Status status;
  status.zone = choice.name;
  status.playing = choice.playing;
  // Capteurs et batterie arrivent à la livraison 9.
  status.indoor_temperature_c = 0.0f;
  status.indoor_humidity_pct = 0;

  display::showTrack(track, status);
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
