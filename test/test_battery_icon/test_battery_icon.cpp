// Géométrie du pictogramme de batterie. Aucun matériel : le calcul ne voit que
// des entiers, et c'est précisément pourquoi il vit dans `core/`.
#include <unity.h>

#include "core/battery_icon.h"

namespace {

constexpr int16_t kBody = 40;
constexpr int16_t kStroke = 3;

}  // namespace

// Une mesure absente ne doit rien dessiner. Une pile vide se lirait comme une
// batterie à plat — c'est-à-dire comme une mesure.
void test_no_measurement_draws_nothing() {
  const battery_icon::Geometry geometry = battery_icon::plan(-1, kBody, kStroke);
  TEST_ASSERT_FALSE(geometry.visible);
  TEST_ASSERT_EQUAL_INT16(0, geometry.fill_width);
}

// Batterie pleine : la jauge occupe toute la largeur intérieure, ni plus.
void test_full_battery_fills_the_inside_exactly() {
  const battery_icon::Geometry geometry = battery_icon::plan(100, kBody, kStroke);
  TEST_ASSERT_TRUE(geometry.visible);
  TEST_ASSERT_EQUAL_INT16(geometry.inner_width, geometry.fill_width);
  TEST_ASSERT_TRUE(geometry.inner_width < kBody);
}

// Batterie à plat : l'icône reste visible, mais sans aplat. Zéro pour cent est
// une mesure, contrairement à l'absence de mesure.
void test_empty_battery_is_drawn_but_not_filled() {
  const battery_icon::Geometry geometry = battery_icon::plan(0, kBody, kStroke);
  TEST_ASSERT_TRUE(geometry.visible);
  TEST_ASSERT_EQUAL_INT16(0, geometry.fill_width);
}

// La jauge suit le pourcentage.
void test_half_battery_fills_about_half() {
  const battery_icon::Geometry geometry = battery_icon::plan(50, kBody, kStroke);
  TEST_ASSERT_EQUAL_INT16(geometry.inner_width / 2, geometry.fill_width);
}

// Un capteur peut dépasser 100 % : on ramène dans la plage plutôt que de
// laisser l'aplat déborder du corps de la pile.
void test_an_out_of_range_reading_never_overflows_the_body() {
  const battery_icon::Geometry geometry = battery_icon::plan(140, kBody, kStroke);
  TEST_ASSERT_EQUAL_INT16(geometry.inner_width, geometry.fill_width);
}

// Un corps plus étroit que son propre contour ne doit pas produire de largeur
// négative, que le tracé interpréterait comme une valeur énorme non signée.
void test_a_body_narrower_than_its_outline_stays_at_zero() {
  const battery_icon::Geometry geometry = battery_icon::plan(100, 4, kStroke);
  TEST_ASSERT_TRUE(geometry.visible);
  TEST_ASSERT_EQUAL_INT16(0, geometry.inner_width);
  TEST_ASSERT_EQUAL_INT16(0, geometry.fill_width);
}

// La jauge croît avec le niveau, sans retour en arrière.
void test_the_gauge_grows_monotonically() {
  int16_t previous = -1;
  for (int pct = 0; pct <= 100; ++pct) {
    const battery_icon::Geometry geometry = battery_icon::plan(pct, kBody, kStroke);
    TEST_ASSERT_TRUE(geometry.fill_width >= previous);
    previous = geometry.fill_width;
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_no_measurement_draws_nothing);
  RUN_TEST(test_full_battery_fills_the_inside_exactly);
  RUN_TEST(test_empty_battery_is_drawn_but_not_filled);
  RUN_TEST(test_half_battery_fills_about_half);
  RUN_TEST(test_an_out_of_range_reading_never_overflows_the_body);
  RUN_TEST(test_a_body_narrower_than_its_outline_stays_at_zero);
  RUN_TEST(test_the_gauge_grows_monotonically);
  return UNITY_END();
}
