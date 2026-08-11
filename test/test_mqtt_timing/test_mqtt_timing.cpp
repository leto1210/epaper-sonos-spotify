#include <unity.h>

#include "core/mqtt_timing.h"

namespace {

void test_mqtt_keepalive_covers_cycle() {
  // Le keepalive doit être strictement supérieur au cycle sleep + réveil.
  TEST_ASSERT_GREATER_THAN(
      mqtt_timing::kSleepIntervalMs + mqtt_timing::kMaxWakeupCostMs,
      mqtt_timing::kKeepAliveMs);
}

void test_safety_margin_is_positive() {
  // Il doit rester une marge de sécurité après le cycle.
  TEST_ASSERT_GREATER_THAN(0, mqtt_timing::kSafetyMarginMs);
}

void test_keepalive_is_sufficient() {
  // Vérification explicite : 90 s doit couvrir 60 s sleep + 2.5 s réveil.
  TEST_ASSERT_EQUAL_UINT32(90000, mqtt_timing::kKeepAliveMs);
  TEST_ASSERT_EQUAL_UINT32(60000, mqtt_timing::kSleepIntervalMs);
  TEST_ASSERT_EQUAL_UINT32(2500, mqtt_timing::kMaxWakeupCostMs);
  // 60000 + 2500 = 62500 < 90000, donc OK.
  TEST_ASSERT_GREATER_THAN(
      mqtt_timing::kSleepIntervalMs + mqtt_timing::kMaxWakeupCostMs,
      mqtt_timing::kKeepAliveMs);
}

void test_short_sleep_keeps_online() {
  const mqtt_timing::AvailabilityPolicy policy =
      mqtt_timing::availabilityPolicyForSleep(60 * 1000);
  TEST_ASSERT_EQUAL_INT(mqtt_timing::AvailabilityPolicy::kKeepOnlineRetained,
                        policy);
}

void test_long_sleep_publishes_offline() {
  const mqtt_timing::AvailabilityPolicy policy =
      mqtt_timing::availabilityPolicyForSleep(5 * 60 * 1000);
  TEST_ASSERT_EQUAL_INT(mqtt_timing::AvailabilityPolicy::kPublishOfflineRetained,
                        policy);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char* argv[]) {
  UNITY_BEGIN();

  RUN_TEST(test_mqtt_keepalive_covers_cycle);
  RUN_TEST(test_safety_margin_is_positive);
  RUN_TEST(test_keepalive_is_sufficient);
  RUN_TEST(test_short_sleep_keeps_online);
  RUN_TEST(test_long_sleep_publishes_offline);

  return UNITY_END();
}
