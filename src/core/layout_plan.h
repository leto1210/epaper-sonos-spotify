#pragma once

#include <functional>
#include <string>
#include <vector>

// Choix de la mise en page du morceau. Logique pure : la mesure du texte est
// injectée, ce qui permet de tester la politique sans écran.
//
// Deux dispositions, décidées d'après la longueur du titre :
//
//   kTypography (C) — titre 56 px, pochette réduite à 200 px en haut à droite.
//     Lisible d'un bout à l'autre d'une pièce. C'est le mode nominal.
//   kArtwork (A)    — pochette 360 px à gauche, titre 34 px sur quatre lignes.
//     Repli pour les titres longs : on préfère réduire le texte et agrandir
//     l'image plutôt que tronquer.
//
// Voir docs/architecture.md.
namespace layout {

enum class Variant {
  kTypography,
  kArtwork,
};

// Styles de titre, du plus grand au plus petit.
//
// Nommés plutôt que chiffrés : les FreeFonts d'Adafruit plafonnent à 24 pt, et
// les grands corps s'obtiennent par un facteur d'échelle entier. Un « 56 px »
// dans le code n'aurait correspondu à aucun rendu réel, et le plan aurait pu
// diverger de l'affichage sans qu'on s'en aperçoive. La correspondance exacte
// vit dans display.cpp, en un seul endroit.
enum class TitleStyle {
  kHuge,    // 24 pt x2, environ 66 px
  kLarge,   // 18 pt x2, environ 50 px
  kMedium,  // 24 pt x1, environ 33 px
};

// Largeur en pixels d'une chaîne, pour un style donné. Fournie par l'appelant :
// TFT_eSPI sur la cible, une approximation dans les tests. C'est la même
// fonction qui sert à décider et à dessiner — sans quoi le texte planifié et le
// texte rendu pourraient ne pas coïncider.
using Measure = std::function<int(const std::string&, TitleStyle style)>;

struct TrackPlan {
  Variant variant = Variant::kTypography;
  std::vector<std::string> title_lines;
  std::string artist;
  std::string album;

  TitleStyle title_style = TitleStyle::kHuge;
  int art_size_px = 200;

  // Vrai si le titre a dû être tronqué malgré tous les replis. Signale un cas
  // limite : utile pour le journal, pas affiché.
  bool truncated = false;
};

TrackPlan planTrack(const std::string& title, const std::string& artist,
                    const std::string& album, const Measure& measure);

// Découpe `text` en au plus `max_lines` lignes tenant dans `max_width_px`.
// La coupure se fait aux espaces ; un mot plus large que la ligne est coupé
// en dur plutôt que de déborder. Renvoie faux si le texte ne tient pas.
bool wrapText(const std::string& text, int max_width_px, int max_lines, TitleStyle style,
              const Measure& measure, std::vector<std::string>& out);

// "2:06" à partir de secondes. Négatif ou nul donne "--:--".
std::string formatDuration(int seconds);

}  // namespace layout
