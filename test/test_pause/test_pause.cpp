// Tests du délai de grâce sur pause. Aucun matériel : le compteur ne voit
// qu'une horloge et un état.
#include <unity.h>

#include "core/pause_timer.h"

namespace {

constexpr uint32_t kPoll = 20000;  // intervalle de sondage, 20 s

}  // namespace

void test_playing_never_expires() {
  idle::PauseTimer timer;
  uint32_t now = 1000;
  for (int i = 0; i < 100; ++i, now += kPoll) {
    TEST_ASSERT_FALSE(timer.expired(now, true, "Sonos Holiday"));
  }
}

// Une pause courte garde la fiche à l'écran : c'est encore l'écoute en cours.
void test_short_pause_keeps_the_track_on_screen() {
  idle::PauseTimer timer;
  uint32_t now = 1000;
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Holiday"));

  now += 2 * 60 * 1000;
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Holiday"));
}

void test_long_pause_yields_the_screen() {
  idle::PauseTimer timer;
  uint32_t now = 1000;
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Holiday"));

  now += idle::kPauseGraceMs;
  TEST_ASSERT_TRUE(timer.expired(now, false, "Sonos Holiday"));
}

// Reprendre la lecture doit rendre l'écran au morceau, et repartir de zéro.
void test_resuming_restarts_the_grace_period() {
  idle::PauseTimer timer;
  uint32_t now = 1000;
  timer.expired(now, false, "Sonos Holiday");

  now += idle::kPauseGraceMs;
  TEST_ASSERT_TRUE(timer.expired(now, false, "Sonos Holiday"));

  now += kPoll;
  TEST_ASSERT_FALSE(timer.expired(now, true, "Sonos Holiday"));

  now += kPoll;
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Holiday"));
}

// Mettre en pause dans le séjour puis reprendre dans la cuisine, c'est une
// nouvelle écoute — pas la suite d'une pause de dix minutes.
void test_changing_zone_restarts_the_grace_period() {
  idle::PauseTimer timer;
  uint32_t now = 1000;
  timer.expired(now, false, "Sonos Holiday");

  now += idle::kPauseGraceMs;
  TEST_ASSERT_TRUE(timer.expired(now, false, "Sonos Holiday"));
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Cuisine"));
}

void test_survives_millis_rollover() {
  idle::PauseTimer timer;
  uint32_t now = 0xFFFFF000u;
  TEST_ASSERT_FALSE(timer.expired(now, false, "Sonos Holiday"));

  now += idle::kPauseGraceMs;  // déborde
  TEST_ASSERT_TRUE(timer.expired(now, false, "Sonos Holiday"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_playing_never_expires);
  RUN_TEST(test_short_pause_keeps_the_track_on_screen);
  RUN_TEST(test_long_pause_yields_the_screen);
  RUN_TEST(test_resuming_restarts_the_grace_period);
  RUN_TEST(test_changing_zone_restarts_the_grace_period);
  RUN_TEST(test_survives_millis_rollover);
  return UNITY_END();
}
