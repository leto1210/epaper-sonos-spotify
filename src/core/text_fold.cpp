#include "core/text_fold.h"

#include <cstdint>

namespace text {
namespace {

// Supplément Latin-1 et latin étendu A, atteints en UTF-8 par les préfixes
// 0xC3 et 0xC5. La table est indexée par le second octet.
const char* foldTwoByte(uint8_t lead, uint8_t second) {
  if (lead == 0xC3) {
    switch (second) {
      case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: return "A";
      case 0x86: return "AE";
      case 0x87: return "C";
      case 0x88: case 0x89: case 0x8A: case 0x8B: return "E";
      case 0x8C: case 0x8D: case 0x8E: case 0x8F: return "I";
      case 0x91: return "N";
      case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x98: return "O";
      case 0x99: case 0x9A: case 0x9B: case 0x9C: return "U";
      case 0x9D: return "Y";
      case 0x9F: return "ss";
      case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: return "a";
      case 0xA6: return "ae";
      case 0xA7: return "c";
      case 0xA8: case 0xA9: case 0xAA: case 0xAB: return "e";
      case 0xAC: case 0xAD: case 0xAE: case 0xAF: return "i";
      case 0xB1: return "n";
      case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB8: return "o";
      case 0xB9: case 0xBA: case 0xBB: case 0xBC: return "u";
      case 0xBD: case 0xBF: return "y";
      default: return nullptr;
    }
  }
  if (lead == 0xC5) {
    switch (second) {
      case 0x92: return "OE";
      case 0x93: return "oe";
      case 0xA0: return "S";
      case 0xA1: return "s";
      case 0xB8: return "Y";
      case 0xBD: return "Z";
      case 0xBE: return "z";
      default: return nullptr;
    }
  }
  return nullptr;
}

// Ponctuation typographique : apostrophes et guillemets courbes, tirets longs,
// points de suspension. Fréquents dans les titres, et tous en 0xE2 0x80 xx.
const char* foldPunctuation(uint8_t third) {
  switch (third) {
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: return "-";
    case 0x98: case 0x99: return "'";
    case 0x9C: case 0x9D: return "\"";
    case 0xA6: return "...";
    default: return nullptr;
  }
}

}  // namespace

std::string foldToAscii(const std::string& utf8) {
  std::string out;
  out.reserve(utf8.size());

  for (size_t i = 0; i < utf8.size();) {
    const uint8_t c = static_cast<uint8_t>(utf8[i]);

    if (c < 0x80) {  // ASCII : rien à faire
      out.push_back(static_cast<char>(c));
      ++i;
      continue;
    }

    if ((c == 0xC3 || c == 0xC5) && i + 1 < utf8.size()) {
      if (const char* replacement =
              foldTwoByte(c, static_cast<uint8_t>(utf8[i + 1]))) {
        out.append(replacement);
      }
      i += 2;
      continue;
    }

    if (c == 0xE2 && i + 2 < utf8.size() &&
        static_cast<uint8_t>(utf8[i + 1]) == 0x80) {
      if (const char* replacement =
              foldPunctuation(static_cast<uint8_t>(utf8[i + 2]))) {
        out.append(replacement);
      }
      i += 3;
      continue;
    }

    // Séquence inconnue : on la saute entièrement, en s'appuyant sur la
    // longueur annoncée par son premier octet. Avancer d'un seul octet
    // laisserait les octets de continuation se glisser dans la sortie.
    if ((c & 0xE0) == 0xC0) i += 2;
    else if ((c & 0xF0) == 0xE0) i += 3;
    else if ((c & 0xF8) == 0xF0) i += 4;
    else ++i;
  }

  return out;
}

}  // namespace text
