// Copiez ce fichier vers src/config.h et renseignez vos valeurs.
//   cp config.example.h src/config.h
// src/config.h est git-ignoré : il ne doit JAMAIS être commité.
#pragma once

// --- Wi-Fi ------------------------------------------------------------------
#define WIFI_SSID "mon-reseau"
#define WIFI_PASSWORD "mon-mot-de-passe"

// --- Sonos ------------------------------------------------------------------
// IP d'une enceinte, n'importe laquelle : la topologie complète (toutes les
// zones, leurs IP et leurs coordinateurs) s'en déduit par SOAP.
//
// Renseignez-la dès que le boîtier et les enceintes ne sont pas sur le même
// VLAN — cas courant quand le boîtier est sur un SSID « objets connectés » :
// la découverte SSDP repose sur du multicast, qui ne franchit pas un routeur.
// Laissez vide uniquement si tout le monde est sur le même sous-réseau.
#define SONOS_SEED_IP "192.168.1.50"

// Ordre de préférence quand plusieurs zones jouent en même temps.
//
// Les noms doivent correspondre **exactement** à ceux de l'app Sonos, préfixe
// compris : une pièce « Séjour » y apparaît souvent comme « Sonos Séjour ».
// Le firmware liste les noms réels sur le port série au démarrage.
//
// Cette liste ne départage que des zones qui jouent : une pièce favorite mise
// en pause ne masquera jamais celle où la musique tourne réellement.
#define SONOS_ZONE_PRIORITY {"Sonos Séjour", "Sonos Beam", "Sonos Cuisine"}

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
