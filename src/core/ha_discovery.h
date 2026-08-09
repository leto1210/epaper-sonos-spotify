#pragma once

#include <string>
#include <vector>

// Construction des messages MQTT Discovery de Home Assistant et des payloads
// d'état. Pur C++ : aucun accès réseau ici, ce qui rend les payloads
// vérifiables au caractère près par les tests natifs.
//
// Deux sujets d'état seulement, plutôt qu'un par entité : Home Assistant
// extrait chaque valeur par `value_template`. Une publication au lieu de neuf,
// et surtout des mesures cohérentes entre elles.
namespace ha {

struct Device {
  std::string id = "reterminal_sonos";
  std::string name = "ePaper Sonos";
  std::string sw_version;
};

struct Entity {
  std::string object_id;  // suffixe du topic et de l'`unique_id`
  std::string component;  // "sensor", "binary_sensor", …
  std::string name;
  std::string device_class;
  std::string state_class;
  std::string unit;
  std::string value_template;
  std::string entity_category;  // "diagnostic" pour ce qui n'intéresse pas au quotidien
  std::string icon;

  // Vide = sujet d'état commun. Seul le morceau en cours a le sien, parce
  // qu'il porte des attributs (artiste, album, zone).
  std::string state_topic_suffix;
  bool json_attributes = false;
};

// Sujets, tous préfixés par l'identifiant de l'appareil.
std::string statusTopic(const Device& device);   // LWT : online / offline
std::string stateTopic(const Device& device);    // mesures
std::string trackTopic(const Device& device);    // morceau en cours + attributs
std::string weatherTopic(const Device& device);  // publié par Home Assistant

std::string configTopic(const Device& device, const Entity& entity);

// Les entités publiées à la connexion, dans l'ordre d'apparition.
std::vector<Entity> entities();

std::string discoveryPayload(const Device& device, const Entity& entity);

struct State {
  int battery_pct = -1;  // négatif : mesure absente, la valeur est omise
  int battery_mv = 0;
  bool charging = false;

  bool has_climate = false;
  float temperature_c = 0.0f;
  int humidity_pct = 0;

  int rssi_dbm = 0;
  long uptime_s = 0;
  int refresh_count = 0;
  std::string last_refresh_iso;  // vide tant que rien n'a été affiché
};

std::string statePayload(const State& state);

struct Track {
  std::string title;
  std::string artist;
  std::string album;
  std::string zone;
  std::string art_url;
  bool playing = false;
};

std::string trackPayload(const Track& track);

}  // namespace ha
