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

Choice toChoice(const ZoneStatus& zone) {
  return Choice{true, zone.name, zone.ip, isPlaying(zone)};
}

bool alreadyListed(const std::vector<Choice>& ranked, const std::string& name) {
  for (const Choice& choice : ranked) {
    if (choice.name == name) return true;
  }
  return false;
}

void append(std::vector<Choice>& ranked, const ZoneStatus& zone) {
  if (!alreadyListed(ranked, zone.name)) ranked.push_back(toChoice(zone));
}

}  // namespace

std::vector<Choice> rankZones(const std::vector<ZoneStatus>& zones,
                              const std::vector<std::string>& priority,
                              const std::string& last_zone_name,
                              const std::string& forced_zone) {
  std::vector<Choice> ranked;

  if (!forced_zone.empty() && forced_zone != "auto") {
    // Une zone forcée introuvable ne doit pas retomber silencieusement sur une
    // autre pièce : l'utilisateur a demandé celle-là.
    if (const ZoneStatus* zone = findByName(zones, forced_zone)) {
      ranked.push_back(toChoice(*zone));
    }
    return ranked;
  }

  // Deux passes : ce qui joue vraiment d'abord, les zones en pause ensuite.
  // Dans chaque passe, les préférences priment sur l'ordre de découverte.
  for (const bool playing_only : {true, false}) {
    const auto retained = [playing_only](const ZoneStatus& zone) {
      return playing_only ? isPlaying(zone) : isCurrent(zone);
    };

    for (const std::string& preferred : priority) {
      const ZoneStatus* zone = findByName(zones, preferred);
      if (zone != nullptr && retained(*zone)) append(ranked, *zone);
    }
    for (const ZoneStatus& zone : zones) {
      if (retained(zone)) append(ranked, zone);
    }
  }

  if (!last_zone_name.empty()) {
    if (const ZoneStatus* zone = findByName(zones, last_zone_name)) append(ranked, *zone);
  }

  return ranked;
}

Choice pickZone(const std::vector<ZoneStatus>& zones,
                const std::vector<std::string>& priority,
                const std::string& last_zone_name,
                const std::string& forced_zone) {
  const std::vector<Choice> ranked =
      rankZones(zones, priority, last_zone_name, forced_zone);
  return ranked.empty() ? Choice{} : ranked.front();
}

}  // namespace sonos
