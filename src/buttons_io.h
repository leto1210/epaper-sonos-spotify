#pragma once

#include "core/buttons.h"

// Couche matérielle des boutons : lecture des GPIO et bip du buzzer. Toute la
// logique (anti-rebond, appui long, coalescence) vit dans `core/buttons`.
namespace buttons_io {

void begin();

// À appeler à chaque tour de boucle. Émet le bip d'accusé de réception dès
// qu'une action est reconnue — l'écran, lui, mettra 37 s à suivre.
buttons::Action poll();

// Vrai une seule fois, quand la rafale d'appuis est retombée et qu'un
// rafraîchissement est dû.
bool refreshDue();


}  // namespace buttons_io
