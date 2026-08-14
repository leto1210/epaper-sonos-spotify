#pragma once

#include <cstdint>

// Géométrie du pictogramme de batterie du bandeau.
//
// Le calcul est ici, séparé du tracé, pour être vérifiable sans matériel : un
// rafraîchissement coûte 37 s, et une erreur d'un pixel sur la jauge ne se voit
// qu'à l'écran. Le tracé proprement dit reste dans `display.cpp`.
//
// Le pictogramme est monochrome et à contour épais, comme les pictogrammes
// météo : le panneau Spectra 6 rend un trait fin *brun* et non noir, la couleur
// pure ne survivant qu'aux aplats larges. Voir docs/hardware.md.
namespace battery_icon {

struct Geometry {
  // Faux quand il n'y a rien à dessiner : la mesure est absente. Mieux vaut
  // aucune icône qu'une pile vide, qui se lirait comme une batterie à plat.
  bool visible = false;

  // Largeur de l'aplat de jauge, en pixels, à l'intérieur du corps.
  int16_t fill_width = 0;

  // Largeur intérieure disponible, une fois le contour retranché.
  int16_t inner_width = 0;
};

// `body_width` est la largeur hors tout du corps, borne positive exclue.
// `stroke` est l'épaisseur du contour.
Geometry plan(int battery_pct, int16_t body_width, int16_t stroke);

}  // namespace battery_icon
