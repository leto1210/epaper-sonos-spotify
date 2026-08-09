#include "core/sonos_parser.h"

#include <cstdlib>

namespace sonos {
namespace {

// Extraction par balise plutôt que par arbre DOM : les réponses font jusqu'à
// une quinzaine de kilo-octets et l'ESP32 n'a aucune raison d'en construire
// une représentation complète pour n'en lire que six champs.
std::string tagValue(const std::string& xml, const std::string& tag, size_t from = 0) {
  const std::string open = "<" + tag;
  const std::string close = "</" + tag + ">";

  // Le nom de balise doit se terminer là où on le pense : sans cette
  // vérification, chercher `upnp:album` trouve `upnp:albumArtURI`.
  size_t start = std::string::npos;
  for (size_t at = from; (at = xml.find(open, at)) != std::string::npos; ++at) {
    const char next = at + open.size() < xml.size() ? xml[at + open.size()] : '\0';
    if (next == '>' || next == ' ' || next == '/' || next == '\t' || next == '\n') {
      start = at;
      break;
    }
  }
  if (start == std::string::npos) return {};
  start = xml.find('>', start);
  if (start == std::string::npos) return {};
  ++start;

  const size_t end = xml.find(close, start);
  if (end == std::string::npos) return {};
  return xml.substr(start, end - start);
}

// Valeur d'un attribut XML, en acceptant les deux formes de guillemet : la
// topologie est renvoyée échappée, `"` y devient `&quot;`.
std::string attrValue(const std::string& xml, const std::string& name, size_t from = 0) {
  for (const char* quote : {"\"", "&quot;"}) {
    const std::string prefix = name + "=" + quote;
    const size_t start = xml.find(prefix, from);
    if (start == std::string::npos) continue;
    const size_t value_start = start + prefix.size();
    const size_t end = xml.find(quote, value_start);
    if (end == std::string::npos) continue;
    return xml.substr(value_start, end - value_start);
  }
  return {};
}

bool isPlaceholder(const std::string& value) {
  return value.empty() || value == "NOT_IMPLEMENTED";
}

}  // namespace

std::string xmlUnescape(const std::string& text) {
  std::string out;
  out.reserve(text.size());

  for (size_t i = 0; i < text.size();) {
    if (text[i] != '&') {
      out += text[i++];
      continue;
    }
    const size_t semi = text.find(';', i);
    if (semi == std::string::npos || semi - i > 8) {
      out += text[i++];
      continue;
    }
    const std::string entity = text.substr(i, semi - i + 1);
    if (entity == "&lt;") out += '<';
    else if (entity == "&gt;") out += '>';
    else if (entity == "&quot;") out += '"';
    else if (entity == "&apos;") out += '\'';
    else if (entity == "&amp;") out += '&';
    else out += entity;  // entité inconnue : laissée telle quelle
    i = semi + 1;
  }
  return out;
}

int parseDuration(const std::string& hms) {
  if (isPlaceholder(hms)) return 0;

  int parts[3] = {0, 0, 0};
  int count = 0;
  size_t pos = 0;
  while (count < 3 && pos <= hms.size()) {
    const size_t colon = hms.find(':', pos);
    const std::string chunk = hms.substr(pos, colon == std::string::npos
                                                  ? std::string::npos
                                                  : colon - pos);
    parts[count++] = std::atoi(chunk.c_str());
    if (colon == std::string::npos) break;
    pos = colon + 1;
  }

  if (count == 3) return parts[0] * 3600 + parts[1] * 60 + parts[2];
  if (count == 2) return parts[0] * 60 + parts[1];
  return parts[0];
}

TrackInfo parsePositionInfo(const std::string& soap) {
  TrackInfo track;

  track.track_uri = xmlUnescape(tagValue(soap, "TrackURI"));
  track.duration_s = parseDuration(tagValue(soap, "TrackDuration"));
  track.position_s = parseDuration(tagValue(soap, "RelTime"));

  // Une enceinte esclave renvoie NOT_IMPLEMENTED partout, mais son TrackURI
  // trahit son coordinateur : `x-rincon:RINCON_...`. C'est le seul indice
  // disponible sans passer par la topologie.
  constexpr const char* kSlavePrefix = "x-rincon:";
  if (track.track_uri.rfind(kSlavePrefix, 0) == 0) {
    track.coordinator_uuid = track.track_uri.substr(std::string(kSlavePrefix).size());
  }

  // Doublement échappé : une fois pour l'enveloppe SOAP, une fois pour loger du
  // XML (le DIDL-Lite) dans un champ texte.
  const std::string metadata = xmlUnescape(xmlUnescape(tagValue(soap, "TrackMetaData")));
  if (isPlaceholder(metadata)) return track;

  track.title = tagValue(metadata, "dc:title");
  track.artist = tagValue(metadata, "dc:creator");
  track.album = tagValue(metadata, "upnp:album");
  track.art_uri = tagValue(metadata, "upnp:albumArtURI");
  track.has_metadata = !track.title.empty();

  return track;
}

TransportState parseTransportState(const std::string& soap) {
  const std::string state = tagValue(soap, "CurrentTransportState");
  if (state == "PLAYING") return TransportState::kPlaying;
  if (state == "PAUSED_PLAYBACK") return TransportState::kPaused;
  if (state == "STOPPED") return TransportState::kStopped;
  if (state == "TRANSITIONING") return TransportState::kTransitioning;
  return TransportState::kUnknown;
}

const ZoneMember* ZoneGroup::coordinator() const {
  for (const ZoneMember& member : members) {
    if (member.uuid == coordinator_uuid) return &member;
  }
  return nullptr;
}

std::vector<ZoneGroup> parseZoneGroups(const std::string& soap) {
  const std::string state = xmlUnescape(xmlUnescape(tagValue(soap, "ZoneGroupState")));
  std::vector<ZoneGroup> groups;

  size_t pos = 0;
  while (true) {
    const size_t group_start = state.find("<ZoneGroup ", pos);
    if (group_start == std::string::npos) break;
    const size_t group_end = state.find("</ZoneGroup>", group_start);
    if (group_end == std::string::npos) break;

    const std::string block = state.substr(group_start, group_end - group_start);
    ZoneGroup group;
    group.coordinator_uuid = attrValue(block, "Coordinator");

    size_t member_pos = 0;
    while (true) {
      const size_t member_start = block.find("<ZoneGroupMember ", member_pos);
      if (member_start == std::string::npos) break;
      const size_t member_end = block.find('>', member_start);
      if (member_end == std::string::npos) break;

      const std::string entry = block.substr(member_start, member_end - member_start);
      ZoneMember member;
      member.uuid = attrValue(entry, "UUID");
      member.name = attrValue(entry, "ZoneName");

      // Location = "http://192.0.2.10:1400/xml/device_description.xml"
      const std::string location = attrValue(entry, "Location");
      const size_t host_start = location.find("://");
      if (host_start != std::string::npos) {
        const size_t host = host_start + 3;
        const size_t colon = location.find(':', host);
        if (colon != std::string::npos) member.ip = location.substr(host, colon - host);
      }

      if (!member.uuid.empty()) group.members.push_back(member);
      member_pos = member_end;
    }

    if (!group.members.empty()) groups.push_back(group);
    pos = group_end;
  }

  return groups;
}

std::string albumArtUrl(const std::string& coordinator_ip, const TrackInfo& track) {
  if (track.track_uri.empty()) return {};

  // Encodage pour-cent de l'URI du morceau : elle contient `:` et `&`.
  std::string encoded;
  encoded.reserve(track.track_uri.size() * 3);
  for (const unsigned char c : track.track_uri) {
    const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                            c == '.' || c == '~';
    if (unreserved) {
      encoded += static_cast<char>(c);
    } else {
      static const char* kHex = "0123456789ABCDEF";
      encoded += '%';
      encoded += kHex[c >> 4];
      encoded += kHex[c & 0x0F];
    }
  }

  return "http://" + coordinator_ip + ":1400/getaa?s=1&u=" + encoded;
}

}  // namespace sonos
