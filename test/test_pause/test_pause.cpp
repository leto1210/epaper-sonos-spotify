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

// Le deep sleep repasse par `setup()` : `millis()` y recommence près de zéro.
// Une date de l'époque précédente, relue telle quelle, donnait un écart négatif
// et la pause n'expirait plus jamais — l'écran restait figé sur le morceau.
void test_a_deep_sleep_does_not_erase_the_pause() {
  idle::PauseTimer before;
  const uint32_t paused_at = 300000;  // cinq minutes d'éveil au compteur
  before.expired(paused_at, false, "Sonos Sejour");

  // Quatre minutes de pause, puis une tranche de sommeil de soixante secondes.
  const idle::PauseTimerState saved = before.serialize(paused_at + 4 * 60 * 1000);

  idle::PauseTimer after;
  const uint32_t woke_at = 4000;  // nouvelle époque : `millis()` repart de zéro
  after.deserialize(saved, woke_at, 60 * 1000);

  // Quatre minutes de pause plus une de sommeil : la grâce est échue.
  TEST_ASSERT_TRUE(after.expired(woke_at, false, "Sonos Sejour"));
}

// Le même trajet, interrompu plus tôt : le compteur ne doit pas non plus se
// déclencher d'avance.
void test_a_deep_sleep_does_not_shorten_the_pause_either() {
  idle::PauseTimer before;
  before.expired(300000, false, "Sonos Sejour");
  const idle::PauseTimerState saved = before.serialize(300000 + 60 * 1000);

  idle::PauseTimer after;
  after.deserialize(saved, 4000, 60 * 1000);
  TEST_ASSERT_FALSE(after.expired(4000, false, "Sonos Sejour"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_deep_sleep_does_not_erase_the_pause);
  RUN_TEST(test_a_deep_sleep_does_not_shorten_the_pause_either);
  RUN_TEST(test_playing_never_expires);
  RUN_TEST(test_short_pause_keeps_the_track_on_screen);
  RUN_TEST(test_long_pause_yields_the_screen);
  RUN_TEST(test_resuming_restarts_the_grace_period);
  RUN_TEST(test_changing_zone_restarts_the_grace_period);
  RUN_TEST(test_survives_millis_rollover);
  return UNITY_END();
}
