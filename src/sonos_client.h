#pragma once

#include <string>
#include <vector>

#include "core/sonos_parser.h"
#include "core/zone_picker.h"

// Accès réseau aux enceintes Sonos : requêtes SOAP sur le port 1400. Cette
// couche ne décide de rien — elle interroge, et délègue l'analyse à
// `core/sonos_parser` et le choix de zone à `core/zone_picker`.
namespace sonos_client {

// Lit la topologie depuis n'importe quelle enceinte, puis interroge l'état de
// chaque coordinateur. C'est l'opération coûteuse : une requête par groupe.
//
// La découverte SSDP n'est pas utilisée : elle repose sur du multicast, qui ne
// franchit pas un routeur, et le boîtier est en pratique sur un VLAN distinct
// de celui des enceintes. Une seule adresse d'amorçage suffit.
bool fetchZones(const std::string& seed_ip, std::vector<sonos::ZoneStatus>& out);

sonos::TransportState fetchTransportState(const std::string& ip);
sonos::TrackInfo fetchPositionInfo(const std::string& ip);

// Commandes de transport, à envoyer au coordinateur du groupe.
bool play(const std::string& ip);
bool pause(const std::string& ip);
bool next(const std::string& ip);
bool previous(const std::string& ip);

}  // namespace sonos_client
