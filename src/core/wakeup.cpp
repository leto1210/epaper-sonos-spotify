#include "core/wakeup.h"

namespace wakeup {

buttons::Action wakeupButtonToAction(uint32_t ext1_wakeup_mask) {
  // L'ordre de priorité reflète l'utilité : un appui du vert écrase tout,
  // car c'est l'action la plus délibérée. Appui long = force redraw ; appui
  // court = lecture/pause. On ne peut pas le distinguer ici — on ne voit que
  // la broche qui s'est levée. Voir la note dans main.cpp pour la décision.
  if (ext1_wakeup_mask & (1ULL << kGpioGreen)) {
    return buttons::Action::kPlayPause;
  }
  if (ext1_wakeup_mask & (1ULL << kGpioNext)) {
    return buttons::Action::kNext;
  }
  if (ext1_wakeup_mask & (1ULL << kGpioPrevious)) {
    return buttons::Action::kPrevious;
  }
  return buttons::Action::kNone;
}

}  // namespace wakeup
