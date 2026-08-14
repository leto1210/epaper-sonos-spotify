#include "buttons_io.h"

#include <Arduino.h>

namespace buttons_io {
namespace {

// Brochage du reTerminal E1002, voir docs/hardware.md. Les trois entrées sont
// en pull-up interne : un bouton enfoncé tire la broche à la masse.
constexpr int kPinGreen = 3;
constexpr int kPinNext = 4;      // bouton blanc de droite
constexpr int kPinPrevious = 5;  // bouton blanc de gauche
constexpr int kPinBuzzer = 45;

constexpr int kBeepHz = 2400;
constexpr int kBeepMs = 40;

buttons::Controller g_controller;

}  // namespace

void beep() {
  ledcWriteTone(kPinBuzzer, kBeepHz);
  delay(kBeepMs);
  ledcWriteTone(kPinBuzzer, 0);
}

void begin() {
  pinMode(kPinGreen, INPUT_PULLUP);
  pinMode(kPinNext, INPUT_PULLUP);
  pinMode(kPinPrevious, INPUT_PULLUP);

  ledcAttach(kPinBuzzer, kBeepHz, 10);
  ledcWriteTone(kPinBuzzer, 0);

  Serial.println("[boutons] prets (GPIO 3/4/5, buzzer 45)");
}

buttons::Action poll() {
  const buttons::Action action =
      g_controller.update(millis(), digitalRead(kPinPrevious) == LOW,
                          digitalRead(kPinNext) == LOW, digitalRead(kPinGreen) == LOW);

  // Le bip part avant la requête réseau : c'est le seul retour immédiat dont
  // dispose l'utilisateur, l'écran étant hors de portée à cette échelle.
  if (action != buttons::Action::kNone) beep();
  return action;
}

bool refreshDue() { return g_controller.takeRefresh(millis()); }

}  // namespace buttons_io
