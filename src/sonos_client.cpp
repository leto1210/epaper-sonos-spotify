#include "sonos_client.h"

#include <Arduino.h>
#include <WiFiClient.h>

#include <algorithm>

namespace sonos_client {
namespace {

constexpr uint16_t kSonosPort = 1400;
constexpr uint32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kReadTimeoutMs = 6000;

const char* kAvTransportPath = "/MediaRenderer/AVTransport/Control";
const char* kAvTransportService = "urn:schemas-upnp-org:service:AVTransport:1";
const char* kTopologyPath = "/ZoneGroupTopology/Control";
const char* kTopologyService = "urn:schemas-upnp-org:service:ZoneGroupTopology:1";

std::string envelope(const std::string& service, const std::string& action,
                     const std::string& arguments) {
  return "<?xml version=\"1.0\"?>"
         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>"
         "<u:" + action + " xmlns:u=\"" + service + "\">" + arguments +
         "</u:" + action + "></s:Body></s:Envelope>";
}

// Requête SOAP synchrone. Renvoie une chaîne vide en cas d'échec — l'appelant
// distingue toujours « pas de réponse » de « réponse sans métadonnées ».
std::string soap(const std::string& ip, const char* path, const char* service,
                 const std::string& action, const std::string& arguments = "<InstanceID>0</InstanceID>") {
  WiFiClient client;
  client.setTimeout(kReadTimeoutMs / 1000);

  if (!client.connect(ip.c_str(), kSonosPort, kConnectTimeoutMs)) {
    Serial.printf("[sonos] connexion impossible a %s\n", ip.c_str());
    return {};
  }

  const std::string body = envelope(service, action, arguments);
  std::string request;
  request.reserve(body.size() + 320);
  request += "POST " + std::string(path) + " HTTP/1.1\r\n";
  request += "HOST: " + ip + ":1400\r\n";
  request += "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n";
  request += "SOAPACTION: \"" + std::string(service) + "#" + action + "\"\r\n";
  request += "CONNECTION: close\r\n";
  request += "CONTENT-LENGTH: " + std::to_string(body.size()) + "\r\n\r\n";
  request += body;

  client.print(request.c_str());

  // Saute les en-têtes de réponse : seul le corps nous intéresse, et le
  // parseur travaille par recherche de balise, pas sur un document structuré.
  const uint32_t deadline = millis() + kReadTimeoutMs;
  bool headers_done = false;
  std::string response;
  while (client.connected() || client.available()) {
    if (millis() > deadline) {
      Serial.printf("[sonos] delai depasse sur %s (%s)\n", ip.c_str(), action.c_str());
      break;
    }
    if (!client.available()) {
      delay(5);
      continue;
    }
    if (!headers_done) {
      const String line = client.readStringUntil('\n');
      if (line.length() <= 1) headers_done = true;
      continue;
    }
    // Ne lire que ce qui est réellement disponible : readBytes() attend de
    // remplir tout le tampon et bloque jusqu'au délai d'attente sur le dernier
    // fragment, ce qui ajoutait plusieurs secondes par requête et noyait la
    // console d'avertissements.
    char buffer[512];
    const size_t wanted = std::min(static_cast<size_t>(client.available()), sizeof(buffer));
    const int read = client.read(reinterpret_cast<uint8_t*>(buffer), wanted);
    if (read > 0) response.append(buffer, read);
  }
  client.stop();

  return response;
}

}  // namespace

bool fetchZones(const std::string& seed_ip, std::vector<sonos::ZoneStatus>& out) {
  out.clear();

  const std::string topology =
      soap(seed_ip, kTopologyPath, kTopologyService, "GetZoneGroupState", "");
  if (topology.empty()) return false;

  const std::vector<sonos::ZoneGroup> groups = sonos::parseZoneGroups(topology);
  if (groups.empty()) {
    Serial.println("[sonos] topologie illisible");
    return false;
  }

  for (const sonos::ZoneGroup& group : groups) {
    const sonos::ZoneMember* coordinator = group.coordinator();
    if (coordinator == nullptr || coordinator->ip.empty()) continue;

    sonos::ZoneStatus zone;
    zone.coordinator_uuid = coordinator->uuid;
    zone.name = coordinator->name;
    zone.ip = coordinator->ip;
    zone.state = fetchTransportState(coordinator->ip);
    out.push_back(zone);
  }

  return !out.empty();
}

sonos::TransportState fetchTransportState(const std::string& ip) {
  return sonos::parseTransportState(
      soap(ip, kAvTransportPath, kAvTransportService, "GetTransportInfo"));
}

sonos::TrackInfo fetchPositionInfo(const std::string& ip) {
  return sonos::parsePositionInfo(
      soap(ip, kAvTransportPath, kAvTransportService, "GetPositionInfo"));
}

bool play(const std::string& ip) {
  return !soap(ip, kAvTransportPath, kAvTransportService, "Play",
               "<InstanceID>0</InstanceID><Speed>1</Speed>")
              .empty();
}

bool pause(const std::string& ip) {
  return !soap(ip, kAvTransportPath, kAvTransportService, "Pause").empty();
}

bool next(const std::string& ip) {
  return !soap(ip, kAvTransportPath, kAvTransportService, "Next").empty();
}

bool previous(const std::string& ip) {
  return !soap(ip, kAvTransportPath, kAvTransportService, "Previous").empty();
}

}  // namespace sonos_client
