#pragma once

#include "core/ha_discovery.h"

// Liaison MQTT avec Home Assistant. Entièrement optionnelle : si `MQTT_HOST`
// est vide dans `src/config.h`, rien n'est tenté et le boîtier fonctionne
// exactement comme avant.
namespace mqtt {

void begin();

// Reconnexion non bloquante et entretien de la session. À appeler à chaque
// tour de boucle.
void loop();

bool isConnected();

// Publie les mesures. Découplé de l'écran à dessein : l'ePaper ne se redessine
// qu'au changement de morceau, mais Home Assistant, lui, reste à jour.
void publishState(const ha::State& state);

void publishTrack(const ha::Track& track);

}  // namespace mqtt
