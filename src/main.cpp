// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 1 : l'écran s'allume. Wi-Fi et Sonos arrivent ensuite.
#include <Arduino.h>

#include "core/version.h"
#include "display.h"

void setup() {
  Serial.begin(115200);
  delay(2000);  // laisse le temps au CDC USB de s'énumérer
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());

  display::begin();
  display::showBootScreen("Pret.");

  Serial.printf("[boot] termine, %lu rafraichissement(s)\n",
                static_cast<unsigned long>(display::refreshCount()));
}

void loop() {
  // Rien : l'écran garde son image sans alimentation. Toute la logique de
  // sondage arrive aux livraisons suivantes.
  delay(1000);
}
