#pragma once

#include <cstdint>

// Temps de fonctionnement, à travers le deep sleep.
//
// `millis()` compte depuis le démarrage, et un réveil de deep sleep *est* un
// démarrage : l'entité « Temps de fonctionnement » de Home Assistant retombait
// donc à deux secondes toutes les minutes, laissant croire à un boîtier qui
// redémarre en boucle. Le seul diagnostic capable de signaler un vrai
// redémarrage intempestif était ainsi rendu inutilisable.
//
// On accumule donc en mémoire RTC, et l'on y ajoute à chaque endormissement
// l'éveil écoulé *et* la durée du sommeil qui va suivre. La grandeur publiée
// devient le temps écoulé depuis le dernier démarrage à froid — ce que son nom
// promet.
//
// Le cumul est en millisecondes sur 64 bits : garder des secondes ferait perdre
// jusqu'à une seconde par cycle de sommeil, soit près d'une demi-heure de
// dérive par jour au rythme d'un réveil par minute.
namespace uptime {

// Temps de fonctionnement total, cumul RTC plus l'éveil en cours.
uint64_t totalMs(uint64_t accumulated_ms, uint32_t awake_ms);

// Ce qu'il faut reporter en mémoire RTC avant de s'endormir.
uint64_t accumulate(uint64_t accumulated_ms, uint32_t awake_ms, uint32_t sleep_ms);

}  // namespace uptime
