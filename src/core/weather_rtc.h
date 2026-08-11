#pragma once

#include <cstdint>

#include "core/weather.h"

// Conservation du bulletin météo à travers le deep sleep.
//
// Le boîtier se rendort désormais toutes les minutes, et chaque réveil repasse
// par `setup()` : le dernier bulletin reçu était perdu. Home Assistant le
// republie bien, le sujet étant retenu — mais l'abonnement, la livraison et le
// rendu ne tiennent pas dans les trois secondes d'éveil, si bien que l'écran
// affichait « Météo indisponible » alors que la donnée existait.
//
// D'où cette forme compacte et de taille fixe, seule admise en mémoire RTC :
// ni `std::string` ni `std::vector` n'y ont leur place.
namespace weather {

struct RtcHour {
  char label[4] = {};  // "16", plus le terminateur
  uint8_t condition = 0;
  int16_t temperature_dc = 0;    // dixièmes de degré
  uint16_t precipitation_dmm = 0;  // dixièmes de millimètre
};

struct RtcReport {
  // Distingue une mémoire RTC jamais écrite d'un bulletin valide. Au premier
  // démarrage, elle contient n'importe quoi.
  uint32_t magic = 0;

  uint8_t condition = 0;
  int16_t temperature_dc = 0;
  uint8_t humidity_pct = 0;
  uint16_t wind_kmh = 0;
  uint8_t uv_index_d = 0;  // dixièmes, 0 à 25,5

  int32_t published_at = 0;

  uint8_t hour_count = 0;
  RtcHour hours[6];
};

constexpr uint32_t kRtcMagic = 0x57544852;  // "WTHR"

RtcReport toRtc(const Report& report);

// Renvoie un rapport avec `valid = false` si la zone n'a jamais été écrite.
Report fromRtc(const RtcReport& stored);

}  // namespace weather
