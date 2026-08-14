#pragma once

#include <cstdint>

// Mise à jour du firmware par le réseau.
//
// Le boîtier n'écoute pas en permanence : il dort par tranches et n'est
// joignable que quelques secondes par minute, ce qui rendrait une écoute
// permanente inutilisable en pratique — et exposerait le port en continu. Une
// fenêtre est donc ouverte à la demande, depuis le bouton « Mode mise à jour »
// de Home Assistant.
//
// Couche d'E/S mince : la décision d'ouvrir vient de `core/ha_discovery`, qui
// est testable, celle de ne pas dormir de `main.cpp`.
namespace ota {

// À appeler une fois le Wi-Fi connecté. Sans effet si `OTA_PASSWORD` est vide :
// une mise à jour anonyme sur le réseau local vaut moins que pas de mise à jour
// du tout.
void begin();

// Ouvre la fenêtre d'écoute pour `OTA_WINDOW_S`. Renvoie faux si l'OTA est
// désactivé faute de mot de passe — l'appelant peut alors le dire plutôt que de
// laisser croire à une fenêtre ouverte.
bool openWindow();

// Vrai tant que la fenêtre court. Le boîtier ne doit alors ni s'endormir ni
// redessiner : un rafraîchissement bloque 37 s, pendant lesquelles `handle()`
// n'est pas appelé et la mise à jour expirerait.
bool isWindowOpen();

// À appeler à chaque tour de boucle.
void handle();

}  // namespace ota
