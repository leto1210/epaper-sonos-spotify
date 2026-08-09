// Tests de la logique des boutons. Aucun matériel requis : le contrôleur ne
// voit que des niveaux et une horloge, tous deux fournis ici.
#include <unity.h>

#include "core/buttons.h"

namespace {

using buttons::Action;
using buttons::Controller;

// Maintient un niveau pendant une durée donnée, en avançant l'horloge par pas
// de 10 ms comme le ferait la boucle principale, et renvoie la première action
// non nulle rencontrée.
Action hold(Controller& controller, uint32_t& now_ms, uint32_t duration_ms, bool previous,
            bool next, bool green) {
  // On compte les pas plutôt que de comparer à une date de fin : `now_ms` est
  // volontairement poussé près du débordement par l'un des tests.
  Action seen = Action::kNone;
  for (uint32_t elapsed = 0; elapsed <= duration_ms; elapsed += 10) {
    const Action action = controller.update(now_ms, previous, next, green);
    if (action != Action::kNone && seen == Action::kNone) seen = action;
    now_ms += 10;
  }
  return seen;
}

}  // namespace

// Une impulsion plus courte que l'anti-rebond est du bruit de contact.
void test_ignores_bounce_shorter_than_debounce() {
  Controller controller;
  uint32_t now = 1000;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::kNone),
                        static_cast<int>(hold(controller, now, 20, false, true, false)));
  TEST_ASSERT_FALSE(controller.refreshPending());
}

void test_next_and_previous_fire_once_per_press() {
  Controller controller;
  uint32_t now = 1000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::kNext),
                        static_cast<int>(hold(controller, now, 300, false, true, false)));
  // Maintenir le bouton ne rejoue pas l'action : c'est bien un seul appui.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::kNone),
                        static_cast<int>(hold(controller, now, 300, false, true, false)));

  hold(controller, now, 100, false, false, false);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Action::kPrevious),
      static_cast<int>(hold(controller, now, 300, true, false, false)));
}

void test_green_short_press_is_play_pause() {
  Controller controller;
  uint32_t now = 1000;

  // Rien tant que le bouton est tenu : l'action se décide au relâchement.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::kNone),
                        static_cast<int>(hold(controller, now, 300, false, false, true)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Action::kPlayPause),
      static_cast<int>(hold(controller, now, 300, false, false, false)));
}

void test_green_long_press_forces_redraw_and_not_play_pause() {
  Controller controller;
  uint32_t now = 1000;

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Action::kForceRedraw),
      static_cast<int>(hold(controller, now, 1400, false, false, true)));

  // Le relâchement qui suit ne doit pas mettre la musique en pause par-dessus.
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Action::kNone),
      static_cast<int>(hold(controller, now, 300, false, false, false)));
}

// Recette de la livraison : quatre appuis rapides sur « suivant » ⇒ un seul
// rafraîchissement, une fois la rafale terminée.
void test_four_quick_presses_yield_a_single_refresh() {
  Controller controller;
  uint32_t now = 1000;

  int commands = 0;
  int refreshes = 0;
  for (int i = 0; i < 4; ++i) {
    if (hold(controller, now, 100, false, true, false) == Action::kNext) ++commands;
    hold(controller, now, 200, false, false, false);
    if (controller.takeRefresh(now)) ++refreshes;
  }
  TEST_ASSERT_EQUAL_INT(4, commands);
  TEST_ASSERT_EQUAL_INT(0, refreshes);  // la rafale n'est pas encore retombée

  // Le calme revenu, un seul rafraîchissement est dû.
  now += buttons::kCoalesceMs;
  TEST_ASSERT_TRUE(controller.takeRefresh(now));
  TEST_ASSERT_FALSE(controller.takeRefresh(now));
  TEST_ASSERT_FALSE(controller.refreshPending());
}

// Le compteur de millisecondes repasse à zéro au bout de 49 jours ; ni
// l'anti-rebond ni la coalescence ne doivent s'y perdre.
void test_survives_millis_rollover() {
  Controller controller;
  uint32_t now = 0xFFFFFF00u;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::kNext),
                        static_cast<int>(hold(controller, now, 300, false, true, false)));
  hold(controller, now, 100, false, false, false);

  now += buttons::kCoalesceMs;
  TEST_ASSERT_TRUE(controller.takeRefresh(now));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ignores_bounce_shorter_than_debounce);
  RUN_TEST(test_next_and_previous_fire_once_per_press);
  RUN_TEST(test_green_short_press_is_play_pause);
  RUN_TEST(test_green_long_press_forces_redraw_and_not_play_pause);
  RUN_TEST(test_four_quick_presses_yield_a_single_refresh);
  RUN_TEST(test_survives_millis_rollover);
  return UNITY_END();
}
