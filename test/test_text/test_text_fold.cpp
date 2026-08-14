// Tests du repli vers l'ASCII. Aucun écran requis.
#include <unity.h>

#include "core/text_fold.h"

// Le défaut d'origine, vu sur une photo du panneau : « Sonos Séjour »
// s'affichait « Sonos Sjour ». Les FreeFonts d'Adafruit ne couvrent que
// l'ASCII et suppriment le reste sans avertissement.
void test_folds_the_zone_name_that_revealed_the_bug() {
  TEST_ASSERT_EQUAL_STRING("Sonos Sejour", text::foldToAscii("Sonos Séjour").c_str());
}

void test_leaves_plain_ascii_untouched() {
  TEST_ASSERT_EQUAL_STRING("Sonos Beam", text::foldToAscii("Sonos Beam").c_str());
  TEST_ASSERT_EQUAL_STRING("", text::foldToAscii("").c_str());
}

void test_folds_the_usual_french_accents() {
  TEST_ASSERT_EQUAL_STRING("Ca fait rire les oiseaux",
                           text::foldToAscii("Ça fait rire les oiseaux").c_str());
  TEST_ASSERT_EQUAL_STRING("aeiou", text::foldToAscii("àéîôù").c_str());
  TEST_ASSERT_EQUAL_STRING("Chambre Parent, cote jardin",
                           text::foldToAscii("Chambre Parent, côté jardin").c_str());
}

// Les ligatures et l'eszett valent deux lettres, pas une lettre amputée.
void test_expands_ligatures() {
  TEST_ASSERT_EQUAL_STRING("oeuvre", text::foldToAscii("œuvre").c_str());
  TEST_ASSERT_EQUAL_STRING("Strasse", text::foldToAscii("Straße").c_str());
  TEST_ASSERT_EQUAL_STRING("AEther", text::foldToAscii("Æther").c_str());
}

// Les titres de morceaux regorgent d'apostrophes courbes et de tirets longs.
void test_folds_typographic_punctuation() {
  TEST_ASSERT_EQUAL_STRING("L'ete indien", text::foldToAscii("L’été indien").c_str());
  TEST_ASSERT_EQUAL_STRING("Live - Remaster",
                           text::foldToAscii("Live — Remaster").c_str());
  TEST_ASSERT_EQUAL_STRING("\"Bal\" masque",
                           text::foldToAscii("“Bal” masqué").c_str());
  TEST_ASSERT_EQUAL_STRING("Et puis...", text::foldToAscii("Et puis…").c_str());
}

// Un caractère sans équivalent est retiré en entier. Avancer d'un seul octet
// laisserait ses octets de continuation salir la sortie — c'est exactement le
// genre de détail qui produit des caractères parasites à l'écran.
void test_drops_unmapped_sequences_whole() {
  TEST_ASSERT_EQUAL_STRING("ab", text::foldToAscii("a漢b").c_str());
  TEST_ASSERT_EQUAL_STRING("ab", text::foldToAscii("a😀b").c_str());
  // Pas de traduction inventée : le chiffre cerclé n'a pas d'équivalent ASCII
  // dans la table, il disparaît comme le reste.
  TEST_ASSERT_EQUAL_STRING("Piste ", text::foldToAscii("Piste ①").c_str());
}

// Une chaîne tronquée en plein caractère ne doit pas faire déborder la lecture.
void test_survives_a_truncated_sequence() {
  const std::string truncated = std::string("Sejour ") + static_cast<char>(0xC3);
  TEST_ASSERT_EQUAL_STRING("Sejour ", text::foldToAscii(truncated).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_folds_the_zone_name_that_revealed_the_bug);
  RUN_TEST(test_leaves_plain_ascii_untouched);
  RUN_TEST(test_folds_the_usual_french_accents);
  RUN_TEST(test_expands_ligatures);
  RUN_TEST(test_folds_typographic_punctuation);
  RUN_TEST(test_drops_unmapped_sequences_whole);
  RUN_TEST(test_survives_a_truncated_sequence);
  return UNITY_END();
}
