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

// Classement des zones à essayer, de la plus probable à la moins probable :
//   1. celles qui jouent, dans l'ordre de `priority` puis les autres ;
//   2. celles en pause, même ordre ;
//   3. la dernière zone retenue si elle existe encore.
//
// Un classement plutôt qu'un choix unique, parce qu'une zone peut annoncer
// `PLAYING` sans rien savoir du morceau : Spotify Connect laisse cet état
// résiduel sur l'enceinte précédente après un basculement. L'appelant descend
// le classement jusqu'à obtenir des métadonnées.
//
// `forced_zone` court-circuite tout : c'est le sélecteur exposé dans Home
// Assistant. Vide ou "auto" pour appliquer la politique.
std::vector<Choice> rankZones(const std::vector<ZoneStatus>& zones,
                              const std::vector<std::string>& priority,
                              const std::string& last_zone_name = {},
                              const std::string& forced_zone = {});

// Premier candidat du classement, ou `found = false` s'il est vide.
Choice pickZone(const std::vector<ZoneStatus>& zones,
                const std::vector<std::string>& priority,
                const std::string& last_zone_name = {},
                const std::string& forced_zone = {});

}  // namespace sonos
