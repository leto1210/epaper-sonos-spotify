// Tests de la conversion tension -> état de charge. Aucun matériel requis.
#include <unity.h>

#include "core/battery.h"

void test_bounds_map_to_zero_and_hundred() {
  TEST_ASSERT_EQUAL_INT(100, battery::percentFromMillivolts(battery::kFullMv));
  TEST_ASSERT_EQUAL_INT(0, battery::percentFromMillivolts(battery::kEmptyMv));
}

void test_interpolates_between_bounds() {
  const int middle = (battery::kFullMv + battery::kEmptyMv) / 2;
  TEST_ASSERT_INT_WITHIN(2, 50, battery::percentFromMillivolts(middle));
}

void test_clamps_outside_bounds() {
  TEST_ASSERT_EQUAL_INT(100, battery::percentFromMillivolts(battery::kFullMv + 100));
  TEST_ASSERT_EQUAL_INT(0, battery::percentFromMillivolts(battery::kEmptyMv - 100));
}

// Une lecture manquante doit se distinguer d'un accu vide : afficher 0 % quand
// l'ADC n'a rien renvoyé annoncerait une panne inexistante.
void test_missing_reading_is_not_zero_percent() {
  TEST_ASSERT_EQUAL_INT(-1, battery::percentFromMillivolts(0));
  TEST_ASSERT_EQUAL_INT(-1, battery::percentFromMillivolts(-500));
  TEST_ASSERT_EQUAL_INT(-1, battery::percentFromMillivolts(12000));
}

void test_detects_usb_power() {
  TEST_ASSERT_TRUE(battery::isCharging(4300));
  TEST_ASSERT_FALSE(battery::isCharging(4100));
  TEST_ASSERT_FALSE(battery::isCharging(3500));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bounds_map_to_zero_and_hundred);
  RUN_TEST(test_interpolates_between_bounds);
  RUN_TEST(test_clamps_outside_bounds);
  RUN_TEST(test_missing_reading_is_not_zero_percent);
  RUN_TEST(test_detects_usb_power);
  return UNITY_END();
}
