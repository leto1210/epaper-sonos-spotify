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

  // État de pause_timer : délai de grâce 5 min après une mise en pause. On
  // conserve une durée écoulée, pas une date — voir core/pause_timer.h.
  bool pause_timer_paused = false;
  uint32_t pause_timer_elapsed_ms = 0;
  char pause_timer_zone[kZoneNameLen] = {};

  // Durée du sommeil qui vient d'être demandé, pour que le réveil sache
  // combien de temps créditer aux compteurs qui mesurent du temps réel.
  uint32_t sleep_duration_ms = 0;

  // Temps de fonctionnement cumulé depuis le dernier démarrage à froid, sommeil
  // compris. Voir core/uptime.h : sans ce cumul, l'entité de Home Assistant
  // retombait à quelques secondes à chaque réveil.
  uint64_t uptime_accumulated_ms = 0;

  // État de sleep_manager : seuil d'inactivité et arrivée du prochain sleep.
  uint32_t sleep_mgr_inactive_since_ms = 0;
  uint32_t sleep_mgr_wake_at_ms = 0;
};

}  // namespace rtc
