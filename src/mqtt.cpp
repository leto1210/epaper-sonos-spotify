#include "mqtt.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "core/version.h"

namespace mqtt {
namespace {

// Un message de découverte tient dans ~700 octets ; le tampon de PubSubClient
// n'en fait que 256 par défaut, et une publication trop grande échoue
// silencieusement. C'est le piège classique de MQTT Discovery.
constexpr uint16_t kBufferSize = 1024;

constexpr uint32_t kReconnectIntervalMs = 10000;

// Un rafraîchissement de l'ePaper bloque 37 s, pendant lesquelles personne
// n'entretient la session MQTT. Avec les 15 s de keepalive par défaut, le
// broker déclarait le boîtier mort à chaque redessin : testament publié,
// entités en `unavailable`, puis reconnexion — un clignotement visible dans
// Home Assistant. Mesuré sur cible.
constexpr uint16_t kKeepAliveS = 90;

WiFiClient g_socket;
PubSubClient g_client(g_socket);
ha::Device g_device;

bool g_enabled = false;
bool g_discovery_sent = false;
uint32_t g_last_attempt_ms = 0;

weather::Report g_weather;

// Le payload arrive en octets bruts, sans terminateur : PubSubClient réutilise
// son tampon d'un message à l'autre. On copie avant d'analyser.
void onMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (ha::weatherTopic(g_device) != topic) return;

  const std::string json(reinterpret_cast<const char*>(payload), length);
  const weather::Report report = weather::parse(json);
  if (!report.valid) {
    Serial.printf("[mqtt] meteo illisible (%u octets)\n", length);
    return;
  }

  g_weather = report;
  Serial.printf("[mqtt] meteo recue : %s, %.1f C, %u creneaux\n",
                weather::conditionLabel(report.condition), report.temperature_c,
                static_cast<unsigned>(report.hourly.size()));
}

void publishDiscovery() {
  int published = 0;
  for (const ha::Entity& entity : ha::entities()) {
    const std::string topic = ha::configTopic(g_device, entity);
    const std::string payload = ha::discoveryPayload(g_device, entity);
    // `retain` : les entités survivent à un redémarrage de Home Assistant sans
    // attendre le prochain réveil du boîtier.
    if (g_client.publish(topic.c_str(), payload.c_str(), true)) ++published;
    else Serial.printf("[mqtt] echec de publication : %s\n", topic.c_str());
  }
  Serial.printf("[mqtt] decouverte : %d/%d entites\n", published,
                static_cast<int>(ha::entities().size()));
}

bool connect() {
  const std::string status = ha::statusTopic(g_device);

  // Le testament est déposé à la connexion : c'est le broker qui publiera
  // « offline » si le boîtier disparaît sans prévenir.
  const bool ok = g_client.connect(g_device.id.c_str(), MQTT_USER, MQTT_PASSWORD,
                                   status.c_str(), 0, true, "offline");
  if (!ok) {
    Serial.printf("[mqtt] connexion refusee (code %d)\n", g_client.state());
    return false;
  }

  Serial.printf("[mqtt] connecte a %s\n", MQTT_HOST);
  g_client.publish(status.c_str(), "online", true);

  // À réabonner à chaque connexion : la session n'est pas persistante. Le
  // sujet étant retenu, la météo courante arrive dans la foulée.
  g_client.subscribe(ha::weatherTopic(g_device).c_str());

  if (!g_discovery_sent) {
    publishDiscovery();
    g_discovery_sent = true;
  }
  return true;
}

bool publish(const std::string& topic, const std::string& payload) {
  if (!isConnected()) return false;
  return g_client.publish(topic.c_str(), payload.c_str(), true);
}

}  // namespace

void begin() {
  g_device.sw_version = epaper_spotify::kFirmwareVersion;

  if (sizeof(MQTT_HOST) <= 1) {
    Serial.println("[mqtt] desactive (MQTT_HOST vide)");
    return;
  }

  g_enabled = true;
  g_client.setServer(MQTT_HOST, MQTT_PORT);
  g_client.setBufferSize(kBufferSize);
  g_client.setKeepAlive(kKeepAliveS);
  g_client.setCallback(onMessage);
}

void loop() {
  if (!g_enabled) return;

  if (g_client.connected()) {
    g_client.loop();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) return;

  // Réessai espacé : une tentative par tour de boucle bloquerait l'affichage
  // pendant tout le délai d'attente TCP si le broker est absent.
  const uint32_t now = millis();
  if (g_last_attempt_ms != 0 && now - g_last_attempt_ms < kReconnectIntervalMs) return;
  g_last_attempt_ms = now;
  connect();
}

bool isConnected() { return g_enabled && g_client.connected(); }

void publishState(const ha::State& state) {
  publish(ha::stateTopic(g_device), ha::statePayload(state));
}

void publishTrack(const ha::Track& track) {
  publish(ha::trackTopic(g_device), ha::trackPayload(track));
}

const weather::Report& weatherReport() { return g_weather; }

}  // namespace mqtt
