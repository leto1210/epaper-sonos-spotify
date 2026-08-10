#pragma once

#include <cstdint>

// Veille et deep sleep : quand s'endormir, pour combien de temps, ce qui
// réveille. Logique pure — elle ne voit qu'une horloge et des états, donc se
// teste sans matériel.
//
// La device entre en deep sleep après 10 minutes sans rien à jouer, puis se
// réveille toutes les minutes pour un sondage. Un appui de bouton (GPIO4)
// réveille immédiatement.
namespace sleep {

// Durée de chaque tranche de sommeil : 60 secondes.
constexpr uint32_t kSleepIntervalMs = 60 * 1000;

// Délai avant d'entrer en sommeil : rien ne joue depuis ce délai.
constexpr uint32_t kInactivityThresholdMs = 10 * 60 * 1000;

struct Decision {
  bool should_sleep;
  uint32_t duration_ms;
};

class SleepManager {
 public:
  // À appeler à chaque cycle de sondage. Renvoie la décision de sommeil.
  //
  // `anything_playing` : vrai si au moins une zone sait ce qu'elle joue.
  // `user_activity_recent` : vrai si un bouton a été appuyé dans les 2 s.
  Decision updateAndDecide(
      uint32_t now_ms,
      bool anything_playing,
      bool user_activity_recent);

  void reset();

 private:
  uint32_t inactive_since_ms_ = 0;
  uint32_t wake_at_ms_ = 0;
};

}  // namespace sleep
