#pragma once

#include "core/ha_discovery.h"
#include "core/weather.h"

// Liaison MQTT avec Home Assistant. Entièrement optionnelle : si `MQTT_HOST`
// est vide dans `src/config.h`, rien n'est tenté et le boîtier fonctionne
// exactement comme avant.
namespace mqtt {

// `onCommand` est appelé depuis `loop()` quand Home Assistant actionne le
// bouton ou le sélecteur.
void begin(void (*onCommand)(const ha::Command&) = nullptr);

// Republie la découverte du sélecteur avec les vrais noms de zones, une fois la
// topologie connue. Sans effet si la liste n'a pas changé.
void publishZoneOptions(const std::vector<std::string>& zones);

// Reconnexion non bloquante et entretien de la session. À appeler à chaque
// tour de boucle.
void loop();

bool isConnected();

// Publie les mesures. Découplé de l'écran à dessein : l'ePaper ne se redessine
// qu'au changement de morceau, mais Home Assistant, lui, reste à jour.
void publishState(const ha::State& state);

void publishTrack(const ha::Track& track);

// Dernière météo reçue de Home Assistant. Le sujet étant retenu par le broker,
// elle arrive dès l'abonnement, sans attendre la prochaine publication.
// `valid` reste faux tant que rien n'a été reçu.
const weather::Report& weatherReport();

}  // namespace mqtt
