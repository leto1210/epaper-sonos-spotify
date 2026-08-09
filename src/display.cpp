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

// Une couleur par famille de temps. C'est le seul endroit du projet où les six
// encres servent à porter du sens plutôt qu'à reproduire une image.
uint16_t conditionColor(weather::Condition condition) {
  switch (condition) {
    case weather::Condition::kSunny: return TFT_YELLOW;
    case weather::Condition::kClearNight: return TFT_BLUE;
    case weather::Condition::kPartlyCloudy: return TFT_YELLOW;
    case weather::Condition::kRainy:
    case weather::Condition::kPouring: return TFT_BLUE;
    case weather::Condition::kLightning: return TFT_RED;
    case weather::Condition::kSnowy:
    case weather::Condition::kHail: return TFT_BLUE;
    case weather::Condition::kWindy: return TFT_GREEN;
    case weather::Condition::kExceptional: return TFT_RED;
    case weather::Condition::kCloudy:
    case weather::Condition::kFog:
    case weather::Condition::kUnknown: break;
  }
  return TFT_BLACK;
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

  epaper.setTextSize(1);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.setTextColor(view.stale ? TFT_RED : TFT_BLUE);
  epaper.setTextDatum(TR_DATUM);
  epaper.drawString(view.condition_label.c_str(), kWidth - kMargin, kHeadlineY + 20);
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

      // Une pastille de couleur tient lieu de pictogramme : les FreeFonts n'ont
      // pas de symboles météo, et six encres suffisent à distinguer le soleil
      // de la pluie d'un coup d'œil.
      epaper.fillCircle(centre, kColumnsY + 48, 14, conditionColor(column.condition));
      epaper.drawCircle(centre, kColumnsY + 48, 14, TFT_BLACK);

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
