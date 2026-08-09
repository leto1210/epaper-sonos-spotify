#pragma once

#include <string>
#include <vector>

// Analyse des réponses SOAP de Sonos. Logique pure, sans Arduino : testée sur la
// machine de dev à partir de captures réelles (test/fixtures/).
//
// Voir docs/sonos-api.md pour les requêtes correspondantes.
namespace sonos {

enum class TransportState {
  kUnknown,
  kPlaying,
  kPaused,
  kStopped,
  kTransitioning,
};

struct TrackInfo {
  std::string title;
  std::string artist;
  std::string album;
  std::string art_uri;    // relatif à l'enceinte, ou absolu (CDN Spotify)
  std::string track_uri;  // sert à demander la pochette à l'enceinte
  int duration_s = 0;
  int position_s = 0;

  // Faux quand l'enceinte ne sait rien du morceau : file d'attente vide, ou
  // enceinte esclave d'un groupe. Voir `coordinator_uuid`.
  bool has_metadata = false;

  // Renseigné quand l'enceinte interrogée est esclave : c'est l'UUID de
  // l'enceinte coordinatrice, la seule qui connaisse le morceau en cours.
  std::string coordinator_uuid;
};

struct ZoneMember {
  std::string uuid;
  std::string name;  // nom de la pièce
  std::string ip;
};

struct ZoneGroup {
  std::string coordinator_uuid;
  std::vector<ZoneMember> members;

  // L'enceinte à interroger pour ce groupe.
  const ZoneMember* coordinator() const;
};

// Réponse de GetPositionInfo.
TrackInfo parsePositionInfo(const std::string& soap);

// Réponse de GetTransportInfo.
TransportState parseTransportState(const std::string& soap);

// Réponse de GetZoneGroupState.
std::vector<ZoneGroup> parseZoneGroups(const std::string& soap);

// Construit l'URL de la pochette servie par l'enceinte elle-même. On ne se sert
// pas de `art_uri` quand il pointe vers un CDN : l'enceinte fait proxy et sert
// l'image en HTTP simple sur le LAN, ce qui évite TLS et tout accès Internet.
std::string albumArtUrl(const std::string& coordinator_ip, const TrackInfo& track);

// --- Exposés pour les tests -------------------------------------------------

// Dé-échappe &lt; &amp; etc. Les métadonnées Sonos sont doublement échappées.
std::string xmlUnescape(const std::string& text);

// "0:03:13" -> 193. Renvoie 0 pour "NOT_IMPLEMENTED" ou une valeur vide.
int parseDuration(const std::string& hms);

}  // namespace sonos
