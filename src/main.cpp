// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 0 : squelette. L'affichage arrive en livraison 1.
#include <Arduino.h>

#include "core/version.h"

void setup() {
  Serial.begin(115200);
  delay(2000);  // laisse le temps au CDC USB de s'énumérer
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());
}

void loop() {
  delay(1000);
}
