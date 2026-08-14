#include "core/pause_timer.h"

// `strncpy`. L'environnement natif la voit par transitivité depuis libstdc++,
// la chaîne croisée non : le défaut n'apparaît qu'avec `pio run`.
#include <cstring>

namespace idle {

void PauseTimer::reset() {
  paused_ = false;
  paused_since_ms_ = 0;
  zone_.clear();
}

PauseTimerState PauseTimer::serialize(uint32_t now_ms) const {
  PauseTimerState state;
  state.paused = paused_;
  state.paused_ms = paused_ ? now_ms - paused_since_ms_ : 0;
  strncpy(state.zone, zone_.c_str(), sizeof(state.zone) - 1);
  state.zone[sizeof(state.zone) - 1] = '\0';
  return state;
}

void PauseTimer::deserialize(const PauseTimerState& state, uint32_t now_ms,
                             uint32_t slept_ms) {
  paused_ = state.paused;
  zone_ = state.zone;
  // Antidater le début de la pause de tout ce qui s'est écoulé : l'éveil
  // précédent et le sommeil qui l'a suivi. L'arithmétique modulaire de
  // `uint32_t` rend la soustraction correcte même quand `millis()` vaut moins
  // que le total, ce qui est le cas dans les secondes qui suivent un réveil.
  paused_since_ms_ = now_ms - (state.paused_ms + slept_ms);
}

bool PauseTimer::expired(uint32_t now_ms, bool playing, const std::string& zone) {
  if (playing) {
    reset();
    zone_ = zone;
    return false;
  }

  // Changer de pièce remet le compteur à zéro : mettre en pause dans le séjour
  // puis reprendre dans la cuisine, c'est une nouvelle écoute, pas la suite
  // d'une pause de dix minutes.
  if (!paused_ || zone != zone_) {
    paused_ = true;
    zone_ = zone;
    paused_since_ms_ = now_ms;
    return false;
  }

  // Différence signée : `millis()` repasse à zéro au bout de 49 jours.
  return static_cast<int32_t>(now_ms - paused_since_ms_) >=
         static_cast<int32_t>(kPauseGraceMs);
}

}  // namespace idle
