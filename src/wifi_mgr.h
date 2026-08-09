#pragma once

#include <stdint.h>

#include <string>

// Connexion Wi-Fi et horloge. L'heure sert à extrapoler la position dans le
// morceau entre deux sondages, et à dater les publications MQTT.
namespace wifi_mgr {

// Bloquant, avec un plafond de temps. Renvoie faux en cas d'échec — l'appelant
// affiche alors un écran d'erreur plutôt que de tourner en boucle.
bool connect(uint32_t timeout_ms = 30000);

// À appeler régulièrement : reconnecte si le lien est tombé. Non bloquant.
void loop();

bool isConnected();
std::string ip();
int rssi();

// Synchronise l'horloge par NTP. À appeler une fois connecté.
bool syncTime(uint32_t timeout_ms = 10000);

// Vrai dès que l'horloge a été synchronisée au moins une fois.
bool timeIsValid();

// "15:42" dans le fuseau configuré. Vide tant que l'heure n'est pas valide.
std::string localTimeHHMM();

}  // namespace wifi_mgr
