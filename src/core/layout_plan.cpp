#include "core/layout_plan.h"

#include <cstdio>

namespace layout {
namespace {

// Largeurs utiles, en pixels, écran de 800x480 et marge de 40.
//
// En disposition C la pochette occupe le coin haut droit : le titre ne dispose
// donc que de la colonne de gauche sur toute sa hauteur.
constexpr int kTypographyTitleWidth = 500;
constexpr int kArtworkTitleWidth = 330;

// L'échelle des replis. On réduit le corps et on ajoute des lignes avant de
// changer de disposition ; on ne tronque qu'en tout dernier recours.
struct Step {
  Variant variant;
  TitleStyle style;
  int max_lines;
  int width_px;
  int art_size_px;
};

constexpr Step kLadder[] = {
    {Variant::kTypography, TitleStyle::kHuge, 2, kTypographyTitleWidth, 200},
    {Variant::kTypography, TitleStyle::kLarge, 3, kTypographyTitleWidth, 200},
    {Variant::kArtwork, TitleStyle::kMedium, 4, kArtworkTitleWidth, 360},
};

}  // namespace

bool wrapText(const std::string& text, int max_width_px, int max_lines, TitleStyle style,
              const Measure& measure, std::vector<std::string>& out) {
  out.clear();
  if (text.empty()) return true;

  std::string line;
  size_t index = 0;

  while (index < text.size()) {
    // Mot suivant, séparateur compris.
    size_t space = text.find(' ', index);
    if (space == std::string::npos) space = text.size();
    const std::string word = text.substr(index, space - index);
    index = space < text.size() ? space + 1 : text.size();

    if (word.empty()) continue;

    const std::string candidate = line.empty() ? word : line + " " + word;
    if (measure(candidate, style) <= max_width_px) {
      line = candidate;
      continue;
    }

    if (!line.empty()) {
      out.push_back(line);
      line.clear();
      if (static_cast<int>(out.size()) >= max_lines) return false;
    }

    // Un mot seul plus large que la ligne : on le coupe en dur, sans quoi il
    // déborderait de l'écran. Cas réel avec certains titres sans espaces.
    if (measure(word, style) > max_width_px) {
      std::string chunk;
      for (const char c : word) {
        const std::string extended = chunk + c;
        if (measure(extended, style) > max_width_px && !chunk.empty()) {
          out.push_back(chunk);
          chunk.clear();
          if (static_cast<int>(out.size()) >= max_lines) return false;
        }
        chunk += c;
      }
      line = chunk;
    } else {
      line = word;
    }
  }

  if (!line.empty()) out.push_back(line);
  return static_cast<int>(out.size()) <= max_lines;
}

TrackPlan planTrack(const std::string& title, const std::string& artist,
                    const std::string& album, const Measure& measure) {
  TrackPlan plan;
  plan.artist = artist;
  plan.album = album;

  for (const Step& step : kLadder) {
    std::vector<std::string> lines;
    if (!wrapText(title, step.width_px, step.max_lines, step.style, measure, lines)) {
      continue;
    }
    plan.variant = step.variant;
    plan.title_style = step.style;
    plan.art_size_px = step.art_size_px;
    plan.title_lines = lines;
    return plan;
  }

  // Aucun palier ne suffit : on garde le dernier et on tronque. Mieux vaut un
  // titre coupé qu'un titre qui déborde de l'écran.
  const Step& last = kLadder[sizeof(kLadder) / sizeof(kLadder[0]) - 1];
  plan.variant = last.variant;
  plan.title_style = last.style;
  plan.art_size_px = last.art_size_px;
  plan.truncated = true;

  wrapText(title, last.width_px, last.max_lines + 8, last.style, measure,
           plan.title_lines);
  plan.title_lines.resize(last.max_lines);
  if (!plan.title_lines.empty()) plan.title_lines.back() += "...";

  return plan;
}

std::string formatDuration(int seconds) {
  if (seconds <= 0) return "--:--";

  char buffer[16];
  const int hours = seconds / 3600;
  if (hours > 0) {
    snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, (seconds % 3600) / 60,
             seconds % 60);
  } else {
    snprintf(buffer, sizeof(buffer), "%d:%02d", seconds / 60, seconds % 60);
  }
  return buffer;
}

}  // namespace layout
