// Copiez ce fichier vers src/config.h et renseignez vos valeurs.
//   cp config.example.h src/config.h
// src/config.h est git-ignoré : il ne doit JAMAIS être commité.
#pragma once

// --- Wi-Fi ------------------------------------------------------------------
#define WIFI_SSID "mon-reseau"
#define WIFI_PASSWORD "mon-mot-de-passe"

// --- Sonos ------------------------------------------------------------------
// Laissez vide pour utiliser la découverte SSDP automatique. Renseignez l'IP
// d'une enceinte si le SSDP ne passe pas (VLAN, Wi-Fi avec isolation client) :
// n'importe quelle enceinte suffit, la topologie complète en découle.
#define SONOS_SEED_IP ""

// Ordre de préférence quand plusieurs zones jouent en même temps.
// Utilisez les noms exacts affichés dans l'app Sonos.
#define SONOS_ZONE_PRIORITY {"Séjour", "Cuisine", "Bureau"}

// Intervalle de sondage de Sonos, en secondes. Inutile de descendre bas :
// un rafraîchissement de l'ePaper prend déjà 25-30 s.
#define SONOS_POLL_INTERVAL_S 20

// --- MQTT / Home Assistant --------------------------------------------------
// Laissez MQTT_HOST vide pour désactiver complètement l'intégration HA.
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER "epaper"
#define MQTT_PASSWORD "mon-mot-de-passe-mqtt"

// Identifiant unique de l'appareil dans HA (topics + device identifiers).
#define MQTT_DEVICE_ID "reterminal_sonos"
#define MQTT_DEVICE_NAME "ePaper Sonos"

// --- Divers -----------------------------------------------------------------
#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Europe/Paris
