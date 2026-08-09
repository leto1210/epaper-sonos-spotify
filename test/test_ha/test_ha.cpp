// Tests des payloads MQTT Discovery. Aucun broker requis : on vérifie le
// contrat que Home Assistant lira.
#include <ArduinoJson.h>
#include <unity.h>

#include <set>
#include <string>

#include "core/ha_discovery.h"

namespace {

ha::Device device() {
  ha::Device dev;
  dev.sw_version = "0.11.0";
  return dev;
}

ha::Entity entityNamed(const std::string& object_id) {
  for (const ha::Entity& entity : ha::entities()) {
    if (entity.object_id == object_id) return entity;
  }
  TEST_FAIL_MESSAGE("entité introuvable");
  return {};
}

JsonDocument parsed(const std::string& json) {
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, json));
  return doc;
}

}  // namespace

void test_config_topic_follows_discovery_convention() {
  const ha::Entity battery = entityNamed("batterie");
  TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/reterminal_sonos/batterie/config",
                           ha::configTopic(device(), battery).c_str());
}

// Un identifiant dupliqué ferait disparaître silencieusement une entité dans
// Home Assistant : la seconde écraserait la première.
void test_unique_ids_are_unique() {
  std::set<std::string> seen;
  for (const ha::Entity& entity : ha::entities()) {
    const JsonDocument doc = parsed(ha::discoveryPayload(device(), entity));
    const std::string unique_id = doc["unique_id"].as<std::string>();
    TEST_ASSERT_FALSE_MESSAGE(seen.count(unique_id) > 0, unique_id.c_str());
    seen.insert(unique_id);
  }
  TEST_ASSERT_EQUAL_INT(11, static_cast<int>(seen.size()));
}

// C'est ce bloc, identique partout, qui regroupe les entités sous un seul
// appareil au lieu de neuf appareils orphelins.
void test_every_entity_shares_the_same_device_block() {
  for (const ha::Entity& entity : ha::entities()) {
    const JsonDocument doc = parsed(ha::discoveryPayload(device(), entity));
    TEST_ASSERT_EQUAL_STRING("reterminal_sonos",
                             doc["device"]["identifiers"][0].as<std::string>().c_str());
    TEST_ASSERT_EQUAL_STRING("ePaper Sonos",
                             doc["device"]["name"].as<std::string>().c_str());
    TEST_ASSERT_EQUAL_STRING("reterminal_sonos/status",
                             doc["availability_topic"].as<std::string>().c_str());
  }
}

void test_track_entity_carries_attributes_on_its_own_topic() {
  const JsonDocument doc = parsed(ha::discoveryPayload(device(), entityNamed("morceau")));
  TEST_ASSERT_EQUAL_STRING("reterminal_sonos/track",
                           doc["state_topic"].as<std::string>().c_str());
  TEST_ASSERT_EQUAL_STRING("reterminal_sonos/track",
                           doc["json_attributes_topic"].as<std::string>().c_str());
}

// Les mesures partagent un sujet unique : une seule publication, et des
// valeurs cohérentes entre elles.
void test_measurements_share_one_state_topic() {
  for (const ha::Entity& entity : ha::entities()) {
    if (entity.object_id == "morceau") continue;
    if (entity.component == "button") continue;  // un bouton n'a pas d'état
    const JsonDocument doc = parsed(ha::discoveryPayload(device(), entity));
    TEST_ASSERT_EQUAL_STRING("reterminal_sonos/state",
                             doc["state_topic"].as<std::string>().c_str());
  }
}

void test_state_payload_carries_the_measurements() {
  ha::State state;
  state.battery_pct = 87;
  state.battery_mv = 4020;
  state.charging = false;
  state.has_climate = true;
  state.temperature_c = 21.4f;
  state.humidity_pct = 48;
  state.rssi_dbm = -63;
  state.uptime_s = 3600;
  state.refresh_count = 3;
  state.last_refresh_iso = "2026-08-09T19:04:57+02:00";

  const JsonDocument doc = parsed(ha::statePayload(state));
  TEST_ASSERT_EQUAL_INT(87, doc["bat"].as<int>());
  TEST_ASSERT_EQUAL_INT(4020, doc["mv"].as<int>());
  TEST_ASSERT_FALSE(doc["chg"].as<bool>());
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.4f, doc["temp"].as<float>());
  TEST_ASSERT_EQUAL_INT(48, doc["hum"].as<int>());
  TEST_ASSERT_EQUAL_INT(-63, doc["rssi"].as<int>());
  TEST_ASSERT_EQUAL_INT(3, doc["refresh"].as<int>());
}

// Même exigence que sur l'écran : une mesure ratée ne doit pas se déguiser en
// zéro. Ici, la clé est simplement absente et Home Assistant affiche
// « inconnu ».
void test_missing_measurements_are_omitted_not_zeroed() {
  ha::State state;  // ni accu ni SHT4x
  const JsonDocument doc = parsed(ha::statePayload(state));
  TEST_ASSERT_FALSE(doc["bat"].is<int>());
  TEST_ASSERT_FALSE(doc["temp"].is<float>());
  TEST_ASSERT_FALSE(doc["hum"].is<int>());
  TEST_ASSERT_FALSE(doc["last"].is<std::string>());
}

// Les titres contiennent des guillemets et des accents ; le payload doit
// rester du JSON valide.
void test_track_payload_escapes_special_characters() {
  ha::Track track;
  track.title = "Le \"Bal\" masqué";
  track.artist = "La Compagnie Créole";
  track.album = "Ça fait rire les oiseaux";
  track.zone = "Sonos Séjour";
  track.playing = true;

  const JsonDocument doc = parsed(ha::trackPayload(track));
  TEST_ASSERT_EQUAL_STRING("Le \"Bal\" masqué", doc["title"].as<std::string>().c_str());
  TEST_ASSERT_EQUAL_STRING("La Compagnie Créole", doc["artist"].as<std::string>().c_str());
  TEST_ASSERT_TRUE(doc["playing"].as<bool>());
}

// --- Entités pilotables ------------------------------------------------------

// Un `button` sans état resterait indisponible s'il déclarait un `state_topic`
// sur lequel rien n'arrive jamais.
void test_refresh_button_declares_only_a_command_topic() {
  const JsonDocument doc =
      parsed(ha::discoveryPayload(device(), entityNamed("rafraichir")));
  TEST_ASSERT_EQUAL_STRING("reterminal_sonos/cmd/refresh",
                           doc["command_topic"].as<std::string>().c_str());
  TEST_ASSERT_FALSE(doc["state_topic"].is<std::string>());
}

// La topologie n'est pas connue à la première connexion : le sélecteur ne
// propose alors qu'`auto`, puis sa découverte est republiée avec les vrais
// noms de pièces.
void test_zone_select_lists_auto_then_the_real_zones() {
  const JsonDocument empty =
      parsed(ha::discoveryPayload(device(), entityNamed("zone")));
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(empty["options"].size()));
  TEST_ASSERT_EQUAL_STRING("auto", empty["options"][0].as<std::string>().c_str());

  ha::Entity zone;
  for (const ha::Entity& candidate : ha::entities({"Sonos Séjour", "Sonos Beam"})) {
    if (candidate.object_id == "zone") zone = candidate;
  }
  const JsonDocument doc = parsed(ha::discoveryPayload(device(), zone));
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(doc["options"].size()));
  TEST_ASSERT_EQUAL_STRING("auto", doc["options"][0].as<std::string>().c_str());
  TEST_ASSERT_EQUAL_STRING("Sonos Séjour", doc["options"][1].as<std::string>().c_str());
}

void test_command_topics_are_the_two_writable_entities() {
  const std::vector<std::string> topics = ha::commandTopics(device());
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(topics.size()));
  TEST_ASSERT_EQUAL_STRING("reterminal_sonos/cmd/refresh", topics[0].c_str());
  TEST_ASSERT_EQUAL_STRING("reterminal_sonos/cmd/zone", topics[1].c_str());
}

void test_parses_incoming_commands() {
  const ha::Device dev = device();

  TEST_ASSERT_EQUAL(ha::CommandKind::kRefresh,
                    ha::parseCommand(dev, "reterminal_sonos/cmd/refresh", "PRESS").kind);

  const ha::Command zone =
      ha::parseCommand(dev, "reterminal_sonos/cmd/zone", "Sonos Cuisine");
  TEST_ASSERT_EQUAL(ha::CommandKind::kSelectZone, zone.kind);
  TEST_ASSERT_EQUAL_STRING("Sonos Cuisine", zone.zone.c_str());
}

// Un message qu'on ne comprend pas ne doit rien déclencher : ni redessin de
// 37 s, ni bascule vers une zone inexistante.
void test_ignores_unknown_or_empty_commands() {
  const ha::Device dev = device();
  TEST_ASSERT_EQUAL(ha::CommandKind::kNone,
                    ha::parseCommand(dev, "reterminal_sonos/weather", "{}").kind);
  TEST_ASSERT_EQUAL(ha::CommandKind::kNone,
                    ha::parseCommand(dev, "autre_appareil/cmd/refresh", "PRESS").kind);
  TEST_ASSERT_EQUAL(ha::CommandKind::kNone,
                    ha::parseCommand(dev, "reterminal_sonos/cmd/zone", "").kind);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_config_topic_follows_discovery_convention);
  RUN_TEST(test_unique_ids_are_unique);
  RUN_TEST(test_every_entity_shares_the_same_device_block);
  RUN_TEST(test_track_entity_carries_attributes_on_its_own_topic);
  RUN_TEST(test_measurements_share_one_state_topic);
  RUN_TEST(test_state_payload_carries_the_measurements);
  RUN_TEST(test_missing_measurements_are_omitted_not_zeroed);
  RUN_TEST(test_track_payload_escapes_special_characters);
  RUN_TEST(test_refresh_button_declares_only_a_command_topic);
  RUN_TEST(test_zone_select_lists_auto_then_the_real_zones);
  RUN_TEST(test_command_topics_are_the_two_writable_entities);
  RUN_TEST(test_parses_incoming_commands);
  RUN_TEST(test_ignores_unknown_or_empty_commands);
  return UNITY_END();
}
