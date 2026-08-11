#pragma once

#include <cstdint>

#include "core/buttons.h"

// Logique de réveil par les trois boutons lors du deep sleep. Avec ext1, on
// sait exactement quel bouton a déclenché le réveil, et on peut agir en
// conséquence. Testable sans matériel : cette logique ne voit que des masques
// de bits et une énumération.
namespace wakeup {

// Les trois pins RTC-eligible sur l'ESP32-S3 — voir boards/seeed_xiao_esp32s3/
// pins_arduino.h. Identiques aux pins de boutons_io.
constexpr uint32_t kGpioGreen = 3;
constexpr uint32_t kGpioNext = 4;
constexpr uint32_t kGpioPrevious = 5;

// Masque ext1 pour tous les trois : actifs à l'état bas.
constexpr uint32_t kExt1MaskAll = (1ULL << kGpioGreen) | (1ULL << kGpioNext) |
                                   (1ULL << kGpioPrevious);

// Décoder le masque ext1 en action de bouton. Si plusieurs boutons ont
// déclenché (cas rare), retourne le premier dans l'ordre : vert > suivant >
// précédent.
buttons::Action wakeupButtonToAction(uint32_t ext1_wakeup_mask);

}  // namespace wakeup
