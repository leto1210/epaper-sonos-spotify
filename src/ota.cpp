#include "ota.h"

#include <ArduinoOTA.h>

#include "buttons_io.h"
#include "config.h"
#include "core/version.h"

// Options récentes : une `src/config.h` écrite avant elles ne les définit pas.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif
#ifndef OTA_WINDOW_S
#define OTA_WINDOW_S 300
#endif

namespace ota {
namespace {

bool g_enabled = false;
bool g_started = false;
uint32_t g_window_ends_ms = 0;

bool passwordIsSet() {
  return sizeof(OTA_PASSWORD) > 1;  // le tableau contient au moins le zéro final
}

}  // namespace

void begin() {
  g_enabled = passwordIsSet();
  if (!g_enabled) {
    Serial.println("[maj] desactivee : OTA_PASSWORD est vide dans src/config.h");
    return;
  }

  ArduinoOTA.setHostname(MQTT_DEVICE_ID);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    // L'écran ne peut pas servir d'accusé de réception : 37 s le rendraient
    // muet pendant la moitié de la fenêtre. Le buzzer, lui, est immédiat.
    Serial.printf("[maj] debut, version actuelle %s\n", epaper_spotify::kFirmwareVersion);
    buttons_io::beep();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("[maj] terminee, redemarrage");
    buttons_io::beep();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[maj] echec, code %u\n", static_cast<unsigned>(error));
  });
}

bool openWindow() {
  if (!g_enabled) {
    Serial.println("[maj] demande ignoree : aucun mot de passe configure");
    return false;
  }

  // `ArduinoOTA.begin()` ouvre le port : on le retarde jusqu'ici plutôt que de
  // le tenir ouvert depuis le démarrage.
  if (!g_started) {
    ArduinoOTA.begin();
    g_started = true;
  }

  g_window_ends_ms = millis() + OTA_WINDOW_S * 1000UL;
  Serial.printf("[maj] fenetre ouverte pour %d s\n", OTA_WINDOW_S);
  buttons_io::beep();
  return true;
}

bool isWindowOpen() {
  if (!g_started || g_window_ends_ms == 0) return false;

  // Différence signée : `millis()` repasse à zéro au bout de 49 jours.
  if (static_cast<int32_t>(millis() - g_window_ends_ms) >= 0) {
    g_window_ends_ms = 0;
    Serial.println("[maj] fenetre refermee");
    return false;
  }
  return true;
}

void handle() {
  if (!g_started) return;
  ArduinoOTA.handle();
}

}  // namespace ota
