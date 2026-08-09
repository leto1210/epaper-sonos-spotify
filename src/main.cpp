// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 4 : topologie Sonos et choix de la zone. L'affichage du morceau
// arrive à la livraison suivante.
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

void reportZones() {
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

  const sonos::Choice choice = sonos::pickZone(zones, kZonePriority);
  if (!choice.found) {
    Serial.println("[sonos] aucune zone a afficher");
    return;
  }

  Serial.printf("[sonos] zone retenue : %s (%s), %s\n", choice.name.c_str(),
                choice.ip.c_str(), choice.playing ? "en lecture" : "inactive");

  const sonos::TrackInfo track = sonos_client::fetchPositionInfo(choice.ip);
  if (track.has_metadata) {
    Serial.printf("[sonos] %s - %s [%s] %d/%d s\n", track.artist.c_str(),
                  track.title.c_str(), track.album.c_str(), track.position_s,
                  track.duration_s);
    Serial.printf("[sonos] pochette : %s\n",
                  sonos::albumArtUrl(choice.ip, track).c_str());
  } else {
    // Attendu en Spotify Connect dès que la lecture n'est pas active.
    Serial.println("[sonos] pas de metadonnees pour cette zone");
  }
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
    reportZones();
  }

  const std::string status =
      online ? "Connecte : " + wifi_mgr::ip() + "  " + wifi_mgr::localTimeHHMM()
             : "Wi-Fi indisponible";
  display::showBootScreen(status.c_str());

  Serial.printf("[boot] termine, %lu rafraichissement(s)\n",
                static_cast<unsigned long>(display::refreshCount()));
}

void loop() {
  wifi_mgr::loop();

  // Sondage périodique, sans toucher à l'écran : la livraison 5 branchera le
  // rendu, une fois la détection de changement en place.
  static uint32_t last_poll_ms = 0;
  if (wifi_mgr::isConnected() &&
      millis() - last_poll_ms >= SONOS_POLL_INTERVAL_S * 1000UL) {
    last_poll_ms = millis();
    reportZones();
  }

  delay(200);
}
