// Vérifie que le harnais de test natif est opérationnel. Les vrais tests
// (parseur DIDL-Lite, sélection de zone, dithering...) arrivent aux livraisons
// suivantes et viennent s'ajouter dans ce répertoire.
#include <unity.h>

#include <cstring>

#include "core/version.h"

void test_firmware_version_is_set() {
  TEST_ASSERT_NOT_NULL(epaper_spotify::kFirmwareVersion);
  TEST_ASSERT_TRUE(std::strlen(epaper_spotify::kFirmwareVersion) > 0);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_firmware_version_is_set);
  return UNITY_END();
}
