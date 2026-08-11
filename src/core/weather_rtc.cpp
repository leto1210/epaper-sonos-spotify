#include "core/weather_rtc.h"

#include <cmath>
#include <cstring>

namespace weather {
namespace {

int16_t toDeciDegrees(float celsius) {
  return static_cast<int16_t>(std::lround(celsius * 10.0f));
}

void copyLabel(char* destination, size_t size, const std::string& source) {
  std::strncpy(destination, source.c_str(), size - 1);
  destination[size - 1] = '\0';
}

}  // namespace

RtcReport toRtc(const Report& report) {
  RtcReport stored;
  if (!report.valid) return stored;  // magic reste nul : rien à restaurer

  stored.magic = kRtcMagic;
  stored.condition = static_cast<uint8_t>(report.condition);
  stored.temperature_dc = toDeciDegrees(report.temperature_c);
  stored.humidity_pct = static_cast<uint8_t>(report.humidity_pct);
  stored.wind_kmh = static_cast<uint16_t>(report.wind_kmh);
  stored.uv_index_d = static_cast<uint8_t>(std::lround(report.uv_index * 10.0f));
  stored.published_at = static_cast<int32_t>(report.published_at);

  const size_t count =
      report.hourly.size() < 6 ? report.hourly.size() : 6;
  stored.hour_count = static_cast<uint8_t>(count);
  for (size_t i = 0; i < count; ++i) {
    const Hour& hour = report.hourly[i];
    copyLabel(stored.hours[i].label, sizeof(stored.hours[i].label), hour.label);
    stored.hours[i].condition = static_cast<uint8_t>(hour.condition);
    stored.hours[i].temperature_dc = toDeciDegrees(hour.temperature_c);
    stored.hours[i].precipitation_dmm =
        static_cast<uint16_t>(std::lround(hour.precipitation_mm * 10.0f));
  }

  return stored;
}

Report fromRtc(const RtcReport& stored) {
  Report report;
  if (stored.magic != kRtcMagic) return report;  // `valid` reste faux

  report.valid = true;
  report.condition = static_cast<Condition>(stored.condition);
  report.temperature_c = stored.temperature_dc / 10.0f;
  report.humidity_pct = stored.humidity_pct;
  report.wind_kmh = stored.wind_kmh;
  report.uv_index = stored.uv_index_d / 10.0f;
  report.published_at = stored.published_at;

  for (uint8_t i = 0; i < stored.hour_count && i < 6; ++i) {
    Hour hour;
    hour.label = stored.hours[i].label;
    hour.condition = static_cast<Condition>(stored.hours[i].condition);
    hour.temperature_c = stored.hours[i].temperature_dc / 10.0f;
    hour.precipitation_mm = stored.hours[i].precipitation_dmm / 10.0f;
    report.hourly.push_back(hour);
  }

  return report;
}

}  // namespace weather
