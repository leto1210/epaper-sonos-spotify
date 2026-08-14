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

// Dormir entre deux sondages **même pendant la lecture**, par tranches de 20 s.
//
// Mesuré : 0,315 W en lecture continue sans cette option, contre ~0,095 W avec.
// Le gain est réel, mais il se paie, et pas seulement en réactivité :
//
//   - un changement de morceau met jusqu'à 20 s de plus à s'afficher, en sus
//     des 37 s du rafraîchissement ;
//   - surtout, **les commandes venues de Home Assistant sont perdues** si elles
//     arrivent pendant un sommeil. La session MQTT n'est pas persistante, donc
//     le broker ne met rien en file d'attente : appuyer sur « Rafraîchir » ou
//     changer de zone n'a alors aucun effet.
//
// Laissée à 0 pour cette raison. Le boîtier étant conçu pour rester alimenté,
// on préfère la fiabilité du pilotage à une autonomie qu'on n'utilise pas.
#define SONOS_SLEEP_WHILE_PLAYING 0

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

// --- Mise à jour par le réseau (OTA) ----------------------------------------
// Le boîtier n'écoute *que* pendant une fenêtre ouverte depuis Home Assistant,
// par le bouton « Mode mise à jour ». Il dort par tranches et ne serait de
// toute façon pas joignable le reste du temps ; une écoute permanente
// exposerait en outre le port en continu.
//
// Un mot de passe vide **désactive l'OTA** : sans lui, n'importe qui sur le
// réseau local pourrait remplacer le firmware. Ce dépôt est public et ce
// fichier sera recopié tel quel — le refus est délibéré, pas une omission.
#define OTA_PASSWORD "changez-moi"

// Durée de la fenêtre, en secondes. Assez pour lancer un `pio run -t upload`
// depuis le Mac, sans laisser le port ouvert indéfiniment.
#define OTA_WINDOW_S 300

// --- Divers -----------------------------------------------------------------
#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Europe/Paris
