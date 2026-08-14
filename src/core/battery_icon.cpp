#include "core/battery_icon.h"

namespace battery_icon {

Geometry plan(int battery_pct, int16_t body_width, int16_t stroke) {
  Geometry geometry;

  // `-1` signifie « pas de mesure » dans tout le firmware. Ne rien dessiner :
  // une pile vide se lirait comme une batterie à plat, c'est-à-dire comme une
  // mesure, alors qu'il n'y en a pas.
  if (battery_pct < 0) return geometry;

  geometry.visible = true;

  // Le contour mord des deux côtés, plus un pixel de jeu pour que l'aplat ne
  // vienne pas se coller au trait.
  geometry.inner_width = body_width - 2 * (stroke + 1);
  if (geometry.inner_width < 0) geometry.inner_width = 0;

  // Une valeur au-delà des bornes vient d'un capteur, pas d'un choix : on la
  // ramène dans la plage plutôt que de déborder du corps de la pile.
  int pct = battery_pct;
  if (pct > 100) pct = 100;

  geometry.fill_width = static_cast<int16_t>(geometry.inner_width * pct / 100);
  return geometry;
}

}  // namespace battery_icon
