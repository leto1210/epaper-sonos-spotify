#pragma once

#include <stdint.h>

#include <string>

#include "albumart.h"
#include "core/layout_plan.h"
#include "core/sonos_parser.h"
#include "core/weather_view.h"

// Pilotage de l'ePaper. Toute écriture à l'écran passe par ici : c'est le seul
// endroit qui déclenche un rafraîchissement, ce qui rend le comptage fiable.
//
// Un rafraîchissement complet prend 37 s et bloque. Voir docs/architecture.md.
namespace display {

// Ce qui figure dans le bandeau bas, commun à tous les écrans.
struct Status {
  std::string zone;
  float indoor_temperature_c = 0.0f;
  int indoor_humidity_pct = 0;
  int battery_pct = -1;  // négatif tant que la mesure n'est pas implémentée
  bool playing = false;
};

void begin();

// Largeur d'un texte dans le style donné, telle que l'écran la rendra. Exposée
// parce que la taille de la pochette dépend de la disposition, elle-même
// choisie d'après la place que prend le titre.
int measureText(const std::string& text, layout::TitleStyle style);

void showBootScreen(const char* status);

// Écran du morceau en cours. La disposition est choisie par core/layout_plan
// d'après la longueur du titre.
void showTrack(const sonos::TrackInfo& track, const Status& status,
               const albumart::Bitmap& art = {});

// Écran de repli quand rien ne joue : météo reçue de Home Assistant par MQTT.
// Le contenu est entièrement mis en forme par core/weather_view.
void showWeather(const weather::View& view, const Status& status);

uint32_t refreshCount();

}  // namespace display
