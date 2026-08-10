#include "core/weather_view.h"

#include <cmath>
#include <cstdio>

namespace weather {
namespace {

// Le degré est en UTF-8 ; l'écran affiche des FreeFonts qui n'en disposent pas.
// On écrit donc « 32 C », comme le bandeau du morceau en cours.
std::string degrees(float celsius) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d C", static_cast<int>(std::lround(celsius)));
  return buffer;
}

}  // namespace

View plan(const Report& report, long now_unix, bool has_indoor, float indoor_c,
          int indoor_humidity_pct) {
  View view;
  view.stale = !report.valid || isStale(report, now_unix);

  if (has_indoor && indoor_humidity_pct > 0) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Interieur %.1f C   %d %%", indoor_c,
             indoor_humidity_pct);
    view.indoor = buffer;
  }

  // Un rapport périmé garde ses chiffres masqués : les afficher grisés
  // reviendrait à les présenter comme des mesures, ce qu'ils ne sont plus.
  if (view.stale) {
    view.condition_label = report.valid ? "Meteo perimee" : "Meteo indisponible";
    return view;
  }

  view.condition = report.condition;
  view.condition_label = conditionLabel(report.condition);
  view.temperature = degrees(report.temperature_c);

  char details[80];
  snprintf(details, sizeof(details), "%d %%   %d km/h   UV %.1f", report.humidity_pct,
           report.wind_kmh, report.uv_index);
  view.details = details;

  for (const Hour& hour : report.hourly) {
    if (static_cast<int>(view.columns.size()) >= kMaxColumns) break;

    Column column;
    column.hour = hour.label + "h";
    column.temperature = degrees(hour.temperature_c);
    column.condition = hour.condition;

    // La pluie n'apparaît que s'il pleut : une colonne « 0 mm » à chaque
    // créneau d'une journée d'été n'apprend rien et charge l'écran.
    if (hour.precipitation_mm >= 0.1f) {
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "%.1f mm", hour.precipitation_mm);
      column.precipitation = buffer;
    }

    view.columns.push_back(column);
  }

  return view;
}

}  // namespace weather
