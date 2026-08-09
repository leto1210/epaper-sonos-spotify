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

class PauseTimer {
 public:
  // À appeler à chaque sondage. Renvoie vrai quand la pause a assez duré pour
  // céder l'écran à la météo.
  bool expired(uint32_t now_ms, bool playing, const std::string& zone);

  void reset();

 private:
  bool paused_ = false;
  uint32_t paused_since_ms_ = 0;
  std::string zone_;
};

}  // namespace idle
