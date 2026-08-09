#pragma once

#include <cstdint>
#include <vector>

// Conversion d'une image couleur vers la palette de six teintes du Spectra 6.
// Logique pure : testable sur machine de dev, sans écran.
namespace dither {

// Indices de la palette. L'ordre est interne au module ; la correspondance vers
// les constantes TFT_* de Seeed_GFX vit dans le code d'affichage.
enum class Ink : uint8_t {
  kBlack = 0,
  kWhite = 1,
  kRed = 2,
  kGreen = 3,
  kBlue = 4,
  kYellow = 5,
};

struct Rgb {
  uint8_t r, g, b;
};

// Teintes telles qu'elles apparaissent réellement sur le panneau, et non les
// primaires saturées qu'on serait tenté d'écrire. Un ePaper couleur rend des
// pigments, pas de la lumière : viser #FF0000 pour le rouge fausse le calcul
// d'erreur et assombrit toute l'image.
const Rgb& inkColor(Ink ink);

// Tramage de Floyd-Steinberg vers les six encres.
//
// `pixels` est en RGB888, `width * height` pixels, `out` reçoit un indice de
// palette par pixel. La diffusion d'erreur travaille sur deux lignes de
// flottants seulement : sur ESP32-S3, une pochette de 640x640 en RGB888 occupe
// déjà 1,2 Mo de PSRAM, inutile d'en ajouter autant.
//
// Les surcharges à pointeurs existent parce que sur la cible les tampons
// d'image sont alloués en PSRAM, hors du tas où vivent les std::vector.
bool floydSteinberg(const Rgb* pixels, int width, int height, Ink* out);
std::vector<Ink> floydSteinberg(const std::vector<Rgb>& pixels, int width, int height);

// Encre la plus proche d'une couleur donnée, sans diffusion d'erreur.
Ink nearestInk(int r, int g, int b);

// Redimensionne par moyenne de blocs (box filter). Les pochettes arrivent en
// 640x640 et l'emplacement fait 200 ou 360 px : un simple échantillonnage
// produirait de l'aliasing que le tramage amplifierait ensuite.
bool downscale(const Rgb* src, int src_width, int src_height, Rgb* dst, int dst_width,
               int dst_height);
std::vector<Rgb> downscale(const std::vector<Rgb>& pixels, int src_width, int src_height,
                           int dst_width, int dst_height);

}  // namespace dither
