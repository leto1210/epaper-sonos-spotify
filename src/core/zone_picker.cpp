#include "core/zone_picker.h"

namespace sonos {
namespace {

bool isPlaying(const ZoneStatus& zone) {
  return zone.state == TransportState::kPlaying ||
         zone.state == TransportState::kTransitioning;
}

// Une zone en pause reste la zone « en cours » du point de vue de
// l'utilisateur, mais elle ne doit jamais l'emporter sur une zone qui joue
// vraiment — même si elle est plus haut dans les préférences.
bool isCurrent(const ZoneStatus& zone) {
  return isPlaying(zone) || zone.state == TransportState::kPaused;
}

const ZoneStatus* findByName(const std::vector<ZoneStatus>& zones, const std::string& name) {
  for (const ZoneStatus& zone : zones) {
    if (zone.name == name) return &zone;
  }
  return nullptr;
}

}  // namespace

Choice pickZone(const std::vector<ZoneStatus>& zones,
                const std::vector<std::string>& priority,
                const std::string& last_zone_name,
                const std::string& forced_zone) {
  Choice choice;

  if (!forced_zone.empty() && forced_zone != "auto") {
    if (const ZoneStatus* zone = findByName(zones, forced_zone)) {
      choice.found = true;
      choice.name = zone->name;
      choice.ip = zone->ip;
      choice.playing = isCurrent(*zone);
    }
    // Une zone forcée introuvable ne doit pas retomber silencieusement sur une
    // autre pièce : l'utilisateur a demandé celle-là.
    return choice;
  }

  // Deux passes : ce qui joue vraiment d'abord, les préférences ensuite. Une
  // zone favorite mise en pause ne doit pas masquer la pièce où la musique
  // tourne réellement.
  for (bool playing_only : {true, false}) {
    auto retained = [playing_only](const ZoneStatus& zone) {
      return playing_only ? isPlaying(zone) : isCurrent(zone);
    };

    for (const std::string& preferred : priority) {
      const ZoneStatus* zone = findByName(zones, preferred);
      if (zone != nullptr && retained(*zone)) {
        return Choice{true, zone->name, zone->ip, true};
      }
    }
    for (const ZoneStatus& zone : zones) {
      if (retained(zone)) {
        return Choice{true, zone.name, zone.ip, true};
      }
    }
  }

  if (!last_zone_name.empty()) {
    if (const ZoneStatus* zone = findByName(zones, last_zone_name)) {
      return Choice{true, zone->name, zone->ip, false};
    }
  }

  return choice;
}

}  // namespace sonos
