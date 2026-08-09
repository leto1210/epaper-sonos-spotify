#include "core/pause_timer.h"

namespace idle {

void PauseTimer::reset() {
  paused_ = false;
  paused_since_ms_ = 0;
  zone_.clear();
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
