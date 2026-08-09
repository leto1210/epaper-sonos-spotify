// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 2 : réseau et horloge. Le sondage Sonos arrive ensuite.
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "core/version.h"
#include "display.h"
#include "wifi_mgr.h"

namespace {

// Le boîtier est destiné à un SSID « objets connectés », souvent placé sur un
// VLAN distinct de celui des enceintes. Un test TCP explicite vaut mieux qu'un
// délai d'attente inexpliqué au premier sondage Sonos.
void probe(const char* label, const char* host, uint16_t port) {
  if (host == nullptr || host[0] == '\0') return;

  WiFiClient client;
  const uint32_t started = millis();
  const bool reachable = client.connect(host, port, 3000);
  Serial.printf("[reseau] %s %s:%u -> %s (%lu ms)\n", label, host, port,
                reachable ? "joignable" : "INJOIGNABLE", millis() - started);
  client.stop();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);  // laisse le temps au pont CH340 de s'ouvrir
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());

  display::begin();

  // L'écran n'est dessiné qu'une fois l'état du réseau connu : un
  // rafraîchissement coûte 37 s, hors de question d'en dépenser un pour
  // afficher « connexion en cours ».
  const bool online = wifi_mgr::connect();
  if (online) {
    wifi_mgr::syncTime();
    probe("Sonos", SONOS_SEED_IP, 1400);
    probe("MQTT ", MQTT_HOST, MQTT_PORT);
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

  // Trace périodique, sans toucher à l'écran : elle sert à vérifier la
  // reconnexion après une coupure du point d'accès.
  static uint32_t last_log_ms = 0;
  if (millis() - last_log_ms >= 10000) {
    last_log_ms = millis();
    if (wifi_mgr::isConnected()) {
      Serial.printf("[etat] %s  RSSI %d dBm  %s\n", wifi_mgr::ip().c_str(),
                    wifi_mgr::rssi(), wifi_mgr::localTimeHHMM().c_str());
    } else {
      Serial.println("[etat] hors ligne");
    }
  }

  delay(200);
}
