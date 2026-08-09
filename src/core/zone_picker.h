#pragma once

#include <string>
#include <vector>

#include "core/sonos_parser.h"

// Choix de la zone à afficher. Logique pure, séparée du réseau : c'est une
// décision, et elle se teste sans enceintes.
namespace sonos {

struct ZoneStatus {
  std::string coordinator_uuid;
  std::string name;
  std::string ip;
  TransportState state = TransportState::kUnknown;
};

struct Choice {
  bool found = false;
  std::string name;
  std::string ip;

  // Vrai quand la zone retenue joue vraiment. Faux quand on retombe sur la
  // dernière zone active faute de mieux : l'appelant garde alors la dernière
  // fiche connue plutôt que d'effacer l'écran.
  bool playing = false;
};

// Politique :
//   1. parmi les zones qui jouent, la première de `priority` ;
//   2. à défaut, n'importe quelle zone qui joue ;
//   3. à défaut, la dernière zone retenue si elle existe encore ;
//   4. sinon, rien.
//
// `forced_zone` court-circuite tout : c'est le sélecteur exposé dans Home
// Assistant. Vide ou "auto" pour appliquer la politique.
Choice pickZone(const std::vector<ZoneStatus>& zones,
                const std::vector<std::string>& priority,
                const std::string& last_zone_name = {},
                const std::string& forced_zone = {});

}  // namespace sonos
