#include "core/sleep_manager.h"

namespace power {

void SleepManager::resumeAfterWake(uint32_t now_ms) {
  // Antidater le début de l'inactivité revient à dire « le compte à rebours est
  // déjà écoulé ». L'arithmétique modulaire de `uint32_t` rend la soustraction
  // correcte même quand `millis()` vaut moins que le seuil, ce qui est le cas
  // dans les secondes qui suivent un réveil.
  inactive_since_ms_ = now_ms - kInactivityThresholdMs;
  wake_at_ms_ = 0;
}

void SleepManager::reset() {
  inactive_since_ms_ = 0;
  wake_at_ms_ = 0;
}

SleepManagerState SleepManager::serialize() const {
  return {inactive_since_ms_, wake_at_ms_};
}

void SleepManager::deserialize(const SleepManagerState& state) {
  inactive_since_ms_ = state.inactive_since_ms;
  wake_at_ms_ = state.wake_at_ms;
}

Decision SleepManager::updateAndDecide(
    uint32_t now_ms,
    bool anything_playing,
    bool user_activity_recent) {
  Decision result;
  result.should_sleep = false;
  result.duration_ms = kSleepIntervalMs;

  // Si quelque chose joue, réinitialiser le compteur d'inactivité.
  if (anything_playing) {
    reset();
    return result;
  }

  // Quelque chose joue ou vient de s'arrêter : noter le moment.
  if (inactive_since_ms_ == 0) {
    inactive_since_ms_ = now_ms;
    return result;
  }

  // Calculer le temps écoulé depuis l'inactivité. Différence signée pour
  // gérer le débordement de millis() au bout de 49 jours.
  int32_t elapsed_ms = static_cast<int32_t>(now_ms - inactive_since_ms_);

  // Activité utilisateur : réinitialiser l'inactivité même en sommeil.
  if (user_activity_recent) {
    // Si on était en sommeil, un appui le coupe court : pas besoin d'attendre
    // le timer. On redémarre un sondage complet.
    if (wake_at_ms_ > 0 && static_cast<int32_t>(now_ms - wake_at_ms_) >= 0) {
      // Timer expiré, on se réveille de toute façon.
      reset();
      return result;
    }

    // Réinitialiser pour ne pas renvoyer "sleep" immédiatement après.
    inactive_since_ms_ = now_ms;
    wake_at_ms_ = 0;
    return result;
  }

  // L'inactivité n'a pas assez duré : rester actif.
  if (elapsed_ms < static_cast<int32_t>(kInactivityThresholdMs)) {
    return result;
  }

  // Assez d'inactivité : passer en sommeil. Calculer la prochaine date de
  // réveil, ou la maintenir si on est déjà en sommeil.
  if (wake_at_ms_ == 0) {
    // Première entrée en sommeil.
    wake_at_ms_ = now_ms + kSleepIntervalMs;
  } else {
    // Déjà en sommeil : vérifier si le timer a expiré.
    if (static_cast<int32_t>(now_ms - wake_at_ms_) >= 0) {
      // Timer expiré : se réveiller une fois, puis se rendormir si toujours
      // inactif. Remettre à zéro pour laisser le cycle prochain décider.
      // Marquer le temps de réveil pour que l'inactivité se compte à partir de là.
      wake_at_ms_ = 0;
      inactive_since_ms_ = now_ms;
      return result;
    }
  }

  // On doit entrer en sommeil ou y rester.
  result.should_sleep = true;
  return result;
}

}  // namespace power
