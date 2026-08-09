#include "sensors.h"

#include <Adafruit_SHT4x.h>
#include <Arduino.h>
#include <Wire.h>

#include "core/battery.h"

namespace sensors {
namespace {

// Brochage du reTerminal E1002, voir docs/hardware.md.
constexpr int kI2cSda = 19;
constexpr int kI2cScl = 20;
constexpr int kBatteryAdc = 1;
constexpr int kBatteryEnable = 21;

// Le pont diviseur ramène la tension d'accu dans la plage de l'ADC.
constexpr int kDividerRatio = 2;

Adafruit_SHT4x g_sht4x;
bool g_climate_ready = false;

int readBatteryMillivolts() {
  digitalWrite(kBatteryEnable, HIGH);
  // Le wiki Seeed insiste sur ce délai : sans lui la première conversion se
  // fait avant que le pont diviseur soit établi, et la valeur est fausse.
  delay(10);

  // Quatre mesures moyennées : l'ADC de l'ESP32-S3 est bruyant, et un écart
  // de quelques dizaines de millivolts déplace le pourcentage affiché.
  long total = 0;
  for (int i = 0; i < 4; ++i) total += analogReadMilliVolts(kBatteryAdc);

  digitalWrite(kBatteryEnable, LOW);
  return static_cast<int>(total / 4) * kDividerRatio;
}

}  // namespace

bool begin() {
  pinMode(kBatteryEnable, OUTPUT);
  digitalWrite(kBatteryEnable, LOW);

  Wire.begin(kI2cSda, kI2cScl);
  g_climate_ready = g_sht4x.begin(&Wire);
  if (g_climate_ready) {
    g_sht4x.setPrecision(SHT4X_HIGH_PRECISION);
    g_sht4x.setHeater(SHT4X_NO_HEATER);
    Serial.println("[capteurs] SHT4x pret");
  } else {
    Serial.println("[capteurs] SHT4x absent");
  }
  return g_climate_ready;
}

Reading read() {
  Reading reading;

  if (g_climate_ready) {
    sensors_event_t humidity, temperature;
    if (g_sht4x.getEvent(&humidity, &temperature)) {
      reading.temperature_c = temperature.temperature;
      reading.humidity_pct = static_cast<int>(humidity.relative_humidity + 0.5f);
      reading.has_climate = true;
    }
  }

  reading.battery_mv = readBatteryMillivolts();
  reading.battery_pct = battery::percentFromMillivolts(reading.battery_mv);
  reading.charging = battery::isCharging(reading.battery_mv);

  return reading;
}

}  // namespace sensors
