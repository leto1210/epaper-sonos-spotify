// Tests de la politique de mise en page. Aucun écran requis : la mesure du
// texte est injectée.
#include <unity.h>

#include <string>
#include <vector>

#include "core/layout_plan.h"

namespace {

// Approximation d'une FreeSans : environ 0,55 em par caractère. Suffisant pour
// éprouver la politique de bascule, qui est ce qu'on teste ici — la mesure
// exacte vient de TFT_eSPI sur la cible.
int pixelsFor(layout::TitleStyle style) {
  switch (style) {
    case layout::TitleStyle::kHuge: return 66;
    case layout::TitleStyle::kLarge: return 50;
    case layout::TitleStyle::kMedium: return 33;
  }
  return 33;
}

int fakeMeasure(const std::string& text, layout::TitleStyle style) {
  return static_cast<int>(text.size() * pixelsFor(style) * 0.55);
}

}  // namespace

// Un titre court reste en grande typographie : c'est le mode nominal.
void test_short_title_uses_large_typography() {
  const layout::TrackPlan plan =
      layout::planTrack("Brain Stew", "Green Day", "Insomniac", fakeMeasure);

  TEST_ASSERT_EQUAL(layout::Variant::kTypography, plan.variant);
  TEST_ASSERT_EQUAL(layout::TitleStyle::kHuge, plan.title_style);
  TEST_ASSERT_EQUAL_UINT(1, plan.title_lines.size());
  TEST_ASSERT_EQUAL_STRING("Brain Stew", plan.title_lines[0].c_str());
  TEST_ASSERT_FALSE(plan.truncated);
}

void test_medium_title_wraps_to_two_lines() {
  const layout::TrackPlan plan = layout::planTrack(
      "The Rockafeller Skank", "Fatboy Slim", "You've Come a Long Way Baby",
      fakeMeasure);

  TEST_ASSERT_EQUAL(layout::Variant::kTypography, plan.variant);
  TEST_ASSERT_TRUE(plan.title_lines.size() <= 2);
}

// Un titre moyen doit faire baisser le corps avant de changer de disposition :
// on préfère un texte plus petit à une image rétrécie.
void test_long_title_reduces_font_before_changing_layout() {
  const layout::TrackPlan plan = layout::planTrack(
      "Never Gonna Give You Up Tonight Forever", "Rick Astley", "Whenever You Need",
      fakeMeasure);

  TEST_ASSERT_EQUAL(layout::Variant::kTypography, plan.variant);
  TEST_ASSERT_EQUAL(layout::TitleStyle::kLarge, plan.title_style);
}

// Où se situe la bascule entre les deux dispositions ? Plutôt que de figer une
// phrase d'exemple — fragile, et que j'ai déjà mal devinée deux fois — le test
// cherche lui-même la longueur de bascule et vérifie qu'elle tombe dans une
// plage défendable. Trop tôt, on renoncerait à la grande typographie pour des
// titres courants ; trop tard, le texte deviendrait illisible de loin.
void test_layout_switch_boundary_is_reasonable() {
  const auto variantFor = [](int char_count) {
    std::string title;
    while (static_cast<int>(title.size()) < char_count) {
      if (!title.empty()) title += " ";
      title += "motif";  // mots courts : le pire cas pour le retour à la ligne
    }
    title.resize(char_count);
    return layout::planTrack(title, "Artiste", "Album", fakeMeasure).variant;
  };

  int boundary = 0;
  for (int length = 10; length <= 200; ++length) {
    if (variantFor(length) == layout::Variant::kArtwork) {
      boundary = length;
      break;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(boundary > 40, "bascule trop precoce");
  TEST_ASSERT_TRUE_MESSAGE(boundary < 100, "bascule trop tardive");
  // Et la bascule est stable : au-delà, on ne revient pas en arrière.
  TEST_ASSERT_EQUAL(layout::Variant::kArtwork, variantFor(boundary + 30));
}

// Cas réel qui a motivé la disposition de repli : les titres de musique
// classique, avec tonalité, numéro d'opus et mouvement.
void test_very_long_title_falls_back_to_artwork_layout() {
  const layout::TrackPlan plan = layout::planTrack(
      "Piano Concerto No. 21 in C Major, K. 467: II. Andante (Elvira Madigan)",
      "Wolfgang Amadeus Mozart", "The Best of Mozart", fakeMeasure);

  TEST_ASSERT_EQUAL(layout::Variant::kArtwork, plan.variant);
  TEST_ASSERT_EQUAL(layout::TitleStyle::kMedium, plan.title_style);
  TEST_ASSERT_EQUAL_INT(360, plan.art_size_px);
  TEST_ASSERT_TRUE(plan.title_lines.size() <= 4);
}

// La troncature est le tout dernier recours, et elle doit se voir.
void test_absurd_title_is_truncated_visibly() {
  const layout::TrackPlan plan =
      layout::planTrack(std::string(600, 'a') + " fin", "Artiste", "Album", fakeMeasure);

  TEST_ASSERT_TRUE(plan.truncated);
  TEST_ASSERT_EQUAL_UINT(4, plan.title_lines.size());
  const std::string& last = plan.title_lines.back();
  TEST_ASSERT_EQUAL_STRING("...", last.substr(last.size() - 3).c_str());
}

// Un mot unique plus large que la colonne doit être coupé, pas laissé déborder.
void test_unbreakable_word_is_split() {
  std::vector<std::string> lines;
  const bool fits = layout::wrapText(std::string(40, 'x'), 200, 4, layout::TitleStyle::kMedium, fakeMeasure, lines);

  TEST_ASSERT_TRUE(fits);
  TEST_ASSERT_TRUE(lines.size() > 1);
  for (const std::string& line : lines) {
    TEST_ASSERT_TRUE(fakeMeasure(line, layout::TitleStyle::kMedium) <= 200);
  }
}

void test_wrap_keeps_words_intact_when_possible() {
  std::vector<std::string> lines;
  layout::wrapText("un deux trois quatre cinq", 120, 5, layout::TitleStyle::kMedium, fakeMeasure, lines);

  for (const std::string& line : lines) {
    TEST_ASSERT_TRUE(fakeMeasure(line, layout::TitleStyle::kMedium) <= 120);
  }
  // Aucun mot ne doit avoir été coupé : ils tiennent tous.
  std::string rejoined;
  for (const std::string& line : lines) {
    if (!rejoined.empty()) rejoined += " ";
    rejoined += line;
  }
  TEST_ASSERT_EQUAL_STRING("un deux trois quatre cinq", rejoined.c_str());
}

// La barre de progression a été abandonnée — elle serait figée pendant tout le
// morceau. Seule la durée est affichée.
void test_format_duration() {
  TEST_ASSERT_EQUAL_STRING("3:13", layout::formatDuration(193).c_str());
  TEST_ASSERT_EQUAL_STRING("0:07", layout::formatDuration(7).c_str());
  TEST_ASSERT_EQUAL_STRING("1:01:01", layout::formatDuration(3661).c_str());
  TEST_ASSERT_EQUAL_STRING("--:--", layout::formatDuration(0).c_str());
  TEST_ASSERT_EQUAL_STRING("--:--", layout::formatDuration(-5).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_short_title_uses_large_typography);
  RUN_TEST(test_medium_title_wraps_to_two_lines);
  RUN_TEST(test_long_title_reduces_font_before_changing_layout);
  RUN_TEST(test_layout_switch_boundary_is_reasonable);
  RUN_TEST(test_very_long_title_falls_back_to_artwork_layout);
  RUN_TEST(test_absurd_title_is_truncated_visibly);
  RUN_TEST(test_unbreakable_word_is_split);
  RUN_TEST(test_wrap_keeps_words_intact_when_possible);
  RUN_TEST(test_format_duration);
  return UNITY_END();
}
