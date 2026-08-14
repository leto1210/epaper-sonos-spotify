#pragma once

#include <string>

// Repli des caractères accentués vers l'ASCII, pour l'affichage.
//
// Les FreeFonts d'Adafruit ne couvrent que l'ASCII imprimable. Un caractère
// hors de cette plage n'est pas remplacé par un rectangle : il est **supprimé
// sans avertissement**. « Sonos Séjour » s'affichait donc « Sonos Sjour », et
// un titre français y perdait ses lettres une à une.
//
// Le repli se fait à l'entrée de l'affichage seulement. Les noms exacts, avec
// leurs accents, continuent de partir vers Home Assistant : c'est du texte, pas
// une image, et rien ne l'y ampute.
namespace text {

// « Séjour » -> « Sejour », « Ça » -> « Ca », « œuvre » -> « oeuvre ».
// Une séquence UTF-8 sans équivalent est retirée plutôt que remplacée par un
// caractère de substitution, qui ferait du bruit sans rien apprendre.
std::string foldToAscii(const std::string& utf8);

}  // namespace text
