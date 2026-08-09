#include "core/battery.h"

namespace battery {

int percentFromMillivolts(int millivolts) {
  // Une lecture nulle ou manifestement hors plage signale un ADC non lu ou une
  // mesure ratée. Renvoyer 0 % ferait croire à un accu vide.
  if (millivolts < 2500 || millivolts > 5500) return -1;

  if (millivolts >= kFullMv) return 100;
  if (millivolts <= kEmptyMv) return 0;

  // Interpolation linéaire. La courbe réelle d'un lithium ne l'est pas, mais
  // afficher un pourcentage au demi-point près sur un écran rafraîchi toutes
  // les dix minutes n'aurait aucun sens.
  return (millivolts - kEmptyMv) * 100 / (kFullMv - kEmptyMv);
}

bool isCharging(int millivolts) {
  // Au-delà de la tension de fin de charge, c'est l'USB qui alimente : aucune
  // broche du reTerminal ne signale la charge autrement.
  return millivolts > kFullMv + 30;
}

}  // namespace battery
