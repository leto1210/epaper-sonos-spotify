#pragma once

#include <stdint.h>

// Pilotage de l'ePaper. Toute écriture à l'écran passe par ici : c'est le seul
// endroit qui déclenche un rafraîchissement, ce qui rend le comptage fiable.
//
// Un rafraîchissement complet prend 25 à 30 s et bloque. Voir docs/architecture.md.
namespace display {

// À appeler une fois au démarrage.
void begin();

// Écran de démarrage : nom du firmware et message d'état. Un rafraîchissement.
void showBootScreen(const char* status);

// Nombre de rafraîchissements depuis le démarrage. Sert de garde-fou : si ce
// compteur grimpe alors que rien ne change à l'écran, il y a une régression.
uint32_t refreshCount();

}  // namespace display
