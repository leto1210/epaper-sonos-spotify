#include "core/weather.h"

#include <ArduinoJson.h>

namespace weather {
namespace {

// Le payload publié par l'automatisation tient largement dans cette taille :
// six créneaux horaires font environ 500 octets. La marge absorbe l'ajout
// éventuel d'un champ sans imposer d'allocation dynamique.
constexpr size_t kDocumentCapacity = 2048;

}  // namespace

Condition conditionFromString(const std::string& name) {
  if (name == "sunny") return Condition::kSunny;
  if (name == "clear-night") return Condition::kClearNight;
  if (name == "partlycloudy") return Condition::kPartlyCloudy;
  if (name == "cloudy") return Condition::kCloudy;
  if (name == "fog") return Condition::kFog;
  if (name == "rainy" || name == "snowy-rainy") return Condition::kRainy;
  if (name == "pouring") return Condition::kPouring;
  if (name == "lightning" || name == "lightning-rainy") return Condition::kLightning;
  if (name == "snowy") return Condition::kSnowy;
  if (name == "hail") return Condition::kHail;
  if (name == "windy" || name == "windy-variant") return Condition::kWindy;
  if (name == "exceptional") return Condition::kExceptional;
  return Condition::kUnknown;
}

const char* conditionLabel(Condition condition) {
  switch (condition) {
    case Condition::kSunny: return "Ensoleille";
    case Condition::kClearNight: return "Nuit claire";
    case Condition::kPartlyCloudy: return "Peu nuageux";
    case Condition::kCloudy: return "Nuageux";
    case Condition::kFog: return "Brouillard";
    case Condition::kRainy: return "Pluie";
    case Condition::kPouring: return "Fortes pluies";
    case Condition::kLightning: return "Orages";
    case Condition::kSnowy: return "Neige";
    case Condition::kHail: return "Grele";
    case Condition::kWindy: return "Vent";
    case Condition::kExceptional: return "Alerte";
    case Condition::kUnknown: break;
  }
  return "--";
}

Report parse(const std::string& json) {
  Report report;

  JsonDocument document;
  if (deserializeJson(document, json) != DeserializationError::Ok) return report;

  JsonObjectConst now = document["now"];
  if (now.isNull()) return report;

  report.published_at = document["ts"] | 0L;
  report.condition = conditionFromString(now["cond"] | "");
  report.temperature_c = now["temp"] | 0.0f;
  report.humidity_pct = now["hum"] | 0;
  report.wind_kmh = now["wind"] | 0;
  report.uv_index = now["uv"] | 0.0f;

  for (JsonObjectConst entry : document["hourly"].as<JsonArrayConst>()) {
    Hour hour;
    hour.label = entry["h"] | "";
    hour.condition = conditionFromString(entry["c"] | "");
    hour.temperature_c = entry["t"] | 0.0f;
    hour.precipitation_mm = entry["p"] | 0.0f;
    report.hourly.push_back(hour);
  }

  report.valid = true;
  return report;
}

bool isStale(const Report& report, long now_unix, long max_age_s) {
  if (!report.valid) return true;
  if (report.published_at <= 0) return true;
  return now_unix - report.published_at > max_age_s;
}

}  // namespace weather
