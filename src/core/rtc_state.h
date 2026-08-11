#pragma once

#include <cstdint>
#include <cstring>

// Sérialisation de l'état qui doit survivre au deep sleep. Le deep sleep
// repasse par setup() : toute variable non marquée RTC_DATA_ATTR est perdue.
//
// Cette structure va en mémoire RTC. Tous les membres sont de taille fixe
// (pas de std::string) afin d'occuper un nombre d'octets constant et de
// survivre au réveil sans relocation.
namespace rtc {

// Taille des tampons, arbitraires mais conservateurs pour les cas réels.
constexpr size_t kZoneNameLen = 32;
constexpr size_t kIPv4StrLen = 16;

struct State {
  // Zone imposée depuis Home Assistant. Empty si auto.
  char forced_zone[kZoneNameLen] = {};

  // Dernière zone lue et son coordinateur (adresse IP).
  char last_zone[kZoneNameLen] = {};
  char last_ip[kIPv4StrLen] = {};

  // État de pause_timer : délai de grâce 5 min après une mise en pause.
  bool pause_timer_paused = false;
  uint32_t pause_timer_since_ms = 0;
  char pause_timer_zone[kZoneNameLen] = {};

  // État de sleep_manager : seuil d'inactivité et arrivée du prochain sleep.
  uint32_t sleep_mgr_inactive_since_ms = 0;
  uint32_t sleep_mgr_wake_at_ms = 0;
};

}  // namespace rtc
