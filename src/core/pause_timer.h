#pragma once

#include <cstdint>
#include <string>

// Combien de temps une zone en pause garde-t-elle l'écran ? Au-delà, la fiche
// du morceau n'informe plus de rien et l'écran passe à la météo.
//
// Logique pure : elle ne voit qu'une horloge et un état, donc se teste sans
// enceintes ni panneau.
namespace idle {

// Cinq minutes. Assez pour couvrir une pause de circonstance — on répond au
// téléphone, on change de pièce — sans laisser une fiche figée toute la nuit.
constexpr uint32_t kPauseGraceMs = 5 * 60 * 1000;

// État sérialisable pour deep sleep. Tampons de taille fixe (pas de
// std::string).
//
// On y conserve une *durée écoulée*, jamais une date. Le deep sleep repasse par
// `setup()` et `millis()` y recommence près de zéro : une date de l'éveil
// précédent, relue dans la nouvelle époque, donne un écart négatif. La pause
// n'expirait alors plus jamais et l'écran restait figé sur le morceau.
struct PauseTimerState {
  bool paused = false;
  uint32_t paused_ms = 0;
  char zone[32] = {};
};

class PauseTimer {
 public:
  // À appeler à chaque sondage. Renvoie vrai quand la pause a assez duré pour
  // céder l'écran à la météo.
  bool expired(uint32_t now_ms, bool playing, const std::string& zone);

  void reset();

  // Sérialisation pour deep sleep. `now_ms` sert à convertir la date interne en
  // durée écoulée, seule grandeur qui garde un sens d'une époque à l'autre.
  PauseTimerState serialize(uint32_t now_ms) const;

  // Restauration au réveil. `slept_ms` est le temps passé en sommeil : sans
  // lui, un boîtier qui se réveille toutes les minutes ne verrait jamais passer
  // les cinq minutes de grâce, puisque seul son éveil compterait.
  void deserialize(const PauseTimerState& state, uint32_t now_ms, uint32_t slept_ms);

 private:
  bool paused_ = false;
  uint32_t paused_since_ms_ = 0;
  std::string zone_;
};

}  // namespace idle
