// Tests du gestionnaire de sommeil. Aucun matériel : le contrôleur ne voit
// qu'une horloge et des états.
#include <unity.h>

#include "core/sleep_manager.h"

namespace {

constexpr uint32_t kPoll = 20000;  // intervalle de sondage, 20 s

}  // namespace

// Tant qu'il y a de la musique, on ne dort jamais.
void test_playing_never_sleeps() {
  power::SleepManager mgr;
  uint32_t now = 1000;
  for (int i = 0; i < 100; ++i, now += kPoll) {
    power::Decision d = mgr.updateAndDecide(now, true, false);
    TEST_ASSERT_FALSE(d.should_sleep);
  }
}

// Rien ne joue mais pas assez longtemps : rester actif.
void test_short_inactivity_stays_active() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Arrêter la musique.
  mgr.updateAndDecide(now, false, false);

  // Attendre 5 min : pas assez.
  now += 5 * 60 * 1000;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_FALSE(d.should_sleep);
}

// Rien ne joue depuis 10 min : entrer en sommeil.
void test_long_inactivity_sleeps() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  mgr.updateAndDecide(now, false, false);

  // Attendre exactement 10 min.
  now += power::kInactivityThresholdMs;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_TRUE(d.should_sleep);
  TEST_ASSERT_EQUAL_UINT32(power::kSleepIntervalMs, d.duration_ms);
}

// Timer de sommeil expir : se réveiller pour un sondage.
void test_sleep_timer_expires_wakes_up() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Entrer en sommeil.
  mgr.updateAndDecide(now, false, false);
  now += power::kInactivityThresholdMs;
  power::Decision d1 = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_TRUE(d1.should_sleep);

  // Attendre 60 s (durée du sommeil).
  now += power::kSleepIntervalMs;
  power::Decision d2 = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_FALSE(d2.should_sleep);  // Réveillé pour sondage
}

// Pendant le sommeil, appui de bouton : réveille immédiatement.
void test_button_during_sleep_wakes_immediately() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Entrer en sommeil.
  mgr.updateAndDecide(now, false, false);
  now += power::kInactivityThresholdMs;
  mgr.updateAndDecide(now, false, false);

  // Attendre 30 s de sommeil (pas d'expiration).
  now += power::kSleepIntervalMs / 2;

  // Appui de bouton.
  power::Decision d = mgr.updateAndDecide(now, false, true);
  TEST_ASSERT_FALSE(d.should_sleep);  // Réveille quand même
}

// La musique reprend pendant le sommeil : réveille et relance les sondages.
void test_activity_resumes_during_sleep_wakes() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Entrer en sommeil.
  mgr.updateAndDecide(now, false, false);
  now += power::kInactivityThresholdMs;
  mgr.updateAndDecide(now, false, false);

  // Attendre 30 s : pas encore d'expiration.
  now += power::kSleepIntervalMs / 2;

  // La musique reprend.
  power::Decision d = mgr.updateAndDecide(now, true, false);
  TEST_ASSERT_FALSE(d.should_sleep);
}

// Après un réveil sur timer, rien ne joue : attendre à nouveau.
void test_wake_then_inactivity_resleeps() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Cycle 1 : inactif, dormir 60 s.
  mgr.updateAndDecide(now, false, false);
  now += power::kInactivityThresholdMs;
  mgr.updateAndDecide(now, false, false);

  // Timer expire : réveille.
  now += power::kSleepIntervalMs;
  mgr.updateAndDecide(now, false, false);

  // Attendre que la prochaine période d'inactivité déclenche le sommeil à
  // nouveau (10 min à partir du réveil).
  now += power::kInactivityThresholdMs;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_TRUE(d.should_sleep);  // Re-endormi
}

// Débordement de millis() au bout de 49 jours : la différence signée tient bon.
void test_survives_millis_rollover() {
  power::SleepManager mgr;
  uint32_t now = 0xFFFFF000u;

  // Arrêter la musique.
  mgr.updateAndDecide(now, false, false);

  // Avancer de 10 min (déborde).
  now += power::kInactivityThresholdMs;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_TRUE(d.should_sleep);
}

// Appui de bouton avant d'atteindre le seuil d'inactivité : réinitialiser.
void test_activity_resets_inactivity_timer() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Arrêter la musique.
  mgr.updateAndDecide(now, false, false);

  // Attendre 5 min (pas assez pour dormir).
  now += 5 * 60 * 1000;
  mgr.updateAndDecide(now, false, false);

  // Appui de bouton.
  mgr.updateAndDecide(now, false, true);

  // Attendre 5 autres minutes : la première période de 5 min ne compte plus.
  now += 5 * 60 * 1000;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_FALSE(d.should_sleep);  // Toujours actif
}

// Appui de bouton réinitialise, puis nouvelle inactivité.
void test_activity_then_inactivity_cycle() {
  power::SleepManager mgr;
  uint32_t now = 1000;

  // Cycle 1 : stop.
  mgr.updateAndDecide(now, false, false);
  now += 5 * 60 * 1000;

  // Appui : réinitialise le compteur.
  mgr.updateAndDecide(now, false, true);

  // Attendre 10 min à partir du dernier appui.
  now += power::kInactivityThresholdMs;
  power::Decision d = mgr.updateAndDecide(now, false, false);
  TEST_ASSERT_TRUE(d.should_sleep);
}

// Le deep sleep repasse par `setup()` : l'objet est reconstruit et a tout
// oublié. Sans reprise, le boîtier resterait éveillé dix minutes pour une
// minute de sommeil — l'inverse du but recherché.
void test_resume_after_wake_sleeps_again_without_a_new_countdown() {
  power::SleepManager mgr;
  uint32_t now = 5000;  // quelques secondes après un réveil

  // Sans reprise, il faudrait attendre le seuil complet.
  TEST_ASSERT_FALSE(mgr.updateAndDecide(now, false, false).should_sleep);

  mgr.resumeAfterWake(now);
  TEST_ASSERT_TRUE(mgr.updateAndDecide(now, false, false).should_sleep);
}

// Une reprise ne doit pas endormir un boîtier qui joue de la musique.
void test_resume_after_wake_yields_to_playback() {
  power::SleepManager mgr;
  uint32_t now = 5000;
  mgr.resumeAfterWake(now);
  TEST_ASSERT_FALSE(mgr.updateAndDecide(now, true, false).should_sleep);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_resume_after_wake_sleeps_again_without_a_new_countdown);
  RUN_TEST(test_resume_after_wake_yields_to_playback);
  RUN_TEST(test_playing_never_sleeps);
  RUN_TEST(test_short_inactivity_stays_active);
  RUN_TEST(test_long_inactivity_sleeps);
  RUN_TEST(test_sleep_timer_expires_wakes_up);
  RUN_TEST(test_button_during_sleep_wakes_immediately);
  RUN_TEST(test_activity_resumes_during_sleep_wakes);
  RUN_TEST(test_wake_then_inactivity_resleeps);
  RUN_TEST(test_survives_millis_rollover);
  RUN_TEST(test_activity_resets_inactivity_timer);
  RUN_TEST(test_activity_then_inactivity_cycle);
  return UNITY_END();
}
