#include <unity.h>

#include "core/wakeup.h"

namespace {

void test_wakeup_green_button() {
  uint32_t mask = (1ULL << wakeup::kGpioGreen);
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kPlayPause, action);
}

void test_wakeup_next_button() {
  uint32_t mask = (1ULL << wakeup::kGpioNext);
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kNext, action);
}

void test_wakeup_previous_button() {
  uint32_t mask = (1ULL << wakeup::kGpioPrevious);
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kPrevious, action);
}

void test_wakeup_no_button() {
  uint32_t mask = 0;
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kNone, action);
}

void test_wakeup_multiple_buttons_green_wins() {
  // Si plusieurs boutons sont pressés, le vert écrase tout.
  uint32_t mask =
      (1ULL << wakeup::kGpioGreen) | (1ULL << wakeup::kGpioNext) |
      (1ULL << wakeup::kGpioPrevious);
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kPlayPause, action);
}

void test_wakeup_multiple_buttons_next_over_previous() {
  // Sans le vert, le suivant l'emporte sur le précédent.
  uint32_t mask =
      (1ULL << wakeup::kGpioNext) | (1ULL << wakeup::kGpioPrevious);
  buttons::Action action = wakeup::wakeupButtonToAction(mask);
  TEST_ASSERT_EQUAL_INT(buttons::Action::kNext, action);
}

void test_ext1_mask_covers_all_buttons() {
  // Vérifier que le masque combiné capture les trois boutons.
  TEST_ASSERT_NOT_EQUAL(0, wakeup::kExt1MaskAll & (1ULL << wakeup::kGpioGreen));
  TEST_ASSERT_NOT_EQUAL(0, wakeup::kExt1MaskAll & (1ULL << wakeup::kGpioNext));
  TEST_ASSERT_NOT_EQUAL(0, wakeup::kExt1MaskAll & (1ULL << wakeup::kGpioPrevious));
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char* argv[]) {
  UNITY_BEGIN();

  RUN_TEST(test_wakeup_green_button);
  RUN_TEST(test_wakeup_next_button);
  RUN_TEST(test_wakeup_previous_button);
  RUN_TEST(test_wakeup_no_button);
  RUN_TEST(test_wakeup_multiple_buttons_green_wins);
  RUN_TEST(test_wakeup_multiple_buttons_next_over_previous);
  RUN_TEST(test_ext1_mask_covers_all_buttons);

  return UNITY_END();
}
