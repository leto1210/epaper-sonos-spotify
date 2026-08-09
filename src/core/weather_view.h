#pragma once

#include <string>
#include <vector>

#include "core/weather.h"

// Mise en forme de l'écran météo : toutes les chaînes affichées sont
// construites ici, sans écran ni Arduino, donc vérifiables par les tests.
// `display` ne fait plus que les poser.
namespace weather {

struct Column {
  std::string hour;           // "16h"
  std::string temperature;    // "33°"
  std::string precipitation;  // "2 mm", vide s'il ne pleut pas
  Condition condition = Condition::kUnknown;
};

struct View {
  // Vrai si le rapport est absent ou périmé. Le sujet MQTT étant retenu, un
  // broker resservirait sinon indéfiniment la dernière météo publiée.
  bool stale = true;

  std::string condition_label;  // "Ensoleillé"
  std::string temperature;      // "32°"
  std::string details;          // "23 %   10 km/h   UV 5.6"
  std::string indoor;           // "Intérieur 30.7°   36 %", vide sans capteur

  std::vector<Column> columns;
};

// Six créneaux au plus : au-delà, les colonnes deviennent illisibles à deux
// mètres, ce qui est la distance de lecture de cet écran.
constexpr int kMaxColumns = 6;

View plan(const Report& report, long now_unix, bool has_indoor = false,
          float indoor_c = 0.0f, int indoor_humidity_pct = 0);

}  // namespace weather
