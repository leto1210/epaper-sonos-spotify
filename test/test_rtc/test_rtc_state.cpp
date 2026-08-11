#include <unity.h>

// `strlen`. Sur macOS, libc++ l'expose par transitivité ; le GCC de la
// forge, non. Le défaut ne se voyait donc qu'en intégration continue.
#include <cstring>

#include "core/pause_timer.h"
#include "core/sleep_manager.h"

namespace {

// Tests pour PauseTimer::serialize() et deserialize()
void test_pause_timer_serialize_empty() {
  idle::PauseTimer timer;
  idle::PauseTimerState state = timer.serialize();

  TEST_ASSERT_FALSE(state.paused);
  TEST_ASSERT_EQUAL_UINT32(0, state.paused_since_ms);
  TEST_ASSERT_EQUAL_STRING("", state.zone);
}

void test_pause_timer_serialize_and_restore() {
  idle::PauseTimer timer1;

  // Mettre le timer1 en pause.
  timer1.expired(1000, false, "Salon");
  timer1.expired(2000, false, "Salon");  // Incremente le timer

  // Sérialiser.
  idle::PauseTimerState state = timer1.serialize();

  // Vérifier les valeurs capturées.
  TEST_ASSERT_TRUE(state.paused);
  TEST_ASSERT_EQUAL_UINT32(1000, state.paused_since_ms);
  TEST_ASSERT_EQUAL_STRING("Salon", state.zone);

  // Créer un nouveau timer et restaurer.
  idle::PauseTimer timer2;
  timer2.deserialize(state);

  // Vérifier que le nouvel état est identique au premier.
  idle::PauseTimerState state2 = timer2.serialize();
  TEST_ASSERT_TRUE(state2.paused);
  TEST_ASSERT_EQUAL_UINT32(1000, state2.paused_since_ms);
  TEST_ASSERT_EQUAL_STRING("Salon", state2.zone);
}

void test_pause_timer_zone_truncation() {
  // Test que les zones longues sont tronquées.
  idle::PauseTimer timer;
  std::string long_zone(100, 'X');
  timer.expired(1000, false, long_zone);

  idle::PauseTimerState state = timer.serialize();
  TEST_ASSERT_EQUAL_UINT32(31, strlen(state.zone));  // 32 - 1 null term
}

// Tests pour SleepManager::serialize() et deserialize()
void test_sleep_manager_serialize_empty() {
  power::SleepManager mgr;
  power::SleepManagerState state = mgr.serialize();

  TEST_ASSERT_EQUAL_UINT32(0, state.inactive_since_ms);
  TEST_ASSERT_EQUAL_UINT32(0, state.wake_at_ms);
}

void test_sleep_manager_serialize_and_restore() {
  power::SleepManager mgr1;
  constexpr uint32_t kThreshold = power::kInactivityThresholdMs;

  // Mettre en sommeil : il faut attendre que kThreshold s'écoule à partir du
  // moment où on détecte qu'aucune zone ne joue.
  mgr1.updateAndDecide(0, true, false);                      // Reset
  mgr1.updateAndDecide(100, false, false);                   // Inactivité début
  power::Decision dec = mgr1.updateAndDecide(100 + kThreshold + 1, false, false);
  TEST_ASSERT_TRUE(dec.should_sleep);

  // Sérialiser juste après décision de sommeil.
  power::SleepManagerState state = mgr1.serialize();

  // Restaurer dans un nouveau manager.
  power::SleepManager mgr2;
  mgr2.deserialize(state);

  // Vérifier que les états sont identiques.
  power::SleepManagerState state2 = mgr2.serialize();
  TEST_ASSERT_EQUAL_UINT32(state.inactive_since_ms, state2.inactive_since_ms);
  TEST_ASSERT_EQUAL_UINT32(state.wake_at_ms, state2.wake_at_ms);
}

void test_sleep_manager_restore_and_continue() {
  power::SleepManager mgr1;
  constexpr uint32_t kThreshold = power::kInactivityThresholdMs;
  constexpr uint32_t kInterval = power::kSleepIntervalMs;

  // Mettre en sommeil.
  mgr1.updateAndDecide(0, true, false);
  mgr1.updateAndDecide(100, false, false);
  mgr1.updateAndDecide(100 + kThreshold + 1, false, false);

  power::SleepManagerState state = mgr1.serialize();

  // Créer un nouveau manager, restaurer, puis vérifier qu'il reste en sommeil.
  power::SleepManager mgr2;
  mgr2.deserialize(state);

  // Simuler un réveil. Après kInterval secondes, mgr2 devrait se réveiller.
  uint32_t wake_time = state.wake_at_ms;
  power::Decision dec = mgr2.updateAndDecide(wake_time + 100, false, false);

  // Le decision doit être "réveille" (should_sleep = false) car le timer a expiré.
  TEST_ASSERT_FALSE(dec.should_sleep);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char* argv[]) {
  UNITY_BEGIN();

  // Tests PauseTimer
  RUN_TEST(test_pause_timer_serialize_empty);
  RUN_TEST(test_pause_timer_serialize_and_restore);
  RUN_TEST(test_pause_timer_zone_truncation);

  // Tests SleepManager
  RUN_TEST(test_sleep_manager_serialize_empty);
  RUN_TEST(test_sleep_manager_serialize_and_restore);
  RUN_TEST(test_sleep_manager_restore_and_continue);

  return UNITY_END();
}
