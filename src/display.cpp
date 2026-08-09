#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "core/version.h"

namespace display {
namespace {

EPaper epaper;
uint32_t g_refresh_count = 0;

// Marges du cadre, en pixels. L'écran fait 800x480.
constexpr int16_t kMargin = 40;

// Les polices numérotées de Seeed_GFX ne sont pas des polices générales : la 6
// ne contient que `1234567890:-.apm` (c'est une police d'horloge) et la 7 que
// des chiffres. Les caractères absents sont supprimés sans avertissement — un
// titre y perd la moitié de ses lettres. On n'utilise donc que les FreeFonts.

// Envoie le tampon vers l'écran. Bloquant : 25 à 30 s.
void commit() {
  const uint32_t started = millis();
  epaper.update();
  ++g_refresh_count;
  Serial.printf("[ecran] rafraichissement #%lu en %lu ms\n",
                static_cast<unsigned long>(g_refresh_count), millis() - started);
}

}  // namespace

void begin() {
  epaper.begin();
  Serial.printf("[ecran] %dx%d, 6 couleurs\n", epaper.width(), epaper.height());
}

void showBootScreen(const char* status) {
  epaper.fillScreen(TFT_WHITE);
  epaper.drawRect(kMargin, kMargin, epaper.width() - 2 * kMargin,
                  epaper.height() - 2 * kMargin, TFT_BLACK);

  epaper.setTextSize(1);

  epaper.setTextColor(TFT_BLACK);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString("ePaper Spotify", kMargin + 30, kMargin + 40);

  epaper.setTextColor(TFT_BLUE);
  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.drawString("Sonos sur reTerminal E1002", kMargin + 30, kMargin + 110);

  epaper.setTextColor(TFT_BLACK);
  epaper.drawString(status, kMargin + 30, epaper.height() / 2 + 20);

  epaper.setTextColor(TFT_RED);
  epaper.drawString(epaper_spotify::kFirmwareVersion, kMargin + 30,
                    epaper.height() - kMargin - 60);

  commit();
}

uint32_t refreshCount() {
  return g_refresh_count;
}

}  // namespace display
