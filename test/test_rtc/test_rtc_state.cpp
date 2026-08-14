#include <unity.h>

// `strlen`. Sur macOS, libc++ l'expose par transitivité ; le GCC de la
// forge, non. Le défaut ne se voyait donc qu'en intégration continue.
#include <cstring>

#include "core/pause_timer.h"
#include "core/sleep_manager.h"
#include "core/uptime.h"

namespace {

// Tests pour PauseTimer::serialize() et deserialize()
void test_pause_timer_serialize_empty() {
  idle::PauseTimer timer;
  idle::PauseTimerState state = timer.serialize(0);

  TEST_ASSERT_FALSE(state.paused);
  TEST_ASSERT_EQUAL_UINT32(0, state.paused_ms);
  TEST_ASSERT_EQUAL_STRING("", state.zone);
}

void test_pause_timer_serialize_and_restore() {
  idle::PauseTimer timer1;

  // Mettre le timer1 en pause.
  timer1.expired(1000, false, "Salon");
  timer1.expired(2000, false, "Salon");  // Incremente le timer

  // Sérialiser.
  // Ce qui traverse le sommeil est une durée, pas une date : la pause a
  // commence a 1000 et on serialise a 2000, soit une seconde ecoulee.
  idle::PauseTimerState state = timer1.serialize(2000);

  // Vérifier les valeurs capturées.
  TEST_ASSERT_TRUE(state.paused);
  TEST_ASSERT_EQUAL_UINT32(1000, state.paused_ms);
  TEST_ASSERT_EQUAL_STRING("Salon", state.zone);

  // Créer un nouveau timer et restaurer, dans une époque neuve et sans
  // sommeil credite : la duree ecoulee doit se retrouver telle quelle.
  idle::PauseTimer timer2;
  timer2.deserialize(state, 500, 0);

  // Vérifier que le nouvel état est identique au premier.
  idle::PauseTimerState state2 = timer2.serialize(500);
  TEST_ASSERT_TRUE(state2.paused);
  TEST_ASSERT_EQUAL_UINT32(1000, state2.paused_ms);
  TEST_ASSERT_EQUAL_STRING("Salon", state2.zone);
}

void test_pause_timer_zone_truncation() {
  // Test que les zones longues sont tronquées.
  idle::PauseTimer timer;
  std::string long_zone(100, 'X');
  timer.expired(1000, false, long_zone);

  idle::PauseTimerState state = timer.serialize(1000);
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

// --- Temps de fonctionnement à travers le sommeil ----------------------------

// Ce que voyait Home Assistant : « 2,00 s » alors que le boîtier tournait
// depuis quarante minutes. Le sommeil doit compter.
void test_uptime_counts_the_sleep_not_just_the_waking() {
  // Trois cycles : 5 s d'éveil, 60 s de sommeil.
  uint64_t accumulated = 0;
  for (int i = 0; i < 3; ++i) {
    accumulated = uptime::accumulate(accumulated, 5000, 60000);
  }

  // Au quatrième réveil, deux secondes après le redémarrage.
  const uint64_t total = uptime::totalMs(accumulated, 2000);

  // 3 x 65 s de cycles, plus les 2 s en cours.
  TEST_ASSERT_EQUAL_UINT64(197000, total);
}

// La grandeur doit croître d'un cycle à l'autre : c'est tout ce qu'un
// diagnostic de redémarrage demande.
void test_uptime_never_goes_backwards_across_a_wake() {
  uint64_t accumulated = 0;
  uint64_t previous = 0;

  for (int i = 0; i < 20; ++i) {
    // Fin de l'éveil, juste avant de s'endormir.
    const uint64_t before_sleep = uptime::totalMs(accumulated, 8000);
    TEST_ASSERT_TRUE(before_sleep > previous);
    previous = before_sleep;

    accumulated = uptime::accumulate(accumulated, 8000, 60000);

    // Tout début de l'éveil suivant : `millis()` est reparti de zéro.
    const uint64_t after_wake = uptime::totalMs(accumulated, 30);
    TEST_ASSERT_TRUE(after_wake > previous);
    previous = after_wake;
  }
}

// Le cumul est en 64 bits : un boîtier qui tourne plus de 49 jours ne doit pas
// voir son temps de fonctionnement repartir de zéro comme le ferait `millis()`.
void test_uptime_survives_beyond_the_millis_ceiling() {
  // 60 jours en millisecondes, au-delà des 2^32 ms (~49,7 jours).
  const uint64_t sixty_days = 60ULL * 24 * 3600 * 1000;
  const uint64_t total = uptime::totalMs(sixty_days, 1000);

  TEST_ASSERT_TRUE(total > 0xFFFFFFFFULL);
  TEST_ASSERT_EQUAL_UINT64(sixty_days + 1000, total);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char* argv[]) {
  UNITY_BEGIN();

  // Tests uptime
  RUN_TEST(test_uptime_counts_the_sleep_not_just_the_waking);
  RUN_TEST(test_uptime_never_goes_backwards_across_a_wake);
  RUN_TEST(test_uptime_survives_beyond_the_millis_ceiling);

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
