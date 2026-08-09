#pragma once

// Capteurs embarqués du reTerminal E1002 : SHT4x sur I2C, tension d'accu sur
// l'ADC. Voir docs/hardware.md pour le brochage.
namespace sensors {

struct Reading {
  bool has_climate = false;
  float temperature_c = 0.0f;
  int humidity_pct = 0;

  int battery_mv = 0;
  int battery_pct = -1;  // négatif si la mesure a échoué
  bool charging = false;
};

// Renvoie faux si le SHT4x ne répond pas. Le reste des mesures reste possible.
bool begin();

Reading read();

}  // namespace sensors
