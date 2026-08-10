#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "core/dither.h"
#include "core/layout_plan.h"
#include "core/version.h"

namespace display {
namespace {

EPaper epaper;
uint32_t g_refresh_count = 0;

constexpr int16_t kMargin = 40;
constexpr int16_t kWidth = 800;
constexpr int16_t kHeight = 480;
constexpr int16_t kFooterY = 420;

// Les polices numérotées de Seeed_GFX ne sont pas des polices générales : la 6
// ne contient que `1234567890:-.apm` (c'est une police d'horloge). Les
// caractères absents sont supprimés sans avertissement — un titre y perd la
// moitié de ses lettres. On n'utilise donc que les FreeFonts.

// Correspondance unique entre style de titre et rendu. Les FreeFonts plafonnent
// à 24 pt : les grands corps passent par un facteur d'échelle entier.
void applyTitleStyle(layout::TitleStyle style) {
  switch (style) {
    case layout::TitleStyle::kHuge:
      epaper.setFreeFont(&FreeSansBold24pt7b);
      epaper.setTextSize(2);
      return;
    case layout::TitleStyle::kLarge:
      epaper.setFreeFont(&FreeSansBold18pt7b);
      epaper.setTextSize(2);
      return;
    case layout::TitleStyle::kMedium:
      epaper.setFreeFont(&FreeSansBold24pt7b);
      epaper.setTextSize(1);
      return;
  }
}

int titleLineHeight(layout::TitleStyle style) {
  switch (style) {
    case layout::TitleStyle::kHuge: return 76;
    case layout::TitleStyle::kLarge: return 58;
    case layout::TitleStyle::kMedium: return 42;
  }
  return 42;
}

// La mesure passe par la même bascule que le rendu : le texte planifié et le
// texte dessiné ne peuvent donc pas diverger.
}  // namespace

int measureText(const std::string& text, layout::TitleStyle style) {
  applyTitleStyle(style);
  return epaper.textWidth(text.c_str());
}

namespace {

void commit() {
  const uint32_t started = millis();
  epaper.update();
  ++g_refresh_count;
  Serial.printf("[ecran] rafraichissement #%lu en %lu ms\n",
                static_cast<unsigned long>(g_refresh_count), millis() - started);
}

uint16_t inkToColor(dither::Ink ink) {
  switch (ink) {
    case dither::Ink::kBlack: return TFT_BLACK;
    case dither::Ink::kWhite: return TFT_WHITE;
    case dither::Ink::kRed: return TFT_RED;
    case dither::Ink::kGreen: return TFT_GREEN;
    case dither::Ink::kBlue: return TFT_BLUE;
    case dither::Ink::kYellow: return TFT_YELLOW;
  }
  return TFT_WHITE;
}

void drawArt(const albumart::Bitmap& art, int16_t x, int16_t y, int16_t size) {
  // Le tramage a été calculé pour cette taille exacte : on pose les pixels tels
  // quels. Redimensionner ici détruirait la trame.
  (void)size;
  for (int row = 0; row < art.size; ++row) {
    for (int col = 0; col < art.size; ++col) {
      epaper.drawPixel(x + col, y + row, inkToColor(art.pixels[row * art.size + col]));
    }
  }
}

// Repli quand la pochette manque : entrée TV, radio sans image, téléchargement
// échoué. Un cadre barré vaut mieux qu'un aplat — on voit que la place est
// prévue et que l'image manque, plutôt qu'un trou inexpliqué.
void drawArtPlaceholder(int16_t x, int16_t y, int16_t size) {
  epaper.drawRect(x, y, size, size, TFT_BLACK);
  epaper.drawLine(x, y, x + size, y + size, TFT_BLACK);
  epaper.drawLine(x + size, y, x, y + size, TFT_BLACK);
}

// --- Pictogrammes météo ------------------------------------------------------
//
// Dessinés au trait plutôt que tirés d'une bibliothèque d'icônes : les
// FreeFonts n'ont pas de symboles météo, et une image bitmap devrait exister en
// deux tailles, être tramée, et occuper de la flash pour rien. Des primitives
// géométriques se redimensionnent d'elles-mêmes et restent nettes.
//
// Uniquement du noir et du blanc. La première version employait les six encres
// — soleil jaune, nuage bleu — mais sur le panneau les contours fins
// ressortaient *bruns* et non noirs : le Spectra 6 rend une ligne de quelques
// pixels en la composant avec ses pigments, et la couleur pure ne survit qu'aux
// aplats larges. Le noir et blanc, lui, est franc à toute échelle.

// Épaisseur du contour, proportionnelle à la taille du pictogramme mais jamais
// inférieure à 3 px : en dessous, le trait grisaille au lieu d'être noir.
int16_t strokeWidth(int16_t r) {
  const int16_t w = r * 13 / 100;
  return w < 3 ? 3 : w;
}

// Le contour s'obtient en dessinant la même forme, en noir, légèrement plus
// grande, puis la forme pleine par-dessus. Sur une silhouette composée de
// plusieurs disques — un nuage — c'est le seul moyen simple de cerner l'union
// sans laisser apparaître les traits intérieurs.
void cloudShape(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  epaper.fillCircle(cx - r * 45 / 100, cy, r * 42 / 100, color);
  epaper.fillCircle(cx + r * 30 / 100, cy, r * 36 / 100, color);
  epaper.fillCircle(cx - r * 5 / 100, cy - r * 28 / 100, r * 50 / 100, color);
  epaper.fillRect(cx - r * 45 / 100, cy, r * 75 / 100, r * 42 / 100, color);
}

void drawCloud(int16_t cx, int16_t cy, int16_t r) {
  // Le plus gros disque du nuage vaut la moitié de `r` : pour épaissir la
  // silhouette de `s` pixels, il faut donc agrandir `r` de deux fois `s`.
  const int16_t s = strokeWidth(r);
  cloudShape(cx, cy, r + 2 * s, TFT_BLACK);
  cloudShape(cx, cy, r, TFT_WHITE);

  // Deux barres dans le ventre du nuage lui donnent du volume, faute d'un gris
  // que le panneau n'a pas. Elles sont placées par construction bien à
  // l'intérieur de la silhouette : impossible qu'elles débordent, contrairement
  // à un hachurage qu'il faudrait détourer.
  //
  // Réservées au grand format : sur les vignettes des créneaux horaires, elles
  // remplissaient le nuage au lieu de le nuancer.
  if (r < 30) return;
  for (int i = 0; i < 2; ++i) {
    const int16_t y = cy + r * (12 + 20 * i) / 100;
    const int16_t half = r * (i == 0 ? 40 : 28) / 100;
    epaper.fillRect(cx - half, y, half * 2, s / 2 + 1, TFT_BLACK);
  }
}

// Segment épais : la bibliothèque ne trace que des lignes d'un pixel.
void thickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t width,
               uint16_t color) {
  for (int16_t i = -width / 2; i <= width / 2; ++i) {
    // Décalage perpendiculaire approximé sur les deux axes : suffisant pour des
    // traits courts, et sans trigonométrie.
    epaper.drawLine(x0 + i, y0, x1 + i, y1, color);
    epaper.drawLine(x0, y0 + i, x1, y1 + i, color);
  }
}

void drawSun(int16_t cx, int16_t cy, int16_t r, bool with_rays) {
  const int16_t s = strokeWidth(r);

  if (with_rays) {
    // Huit rayons, en croix puis en diagonale. Les diagonales sont raccourcies
    // d'un facteur ~0,7 pour que les pointes restent sur un même cercle.
    const int16_t in = r * 72 / 100;
    const int16_t out = r;
    const int16_t din = in * 70 / 100;
    const int16_t dout = out * 70 / 100;

    thickLine(cx, cy - in, cx, cy - out, s, TFT_BLACK);
    thickLine(cx, cy + in, cx, cy + out, s, TFT_BLACK);
    thickLine(cx - in, cy, cx - out, cy, s, TFT_BLACK);
    thickLine(cx + in, cy, cx + out, cy, s, TFT_BLACK);
    thickLine(cx - din, cy - din, cx - dout, cy - dout, s, TFT_BLACK);
    thickLine(cx + din, cy - din, cx + dout, cy - dout, s, TFT_BLACK);
    thickLine(cx - din, cy + din, cx - dout, cy + dout, s, TFT_BLACK);
    thickLine(cx + din, cy + din, cx + dout, cy + dout, s, TFT_BLACK);
  }

  epaper.fillCircle(cx, cy, r * 55 / 100 + s, TFT_BLACK);
  epaper.fillCircle(cx, cy, r * 55 / 100, TFT_WHITE);
}

// Gouttes, flocons ou grêlons sous un nuage. `hollow` évide la forme pour
// distinguer la neige de la grêle sans recourir à une couleur.
void drawFallout(int16_t cx, int16_t cy, int16_t r, int count, bool round,
                 bool hollow = false) {
  const int16_t s = strokeWidth(r);
  const int16_t spacing = r * 30 / 100;
  const int16_t start = cx - spacing * (count - 1) / 2;

  for (int i = 0; i < count; ++i) {
    const int16_t x = start + spacing * i;
    if (round) {
      epaper.fillCircle(x, cy + r * 12 / 100, r * 13 / 100 + s / 2, TFT_BLACK);
      if (hollow) epaper.fillCircle(x, cy + r * 12 / 100, r * 13 / 100 - s / 2, TFT_WHITE);
    } else {
      // Trait plein et court : allongé, il ressemblait à un barreau d'échelle
      // plutôt qu'à de la pluie.
      thickLine(x, cy, x - r * 7 / 100, cy + r * 22 / 100, s, TFT_BLACK);
    }
  }
}

// `r` est le rayon utile du pictogramme : tout tient dans un carré de 2r.
void drawConditionIcon(int16_t cx, int16_t cy, int16_t r,
                       weather::Condition condition) {
  switch (condition) {
    case weather::Condition::kSunny:
      drawSun(cx, cy, r, true);
      return;

    case weather::Condition::kClearNight: {
      // Croissant plein : un disque noir que mord un disque blanc. Cerner la
      // morsure dessinait un cercle noir complet à droite de la lune — on y
      // voyait une pleine lune posée à côté du croissant, pas une échancrure.
      const int16_t s = strokeWidth(r);
      epaper.fillCircle(cx, cy, r * 70 / 100 + s, TFT_BLACK);
      epaper.fillCircle(cx + r * 38 / 100, cy - r * 26 / 100, r * 66 / 100, TFT_WHITE);
      return;
    }

    case weather::Condition::kPartlyCloudy:
      drawSun(cx + r * 32 / 100, cy - r * 38 / 100, r * 58 / 100, true);
      drawCloud(cx - r * 12 / 100, cy + r * 28 / 100, r * 85 / 100);
      return;

    case weather::Condition::kCloudy:
      drawCloud(cx, cy, r);
      return;

    case weather::Condition::kFog:
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      for (int i = 0; i < 3; ++i) {
        const int16_t y = cy + r * (45 + 25 * i) / 100;
        const int16_t half = r * (i == 1 ? 40 : 55) / 100;
        thickLine(cx - half, y, cx + half, y, strokeWidth(r), TFT_BLACK);
      }
      return;

    case weather::Condition::kRainy:
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      drawFallout(cx, cy + r * 45 / 100, r, 3, false);
      return;

    case weather::Condition::kPouring:
      // Deux rangées décalées plutôt qu'une longue traînée : au-delà de quatre
      // gouttes de front, l'icône ne tient plus dans son carré.
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      drawFallout(cx, cy + r * 38 / 100, r, 4, false);
      drawFallout(cx, cy + r * 62 / 100, r, 3, false);
      return;

    case weather::Condition::kLightning: {
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      // Éclair plein, détouré d'un liseré blanc pour qu'il se détache du nuage
      // au lieu de s'y fondre.
      const int16_t s = strokeWidth(r);
      const int16_t top = cy + r * 25 / 100;
      const int16_t mid = cy + r * 55 / 100;
      const int16_t bottom = cy + r * 95 / 100;

      for (int pass = 0; pass < 2; ++pass) {
        const uint16_t color = pass == 0 ? TFT_WHITE : TFT_BLACK;
        const int16_t width = pass == 0 ? s * 2 : s;
        thickLine(cx + r * 15 / 100, top, cx - r * 15 / 100, mid, width, color);
        thickLine(cx - r * 15 / 100, mid, cx + r * 12 / 100, mid, width, color);
        thickLine(cx + r * 12 / 100, mid, cx - r * 20 / 100, bottom, width, color);
      }
      return;
    }

    case weather::Condition::kSnowy:
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      // Flocons évidés, grêlons pleins : sans ce contraste les deux conditions
      // donnaient exactement le même dessin.
      drawFallout(cx, cy + r * 45 / 100, r, 3, true, true);
      return;

    case weather::Condition::kHail:
      drawCloud(cx, cy - r * 25 / 100, r * 85 / 100);
      drawFallout(cx, cy + r * 45 / 100, r, 3, true);
      return;

    case weather::Condition::kWindy: {
      const int16_t s = strokeWidth(r);
      for (int i = 0; i < 3; ++i) {
        const int16_t y = cy + r * (35 * i - 35) / 100;
        const int16_t length = r * (i == 1 ? 85 : 60) / 100;
        thickLine(cx - length, y, cx + length, y, s, TFT_BLACK);
        // Un crochet discret : plus marqué, les trois traits se lisaient comme
        // des flèches.
        thickLine(cx + length, y, cx + length - r * 10 / 100, y - r * 12 / 100, s,
                  TFT_BLACK);
      }
      return;
    }

    case weather::Condition::kExceptional: {
      const int16_t s = strokeWidth(r);
      epaper.fillCircle(cx, cy, r * 70 / 100, TFT_BLACK);
      // Point d'exclamation évidé : il tranche sur le disque plein.
      epaper.fillRect(cx - s / 2 - 1, cy - r * 42 / 100, s + 2, r * 52 / 100, TFT_WHITE);
      epaper.fillCircle(cx, cy + r * 42 / 100, s / 2 + 1, TFT_WHITE);
      return;
    }

    case weather::Condition::kUnknown: {
      // Un cercle vide plutôt qu'un symbole inventé : la condition est inconnue,
      // l'écran le dit sans prétendre autre chose.
      const int16_t s = strokeWidth(r);
      epaper.fillCircle(cx, cy, r * 60 / 100, TFT_BLACK);
      epaper.fillCircle(cx, cy, r * 60 / 100 - s, TFT_WHITE);
      return;
    }
  }
}

// `left` est déjà composé par l'appelant : l'écran météo n'a ni symbole de
// lecture ni zone à annoncer.
void drawFooter(const Status& status, int duration_s, const std::string& left) {
  epaper.drawLine(kMargin, kFooterY, kWidth - kMargin, kFooterY, TFT_BLACK);

  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.setTextSize(1);
  epaper.setTextColor(TFT_BLACK);

  // Les trois blocs sont posés avec le même repère vertical. Les variantes
  // drawCentreString / drawRightString gèrent leur origine autrement que
  // drawString : les mélanger décalait les lignes de base les unes par rapport
  // aux autres, et le texte chevauchait le trait de séparation.
  const int16_t baseline = kFooterY + 16;

  epaper.setTextDatum(TL_DATUM);
  epaper.drawString(left.c_str(), kMargin, baseline);

  // Rien plutôt qu'un zéro inventé : les capteurs arrivent à la livraison 9, et
  // un « 0.0 C » se lit comme une mesure, pas comme une absence de mesure.
  if (status.indoor_humidity_pct > 0) {
    char middle[48];
    snprintf(middle, sizeof(middle), "%.1f C   %d%%", status.indoor_temperature_c,
             status.indoor_humidity_pct);
    epaper.setTextDatum(TC_DATUM);
    epaper.drawString(middle, kWidth / 2, baseline);
  }

  // La durée seule, sans barre de progression : celle-ci serait figée à
  // l'instant du rendu et n'avancerait plus pendant tout le morceau. Sur
  // l'écran météo il n'y a pas de morceau : la place reste à la batterie.
  std::string right = duration_s > 0 ? layout::formatDuration(duration_s) : "";
  if (status.battery_pct >= 0) {
    if (!right.empty()) right += "   ";
    right += std::to_string(status.battery_pct) + "%";
  }
  epaper.setTextDatum(TR_DATUM);
  epaper.drawString(right.c_str(), kWidth - kMargin, baseline);

  epaper.setTextDatum(TL_DATUM);
}

}  // namespace

void begin() {
  epaper.begin();
  Serial.printf("[ecran] %dx%d, 6 couleurs\n", epaper.width(), epaper.height());
}

void showBootScreen(const char* status) {
  epaper.fillScreen(TFT_WHITE);
  epaper.drawRect(kMargin, kMargin, kWidth - 2 * kMargin, kHeight - 2 * kMargin,
                  TFT_BLACK);

  epaper.setTextSize(1);

  epaper.setTextColor(TFT_BLACK);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString("ePaper Spotify", kMargin + 30, kMargin + 40);

  epaper.setTextColor(TFT_BLUE);
  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.drawString("Sonos sur reTerminal E1002", kMargin + 30, kMargin + 110);

  epaper.setTextColor(TFT_BLACK);
  epaper.drawString(status, kMargin + 30, kHeight / 2 + 20);

  epaper.setTextColor(TFT_RED);
  epaper.drawString(epaper_spotify::kFirmwareVersion, kMargin + 30,
                    kHeight - kMargin - 60);

  commit();
}

void showTrack(const sonos::TrackInfo& track, const Status& status,
               const albumart::Bitmap& art) {
  const layout::TrackPlan plan =
      layout::planTrack(track.title, track.artist, track.album, measureText);

  if (plan.truncated) {
    Serial.printf("[ecran] titre tronque : %s\n", track.title.c_str());
  }

  epaper.fillScreen(TFT_WHITE);
  epaper.setTextSize(1);
  epaper.setTextDatum(TL_DATUM);

  // Hauteurs des blocs sous le titre, mesurées sur le rendu réel.
  constexpr int16_t kArtistGap = 18;
  constexpr int16_t kArtistHeight = 52;
  constexpr int16_t kAlbumHeight = 34;

  const int16_t block_height =
      static_cast<int16_t>(plan.title_lines.size()) * titleLineHeight(plan.title_style) +
      kArtistGap + kArtistHeight + kAlbumHeight;

  // Bloc centré verticalement dans l'espace disponible au-dessus du bandeau.
  // Aligné en haut, un titre d'une ou deux lignes laissait un tiers d'écran
  // vide sous l'album.
  int16_t text_x = kMargin;
  int16_t text_y = (kFooterY - block_height) / 2;
  if (text_y < kMargin + 20) text_y = kMargin + 20;

  const int16_t art_x = plan.variant == layout::Variant::kTypography
                            ? kWidth - kMargin - plan.art_size_px
                            : kMargin;
  if (plan.variant == layout::Variant::kArtwork) {
    text_x = kMargin + plan.art_size_px + 30;
  }

  if (art.valid() && art.size == plan.art_size_px) {
    drawArt(art, art_x, kMargin, plan.art_size_px);
  } else {
    drawArtPlaceholder(art_x, kMargin, plan.art_size_px);
  }

  epaper.setTextColor(TFT_BLACK);
  applyTitleStyle(plan.title_style);
  for (const std::string& line : plan.title_lines) {
    epaper.drawString(line.c_str(), text_x, text_y);
    text_y += titleLineHeight(plan.title_style);
  }

  text_y += kArtistGap;
  epaper.setTextSize(1);
  epaper.setTextColor(TFT_BLUE);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString(plan.artist.c_str(), text_x, text_y);

  text_y += kArtistHeight;
  epaper.setTextColor(TFT_BLACK);
  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.drawString(plan.album.c_str(), text_x, text_y);

  drawFooter(status, track.duration_s,
             std::string(status.playing ? "> " : "|| ") + status.zone);
  commit();
}

void showWeather(const weather::View& view, const Status& status) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextSize(1);
  epaper.setTextDatum(TL_DATUM);

  constexpr int16_t kHeadlineY = 60;
  constexpr int16_t kDetailsY = 175;
  constexpr int16_t kIndoorY = 220;
  constexpr int16_t kColumnsY = 290;

  // La température domine : c'est la seule information qu'on lit de loin.
  epaper.setTextColor(TFT_BLACK);
  applyTitleStyle(layout::TitleStyle::kHuge);
  epaper.drawString(view.temperature.c_str(), kMargin, kHeadlineY);

  // Le pictogramme occupe le coin haut droit, le libellé passe dessous. Le
  // libellé reste aligné à droite plutôt que centré sous l'icône : « Peu
  // nuageux » déborderait de la marge.
  if (!view.stale) {
    drawConditionIcon(kWidth - kMargin - 60, kHeadlineY + 50, 55, view.condition);
  }

  epaper.setTextSize(1);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.setTextColor(view.stale ? TFT_RED : TFT_BLUE);
  epaper.setTextDatum(TR_DATUM);
  epaper.drawString(view.condition_label.c_str(), kWidth - kMargin,
                    view.stale ? kHeadlineY + 20 : kHeadlineY + 108);
  epaper.setTextDatum(TL_DATUM);

  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.setTextColor(TFT_BLACK);
  if (!view.details.empty()) epaper.drawString(view.details.c_str(), kMargin, kDetailsY);

  // L'intérieur est affiché même sans météo : il vient du SHT4x, pas du réseau.
  if (!view.indoor.empty()) {
    epaper.setTextColor(TFT_GREEN);
    epaper.drawString(view.indoor.c_str(), kMargin, kIndoorY);
    epaper.setTextColor(TFT_BLACK);
  }

  if (!view.columns.empty()) {
    epaper.drawLine(kMargin, kColumnsY - 20, kWidth - kMargin, kColumnsY - 20, TFT_BLACK);

    const int16_t usable = kWidth - 2 * kMargin;
    const int16_t step = usable / static_cast<int16_t>(view.columns.size());
    epaper.setTextDatum(TC_DATUM);

    for (size_t i = 0; i < view.columns.size(); ++i) {
      const weather::Column& column = view.columns[i];
      const int16_t centre = kMargin + step * static_cast<int16_t>(i) + step / 2;

      epaper.setFreeFont(&FreeSans18pt7b);
      epaper.setTextColor(TFT_BLACK);
      epaper.drawString(column.hour.c_str(), centre, kColumnsY);

      drawConditionIcon(centre, kColumnsY + 44, 20, column.condition);

      epaper.setFreeFont(&FreeSansBold18pt7b);
      epaper.drawString(column.temperature.c_str(), centre, kColumnsY + 74);

      if (!column.precipitation.empty()) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.setTextColor(TFT_BLUE);
        epaper.drawString(column.precipitation.c_str(), centre, kColumnsY + 112);
        epaper.setTextColor(TFT_BLACK);
      }
    }
    epaper.setTextDatum(TL_DATUM);
  }

  // L'intérieur figure déjà dans le corps de l'écran : le répéter au bandeau
  // ferait doublon. Pas de durée non plus — il reste la batterie.
  Status footer = status;
  footer.indoor_humidity_pct = 0;
  drawFooter(footer, 0, "Rien ne joue");
  commit();
}

uint32_t refreshCount() {
  return g_refresh_count;
}

}  // namespace display
