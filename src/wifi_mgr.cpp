#include "wifi_mgr.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

namespace wifi_mgr {
namespace {

// Intervalle minimal entre deux tentatives de reconnexion. Sans ce garde-fou,
// une coupure du point d'accès fait tourner la boucle principale à vide.
constexpr uint32_t kReconnectIntervalMs = 10000;

uint32_t g_last_reconnect_ms = 0;
bool g_time_valid = false;

}  // namespace

bool connect(uint32_t timeout_ms) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);  // le boîtier est sur batterie
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[wifi] connexion a %s", WIFI_SSID);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeout_ms) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[wifi] echec apres %lu ms\n", millis() - started);
    return false;
  }

  Serial.printf("[wifi] %s, RSSI %d dBm, en %lu ms\n", ip().c_str(), rssi(),
                millis() - started);
  return true;
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) return;

  const uint32_t now = millis();
  if (now - g_last_reconnect_ms < kReconnectIntervalMs) return;
  g_last_reconnect_ms = now;

  Serial.println("[wifi] lien perdu, reconnexion");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

std::string ip() {
  return WiFi.localIP().toString().c_str();
}

int rssi() {
  return WiFi.RSSI();
}

bool syncTime(uint32_t timeout_ms) {
  configTzTime(TIMEZONE, NTP_SERVER);

  const uint32_t started = millis();
  struct tm now = {};
  while (millis() - started < timeout_ms) {
    if (getLocalTime(&now, 200) && now.tm_year > 120) {  // > année 2020
      g_time_valid = true;
      Serial.printf("[ntp] %04d-%02d-%02d %02d:%02d:%02d (en %lu ms)\n",
                    now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
                    now.tm_min, now.tm_sec, millis() - started);
      return true;
    }
  }

  Serial.println("[ntp] echec de synchronisation");
  return false;
}

bool timeIsValid() {
  return g_time_valid;
}

std::string localTimeHHMM() {
  if (!g_time_valid) return {};

  struct tm now = {};
  if (!getLocalTime(&now, 100)) return {};

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", now.tm_hour, now.tm_min);
  return buffer;
}

}  // namespace wifi_mgr
