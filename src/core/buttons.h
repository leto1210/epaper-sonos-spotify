#pragma once

#include <cstdint>

// Logique des trois boutons du reTerminal, sans dépendance Arduino : elle ne
// voit que des niveaux logiques et une horloge, ce qui la rend testable
// intégralement sur le Mac.
//
// Trois responsabilités, dans cet ordre :
//   1. anti-rebond — un niveau doit tenir kDebounceMs avant d'être cru ;
//   2. appui long — distinguer « lecture/pause » d'un redessin forcé ;
//   3. coalescence — un rafraîchissement coûte 37 s, donc quatre appuis
//      rapprochés ne doivent en déclencher qu'un seul, une fois le calme revenu.
namespace buttons {

// Voir docs/hardware.md pour le brochage : blanc gauche, blanc droit, vert.
enum class Button { kPrevious, kNext, kGreen };

enum class Action {
  kNone,
  kPrevious,
  kNext,
  kPlayPause,
  kForceRedraw,  // appui long sur le bouton vert
};

constexpr uint32_t kDebounceMs = 50;
constexpr uint32_t kLongPressMs = 1000;

// Délai de calme avant de redessiner. Il doit couvrir une rafale d'appuis :
// en dessous, on paierait un rafraîchissement par morceau sauté.
constexpr uint32_t kCoalesceMs = 1500;

class Controller {
 public:
  // À appeler à chaque tour de boucle avec les niveaux *déjà* remis à
  // l'endroit (vrai = bouton enfoncé ; les entrées sont en pull-up, donc
  // actives à l'état bas côté matériel).
  //
  // Renvoie au plus une action par appel : deux boutons enfoncés au même
  // instant relèvent de la fausse manœuvre, et en traiter un seul évite
  // d'envoyer deux commandes contradictoires à l'enceinte.
  Action update(uint32_t now_ms, bool previous_down, bool next_down, bool green_down);

  // Vrai une seule fois, lorsque le délai de calme est écoulé depuis la
  // dernière action. Consomme la demande.
  bool takeRefresh(uint32_t now_ms);

  bool refreshPending() const { return refresh_pending_; }

 private:
  struct Channel {
    bool raw = false;         // dernier niveau observé
    bool stable = false;      // niveau retenu après anti-rebond
    uint32_t changed_ms = 0;  // date du dernier changement du niveau brut
    uint32_t pressed_ms = 0;  // date de l'appui stable en cours
    bool long_fired = false;  // l'appui long a déjà produit son action
  };

  // Met à jour un canal et indique s'il vient de passer à l'état stable
  // demandé (appui ou relâchement).
  bool settle(Channel& channel, bool level, uint32_t now_ms, bool& released);

  void scheduleRefresh(uint32_t now_ms);

  Channel previous_;
  Channel next_;
  Channel green_;

  bool refresh_pending_ = false;
  uint32_t refresh_due_ms_ = 0;
};

}  // namespace buttons
