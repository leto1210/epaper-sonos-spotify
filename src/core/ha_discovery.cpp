#include "core/ha_discovery.h"

#include <ArduinoJson.h>

#include <cmath>

namespace ha {
namespace {

void addIfSet(JsonDocument& doc, const char* key, const std::string& value) {
  if (!value.empty()) doc[key] = value;
}

}  // namespace

std::string statusTopic(const Device& device) { return device.id + "/status"; }
std::string stateTopic(const Device& device) { return device.id + "/state"; }
std::string trackTopic(const Device& device) { return device.id + "/track"; }
std::string weatherTopic(const Device& device) { return device.id + "/weather"; }

std::string configTopic(const Device& device, const Entity& entity) {
  return "homeassistant/" + entity.component + "/" + device.id + "/" + entity.object_id +
         "/config";
}

std::vector<Entity> entities() {
  std::vector<Entity> list;

  Entity battery;
  battery.object_id = "batterie";
  battery.component = "sensor";
  battery.name = "Batterie";
  battery.device_class = "battery";
  battery.state_class = "measurement";
  battery.unit = "%";
  battery.value_template = "{{ value_json.bat }}";
  list.push_back(battery);

  Entity charging;
  charging.object_id = "en_charge";
  charging.component = "binary_sensor";
  charging.name = "En charge";
  charging.device_class = "battery_charging";
  charging.value_template = "{{ 'ON' if value_json.chg else 'OFF' }}";
  list.push_back(charging);

  Entity temperature;
  temperature.object_id = "temperature";
  temperature.component = "sensor";
  temperature.name = "Température";
  temperature.device_class = "temperature";
  temperature.state_class = "measurement";
  temperature.unit = "°C";
  temperature.value_template = "{{ value_json.temp }}";
  list.push_back(temperature);

  Entity humidity;
  humidity.object_id = "humidite";
  humidity.component = "sensor";
  humidity.name = "Humidité";
  humidity.device_class = "humidity";
  humidity.state_class = "measurement";
  humidity.unit = "%";
  humidity.value_template = "{{ value_json.hum }}";
  list.push_back(humidity);

  Entity rssi;
  rssi.object_id = "signal";
  rssi.component = "sensor";
  rssi.name = "Signal Wi-Fi";
  rssi.device_class = "signal_strength";
  rssi.state_class = "measurement";
  rssi.unit = "dBm";
  rssi.value_template = "{{ value_json.rssi }}";
  rssi.entity_category = "diagnostic";
  list.push_back(rssi);

  Entity uptime;
  uptime.object_id = "uptime";
  uptime.component = "sensor";
  uptime.name = "Temps de fonctionnement";
  uptime.device_class = "duration";
  uptime.unit = "s";
  uptime.value_template = "{{ value_json.up }}";
  uptime.entity_category = "diagnostic";
  list.push_back(uptime);

  Entity last_refresh;
  last_refresh.object_id = "dernier_rafraichissement";
  last_refresh.component = "sensor";
  last_refresh.name = "Dernier rafraîchissement";
  last_refresh.device_class = "timestamp";
  last_refresh.value_template = "{{ value_json.last }}";
  last_refresh.entity_category = "diagnostic";
  list.push_back(last_refresh);

  // Garde-fou anti-régression : si ce compteur grimpe alors que la musique ne
  // change pas, l'écran se redessine pour rien — 37 s et de l'usure à chaque
  // fois. Voir docs/architecture.md.
  Entity refreshes;
  refreshes.object_id = "rafraichissements";
  refreshes.component = "sensor";
  refreshes.name = "Rafraîchissements";
  refreshes.state_class = "total_increasing";
  refreshes.value_template = "{{ value_json.refresh }}";
  refreshes.entity_category = "diagnostic";
  refreshes.icon = "mdi:refresh";
  list.push_back(refreshes);

  Entity track;
  track.object_id = "morceau";
  track.component = "sensor";
  track.name = "Morceau en cours";
  track.value_template = "{{ value_json.title }}";
  track.icon = "mdi:music-note";
  track.state_topic_suffix = "/track";
  track.json_attributes = true;
  list.push_back(track);

  return list;
}

std::string discoveryPayload(const Device& device, const Entity& entity) {
  JsonDocument doc;

  doc["name"] = entity.name;
  doc["unique_id"] = device.id + "_" + entity.object_id;
  doc["object_id"] = device.id + "_" + entity.object_id;

  const std::string state_topic = entity.state_topic_suffix.empty()
                                      ? stateTopic(device)
                                      : device.id + entity.state_topic_suffix;
  doc["state_topic"] = state_topic;
  if (entity.json_attributes) doc["json_attributes_topic"] = state_topic;

  addIfSet(doc, "device_class", entity.device_class);
  addIfSet(doc, "state_class", entity.state_class);
  addIfSet(doc, "unit_of_measurement", entity.unit);
  addIfSet(doc, "value_template", entity.value_template);
  addIfSet(doc, "entity_category", entity.entity_category);
  addIfSet(doc, "icon", entity.icon);

  // Sans disponibilité, un boîtier débranché laisserait ses dernières valeurs
  // en place indéfiniment, sans rien signaler.
  doc["availability_topic"] = statusTopic(device);
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"][0] = device.id;
  dev["name"] = device.name;
  dev["manufacturer"] = "Seeed Studio";
  dev["model"] = "reTerminal E1002";
  if (!device.sw_version.empty()) dev["sw_version"] = device.sw_version;

  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string statePayload(const State& state) {
  JsonDocument doc;

  // Une mesure absente est omise, jamais remplacée par un zéro : Home
  // Assistant affichera « inconnu », ce qui est la vérité.
  if (state.battery_pct >= 0) {
    doc["bat"] = state.battery_pct;
    doc["mv"] = state.battery_mv;
  }
  doc["chg"] = state.charging;

  if (state.has_climate) {
    // Le SHT4x renvoie 28,30587 °C. Le dixième de degré est déjà au-delà de sa
    // précision réelle ; publier tout le reste ne fait qu'encombrer l'historique.
    doc["temp"] = std::round(state.temperature_c * 10.0f) / 10.0f;
    doc["hum"] = state.humidity_pct;
  }

  doc["rssi"] = state.rssi_dbm;
  doc["up"] = state.uptime_s;
  doc["refresh"] = state.refresh_count;
  if (!state.last_refresh_iso.empty()) doc["last"] = state.last_refresh_iso;

  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string trackPayload(const Track& track) {
  JsonDocument doc;
  doc["title"] = track.title;
  doc["artist"] = track.artist;
  doc["album"] = track.album;
  doc["zone"] = track.zone;
  doc["playing"] = track.playing;
  addIfSet(doc, "art_url", track.art_url);

  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace ha
