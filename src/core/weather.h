#pragma once

#include <string>
#include <vector>

// Météo reçue de Home Assistant par MQTT (topic `<device>/weather`, retenu).
// Le boîtier n'appelle jamais Home Assistant : il écoute. Voir
// homeassistant/weather_to_mqtt.yaml et docs/home-assistant.md.
namespace weather {

// Vocabulaire des conditions de Home Assistant. Traduit en énumération pour que
// le choix du pictogramme ne dépende pas de comparaisons de chaînes dispersées
// dans le code d'affichage.
enum class Condition {
  kUnknown,
  kSunny,
  kClearNight,
  kPartlyCloudy,
  kCloudy,
  kFog,
  kRainy,
  kPouring,
  kLightning,
  kSnowy,
  kHail,
  kWindy,
  kExceptional,
};

struct Hour {
  std::string label;  // "16", heure locale
  Condition condition = Condition::kUnknown;
  float temperature_c = 0.0f;
  float precipitation_mm = 0.0f;
};

struct Report {
  bool valid = false;

  Condition condition = Condition::kUnknown;
  float temperature_c = 0.0f;
  int humidity_pct = 0;
  int wind_kmh = 0;
  float uv_index = 0.0f;

  std::vector<Hour> hourly;

  // Horodatage de publication, en secondes Unix. Sert à détecter une météo
  // périmée : le sujet est retenu par le broker, donc une automatisation
  // arrêtée depuis trois jours livrerait sans cela des valeurs anciennes avec
  // l'aplomb des valeurs fraîches.
  long published_at = 0;
};

// Renvoie un rapport avec `valid = false` si le payload est illisible.
Report parse(const std::string& json);

Condition conditionFromString(const std::string& name);

// Libellé court en français, pour l'écran.
const char* conditionLabel(Condition condition);

// Vrai si le rapport date de plus de `max_age_s`.
bool isStale(const Report& report, long now_unix, long max_age_s = 7200);

}  // namespace weather
