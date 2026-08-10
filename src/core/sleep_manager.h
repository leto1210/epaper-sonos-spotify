#pragma once

#include <cstdint>

// Veille et deep sleep : quand s'endormir, pour combien de temps, ce qui
// réveille. Logique pure — elle ne voit qu'une horloge et des états, donc se
// teste sans matériel.
//
// Le boîtier entre en deep sleep après 10 minutes sans rien à jouer, puis se
// réveille toutes les minutes pour un sondage. Un appui de bouton (GPIO4)
// réveille immédiatement.
//
// L'espace de noms s'appelle `power` et non `sleep` : `unistd.h` déclare une
// fonction `sleep()` au niveau global, et le compilateur croisé refuse de voir
// le même nom désigner deux choses. Le défaut n'apparaît qu'à la compilation
// pour la cible — l'environnement natif compile sans broncher.
namespace power {

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

  // À appeler après un réveil de deep sleep, une fois le sondage effectué.
  //
  // Le deep sleep repasse par `setup()` : l'objet est reconstruit et a tout
  // oublié. Sans cette reprise, le boîtier réclamerait dix nouvelles minutes
  // d'inactivité avant de se rendormir — il resterait éveillé dix minutes pour
  // une minute de sommeil, l'inverse de ce qu'on cherche. Ici, l'inactivité est
  // réputée déjà écoulée : si rien ne joue, le sommeil reprend aussitôt.
  void resumeAfterWake(uint32_t now_ms);

  void reset();

 private:
  uint32_t inactive_since_ms_ = 0;
  uint32_t wake_at_ms_ = 0;
};

}  // namespace power
