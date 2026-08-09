// Tests du tramage. Aucun écran requis.
//
// Ces tests vérifient des propriétés — moyenne conservée, palette respectée,
// pas de débordement — et non l'aspect. Le rendu réel sur Spectra 6 ne se juge
// qu'à l'œil, sur le panneau ; voir docs/development.md.
#include <unity.h>

#include <cmath>
#include <vector>

#include "core/dither.h"

namespace {

std::vector<dither::Rgb> uniform(int width, int height, dither::Rgb color) {
  return std::vector<dither::Rgb>(static_cast<size_t>(width) * height, color);
}

}  // namespace

void test_pure_palette_colors_map_to_themselves() {
  for (const dither::Ink ink :
       {dither::Ink::kBlack, dither::Ink::kWhite, dither::Ink::kRed,
        dither::Ink::kGreen, dither::Ink::kBlue, dither::Ink::kYellow}) {
    const dither::Rgb& c = dither::inkColor(ink);
    TEST_ASSERT_EQUAL(ink, dither::nearestInk(c.r, c.g, c.b));
  }
}

void test_output_only_contains_palette_indices() {
  const std::vector<dither::Rgb> image = uniform(32, 32, {120, 200, 90});
  const std::vector<dither::Ink> out = dither::floydSteinberg(image, 32, 32);

  TEST_ASSERT_EQUAL_UINT(32u * 32u, out.size());
  for (const dither::Ink ink : out) {
    TEST_ASSERT_TRUE(static_cast<uint8_t>(ink) <= 5);
  }
}

// La propriété qui fait tout l'intérêt du tramage : sur une plage unie, le
// mélange d'encres doit restituer en moyenne la couleur d'origine, même si
// aucune encre ne s'en approche seule.
void test_dithering_preserves_average_colour() {
  const dither::Rgb target = {150, 150, 150};  // un gris, absent de la palette
  const std::vector<dither::Rgb> image = uniform(64, 64, target);
  const std::vector<dither::Ink> out = dither::floydSteinberg(image, 64, 64);

  long r = 0, g = 0, b = 0;
  for (const dither::Ink ink : out) {
    const dither::Rgb& c = dither::inkColor(ink);
    r += c.r;
    g += c.g;
    b += c.b;
  }
  const long n = static_cast<long>(out.size());

  TEST_ASSERT_INT_WITHIN(20, target.r, r / n);
  TEST_ASSERT_INT_WITHIN(20, target.g, g / n);
  TEST_ASSERT_INT_WITHIN(20, target.b, b / n);
}

// Un gris moyen ne doit pas se rendre en un seul aplat : c'est le signe que la
// diffusion d'erreur ne fonctionne pas.
void test_midtone_uses_more_than_one_ink() {
  const std::vector<dither::Ink> out =
      dither::floydSteinberg(uniform(32, 32, {150, 150, 150}), 32, 32);

  bool distinct = false;
  for (const dither::Ink ink : out) {
    if (ink != out[0]) {
      distinct = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(distinct);
}

// Le blanc pur, lui, doit rester un aplat : diffuser de l'erreur sur une zone
// déjà exacte salirait les fonds clairs des pochettes.
void test_pure_white_stays_flat() {
  const dither::Rgb white = dither::inkColor(dither::Ink::kWhite);
  const std::vector<dither::Ink> out = dither::floydSteinberg(uniform(16, 16, white), 16, 16);

  for (const dither::Ink ink : out) {
    TEST_ASSERT_EQUAL(dither::Ink::kWhite, ink);
  }
}

void test_downscale_averages_blocks() {
  // Damier 4x4 noir et blanc : réduit en 2x2, chaque bloc doit donner un gris.
  std::vector<dither::Rgb> image(16);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const bool light = ((x + y) % 2) == 0;
      image[y * 4 + x] = light ? dither::Rgb{255, 255, 255} : dither::Rgb{0, 0, 0};
    }
  }

  const std::vector<dither::Rgb> out = dither::downscale(image, 4, 4, 2, 2);

  TEST_ASSERT_EQUAL_UINT(4u, out.size());
  for (const dither::Rgb& p : out) {
    TEST_ASSERT_INT_WITHIN(2, 127, p.r);
  }
}

// Cas réel : les pochettes arrivent en 640x640 et l'emplacement fait 200 px.
void test_downscale_handles_real_album_art_ratio() {
  const std::vector<dither::Rgb> out =
      dither::downscale(uniform(640, 640, {80, 160, 240}), 640, 640, 200, 200);

  TEST_ASSERT_EQUAL_UINT(200u * 200u, out.size());
  TEST_ASSERT_EQUAL_UINT8(80, out[0].r);
  TEST_ASSERT_EQUAL_UINT8(160, out[0].g);
  TEST_ASSERT_EQUAL_UINT8(240, out[0].b);
}

// Entrées incohérentes : renvoyer vide plutôt que lire hors des bornes. Une
// pochette tronquée en cours de téléchargement produit exactement ce cas.
void test_malformed_input_returns_empty() {
  TEST_ASSERT_TRUE(dither::floydSteinberg({}, 10, 10).empty());
  TEST_ASSERT_TRUE(dither::floydSteinberg(uniform(4, 4, {0, 0, 0}), 0, 4).empty());
  TEST_ASSERT_TRUE(dither::downscale(uniform(4, 4, {0, 0, 0}), 4, 4, 0, 2).empty());
  TEST_ASSERT_TRUE(dither::downscale({}, 100, 100, 10, 10).empty());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pure_palette_colors_map_to_themselves);
  RUN_TEST(test_output_only_contains_palette_indices);
  RUN_TEST(test_dithering_preserves_average_colour);
  RUN_TEST(test_midtone_uses_more_than_one_ink);
  RUN_TEST(test_pure_white_stays_flat);
  RUN_TEST(test_downscale_averages_blocks);
  RUN_TEST(test_downscale_handles_real_album_art_ratio);
  RUN_TEST(test_malformed_input_returns_empty);
  return UNITY_END();
}
