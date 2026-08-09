// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 6 : affichage du morceau en cours. Pochettes et capteurs suivent.
#include <Arduino.h>
#include <WiFi.h>

#include <vector>

#include "albumart.h"
#include "buttons_io.h"
#include "config.h"
#include "core/version.h"
#include "core/layout_plan.h"
#include "core/pause_timer.h"
#include "core/weather_view.h"
#include "core/zone_picker.h"
#include "display.h"
#include "mqtt.h"
#include "sensors.h"
#include "sonos_client.h"
#include "wifi_mgr.h"

namespace {

const std::vector<std::string> kZonePriority = SONOS_ZONE_PRIORITY;

const char* stateLabel(sonos::TransportState state) {
  switch (state) {
    case sonos::TransportState::kPlaying: return "joue";
    case sonos::TransportState::kPaused: return "en pause";
    case sonos::TransportState::kStopped: return "arrete";
    case sonos::TransportState::kTransitioning: return "transition";
    case sonos::TransportState::kUnknown: break;
  }
  return "inconnu";
}

// Empreinte de ce qui est affiché. Un rafraîchissement coûte 37 s : on ne
// redessine que si l'un de ces éléments change réellement.
std::string g_shown_fingerprint;
std::string g_last_zone;

// Coordinateur de la zone affichée : c'est à lui, et à lui seul, que les
// commandes de transport doivent être adressées.
std::string g_last_ip;

// Zone imposée depuis Home Assistant, ou `auto` pour laisser la politique de
// choix décider. Non conservée au redémarrage : l'automatique est le mode
// nominal, et retrouver une pièce figée après une coupure de courant serait
// plus déroutant qu'utile.
std::string g_forced_zone = ha::kAutoZone;

idle::PauseTimer g_pause_timer;

// Horodatage du dernier rafraîchissement, au format attendu par Home Assistant
// (`device_class: timestamp`). Vide tant que l'heure n'a pas été synchronisée :
// mieux vaut pas de date qu'une date de 1970.
std::string g_last_refresh_iso;

std::string nowIso8601() {
  struct tm local;
  if (!getLocalTime(&local, 0)) return {};
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &local);
  return buffer;
}

// Remonte les mesures à Home Assistant, indépendamment de l'écran : celui-ci
// peut rester figé une heure sur le même morceau sans que la batterie ni la
// température cessent d'être suivies.
void publishMeasurements(const sensors::Reading& measures) {
  if (!mqtt::isConnected()) return;

  ha::State state;
  state.battery_pct = measures.battery_pct;
  state.battery_mv = measures.battery_mv;
  state.charging = measures.charging;
  state.has_climate = measures.has_climate;
  state.temperature_c = measures.temperature_c;
  state.humidity_pct = measures.humidity_pct;
  state.rssi_dbm = WiFi.RSSI();
  state.uptime_s = millis() / 1000;
  state.refresh_count = static_cast<int>(display::refreshCount());
  state.last_refresh_iso = g_last_refresh_iso;
  state.selected_zone = g_forced_zone;

  mqtt::publishState(state);
}

// Applique une commande de transport à la zone affichée. Le rafraîchissement
// n'est pas déclenché ici : il attend que la rafale d'appuis soit retombée.
void sendTransport(buttons::Action action) {
  if (g_last_ip.empty()) {
    Serial.println("[boutons] aucune zone connue, commande ignoree");
    return;
  }

  bool ok = false;
  switch (action) {
    case buttons::Action::kNext:
      ok = sonos_client::next(g_last_ip);
      Serial.printf("[boutons] suivant sur %s : %s\n", g_last_zone.c_str(),
                    ok ? "ok" : "echec");
      break;
    case buttons::Action::kPrevious:
      ok = sonos_client::previous(g_last_ip);
      Serial.printf("[boutons] precedent sur %s : %s\n", g_last_zone.c_str(),
                    ok ? "ok" : "echec");
      break;
    case buttons::Action::kPlayPause: {
      // L'état est relu plutôt que déduit de l'affichage : celui-ci peut dater
      // de plusieurs minutes, et la musique a pu être pilotée depuis le
      // téléphone entre-temps.
      const bool playing =
          sonos_client::fetchTransportState(g_last_ip) == sonos::TransportState::kPlaying;
      ok = playing ? sonos_client::pause(g_last_ip) : sonos_client::play(g_last_ip);
      Serial.printf("[boutons] %s sur %s : %s\n", playing ? "pause" : "lecture",
                    g_last_zone.c_str(), ok ? "ok" : "echec");
      break;
    }
    case buttons::Action::kForceRedraw:
      // Oublier l'empreinte suffit : le prochain rendu se fera sans condition.
      g_shown_fingerprint.clear();
      Serial.println("[boutons] redessin force");
      break;
    case buttons::Action::kNone:
      break;
  }
}

// Un rendu dure 37 s. Le déclencher depuis la fonction de rappel MQTT
// bloquerait la pile réseau au milieu du traitement d'un message : on note la
// demande, et la boucle principale s'en charge.
bool g_pending_render = false;

void onHomeAssistantCommand(const ha::Command& command) {
  switch (command.kind) {
    case ha::CommandKind::kRefresh:
      Serial.println("[ha] rafraichissement demande");
      break;
    case ha::CommandKind::kSelectZone:
      Serial.printf("[ha] zone imposee : %s\n", command.zone.c_str());
      g_forced_zone = command.zone;
      break;
    case ha::CommandKind::kNone:
      return;
  }

  // Oublier l'empreinte suffit à rendre le prochain rendu inconditionnel.
  g_shown_fingerprint.clear();
  g_pending_render = true;
}

// Écran de repli quand rien ne joue. Même politique que pour le morceau : une
// empreinte, et pas de rafraîchissement si rien n'a bougé.
void renderWeather() {
  const sensors::Reading measures = sensors::read();
  const weather::View view = weather::plan(mqtt::weatherReport(), time(nullptr),
                                           measures.has_climate, measures.temperature_c,
                                           measures.humidity_pct);

  // L'empreinte ne retient que ce qui se voit : deux relevés successifs à
  // 32,58 et 32,62 °C ne valent pas 37 s de rafraîchissement.
  std::string fingerprint = "meteo|" + view.condition_label + "|" + view.temperature;
  for (const weather::Column& column : view.columns) {
    fingerprint += "|" + column.hour + column.temperature;
  }
  if (fingerprint == g_shown_fingerprint) return;

  display::Status status;
  status.indoor_temperature_c = measures.temperature_c;
  status.indoor_humidity_pct = measures.has_climate ? measures.humidity_pct : 0;
  status.battery_pct = measures.battery_pct;

  display::showWeather(view, status);
  g_shown_fingerprint = fingerprint;
  g_last_refresh_iso = nowIso8601();
  publishMeasurements(measures);
}

void pollAndRender() {
  std::vector<sonos::ZoneStatus> zones;
  const uint32_t started = millis();

  if (!sonos_client::fetchZones(SONOS_SEED_IP, zones)) {
    Serial.println("[sonos] topologie indisponible");
    return;
  }

  Serial.printf("[sonos] %u zone(s) en %lu ms\n", static_cast<unsigned>(zones.size()),
                millis() - started);
  for (const sonos::ZoneStatus& zone : zones) {
    Serial.printf("[sonos]   %-24s %-15s %s\n", zone.name.c_str(), zone.ip.c_str(),
                  stateLabel(zone.state));
  }

  // Le sélecteur de Home Assistant propose les pièces réelles, pas celles de
  // la configuration : « Séjour » s'y appelle « Sonos Séjour ».
  std::vector<std::string> names;
  for (const sonos::ZoneStatus& zone : zones) names.push_back(zone.name);
  mqtt::publishZoneOptions(names);

  // On descend le classement jusqu'à une zone qui sait vraiment ce qu'elle
  // joue. Après un basculement Spotify Connect, l'enceinte précédente garde un
  // état PLAYING résiduel sans métadonnées : s'arrêter au premier candidat
  // laissait l'écran figé sur cette zone fantôme.
  // `forced_zone` court-circuite la politique de choix : c'est le sélecteur de
  // Home Assistant. Une zone forcée introuvable ne retombe pas sur une autre
  // pièce — l'utilisateur a demandé celle-là — et l'écran passe à la météo.
  const std::vector<sonos::Choice> ranked =
      sonos::rankZones(zones, kZonePriority, g_last_zone, g_forced_zone);

  if (ranked.empty()) {
    Serial.println("[sonos] aucune zone a afficher, ecran meteo");
    renderWeather();
    return;
  }

  sonos::Choice choice;
  sonos::TrackInfo track;
  for (const sonos::Choice& candidate : ranked) {
    track = sonos_client::fetchPositionInfo(candidate.ip);
    if (track.has_metadata) {
      choice = candidate;
      break;
    }
    const char* raison = "ne sait rien";
    switch (sonos::sourceKind(track.track_uri)) {
      case sonos::SourceKind::kTvInput: raison = "diffuse la television"; break;
      case sonos::SourceKind::kLineIn: raison = "est sur son entree ligne"; break;
      case sonos::SourceKind::kSlave: raison = "est esclave d'un groupe"; break;
      default: break;
    }
    Serial.printf("[sonos] %s ecarte : %s\n", candidate.name.c_str(), raison);
  }

  if (!choice.found) {
    // Aucune zone ne sait quoi que ce soit : la télévision, une entrée ligne ou
    // simplement le silence. L'écran passe à la météo.
    Serial.println("[sonos] aucune metadonnee nulle part, ecran meteo");
    renderWeather();
    return;
  }
  g_last_zone = choice.name;
  g_last_ip = choice.ip;

  Serial.printf("[sonos] zone retenue : %s (%s), %s\n", choice.name.c_str(),
                choice.ip.c_str(), choice.playing ? "en lecture" : "en pause");


  Serial.printf("[sonos] %s - %s [%s] %d/%d s\n", track.artist.c_str(),
                track.title.c_str(), track.album.c_str(), track.position_s,
                track.duration_s);

  // Publié avant le test d'empreinte : Home Assistant suit la lecture au
  // rythme du sondage, sans attendre les 37 s d'un rafraîchissement d'écran.
  if (mqtt::isConnected()) {
    ha::Track published;
    published.title = track.title;
    published.artist = track.artist;
    published.album = track.album;
    published.zone = choice.name;
    published.art_url = track.art_uri;
    published.playing = choice.playing;
    mqtt::publishTrack(published);
  }

  // Une pause qui dure n'informe plus de rien : l'écran rend la place à la
  // météo. Le test vient après la publication : Home Assistant continue de
  // suivre le morceau, seul l'affichage change.
  if (g_pause_timer.expired(millis(), choice.playing, choice.name)) {
    Serial.println("[sonos] en pause depuis plus de 5 min, ecran meteo");
    renderWeather();
    return;
  }

  const std::string fingerprint =
      track.track_uri + "|" + track.title + "|" + choice.name +
      (choice.playing ? "|1" : "|0");
  if (fingerprint == g_shown_fingerprint) {
    Serial.println("[ecran] inchange, pas de rafraichissement");
    return;
  }

  // La pochette n'est chargée qu'une fois le redessin décidé : c'est
  // 200 Ko de réseau et une seconde de décodage, inutiles si l'écran ne
  // change pas.
  const int art_size = layout::planTrack(track.title, track.artist, track.album,
                                         display::measureText)
                           .art_size_px;
  const albumart::Bitmap art =
      albumart::load(sonos::albumArtUrl(choice.ip, track), art_size);
  if (!art.valid()) {
    Serial.println("[pochette] indisponible, emplacement de repli");
  }

  const sensors::Reading measures = sensors::read();
  Serial.printf("[capteurs] %.1f C  %d%%HR  accu %d mV (%d%%)%s\n",
                measures.temperature_c, measures.humidity_pct, measures.battery_mv,
                measures.battery_pct, measures.charging ? " en charge" : "");

  display::Status status;
  status.zone = choice.name;
  status.playing = choice.playing;
  status.indoor_temperature_c = measures.temperature_c;
  status.indoor_humidity_pct = measures.has_climate ? measures.humidity_pct : 0;
  status.battery_pct = measures.battery_pct;

  display::showTrack(track, status, art);
  g_shown_fingerprint = fingerprint;
  g_last_refresh_iso = nowIso8601();
  publishMeasurements(measures);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(2000);  // laisse le temps au pont CH340 de s'ouvrir
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());

  display::begin();
  sensors::begin();
  buttons_io::begin();
  mqtt::begin(onHomeAssistantCommand);

  const bool online = wifi_mgr::connect();
  if (online) {
    wifi_mgr::syncTime();
  } else {
    display::showBootScreen("Wi-Fi indisponible");
  }

  Serial.printf("[boot] termine, %lu rafraichissement(s)\n",
                static_cast<unsigned long>(display::refreshCount()));
}

void loop() {
  wifi_mgr::loop();
  mqtt::loop();

  // Les mesures partent toutes les 5 minutes, quoi qu'affiche l'écran.
  static uint32_t last_publish_ms = 0;
  if (mqtt::isConnected() &&
      (last_publish_ms == 0 || millis() - last_publish_ms >= 300000UL)) {
    last_publish_ms = millis();
    publishMeasurements(sensors::read());
  }

  // Les boutons passent avant le sondage : une commande doit partir dans la
  // seconde, alors qu'un sondage peut attendre le tour suivant.
  const buttons::Action action = buttons_io::poll();
  if (action != buttons::Action::kNone) sendTransport(action);

  static uint32_t last_poll_ms = 0;
  static bool first_poll_done = false;

  // Demande venue de Home Assistant, traitée hors de la fonction de rappel MQTT.
  if (g_pending_render && wifi_mgr::isConnected()) {
    g_pending_render = false;

    // L'accusé de réception part avant le rendu : sans cela, le sélecteur
    // resterait sur son ancienne valeur dans Home Assistant pendant les 37 s du
    // rafraîchissement, et jusqu'à la publication périodique suivante.
    publishMeasurements(sensors::read());

    last_poll_ms = millis();
    first_poll_done = true;
    pollAndRender();
    return;
  }

  // Rafale retombée : on redessine une fois, et une seule.
  if (buttons_io::refreshDue() && wifi_mgr::isConnected()) {
    last_poll_ms = millis();
    first_poll_done = true;
    pollAndRender();
    return;
  }

  // Premier sondage immédiat, puis toutes les SONOS_POLL_INTERVAL_S secondes.
  if (wifi_mgr::isConnected() && !first_poll_done) {
    first_poll_done = true;
    last_poll_ms = millis();
    pollAndRender();
    return;
  }

  if (wifi_mgr::isConnected() &&
      millis() - last_poll_ms >= SONOS_POLL_INTERVAL_S * 1000UL) {
    last_poll_ms = millis();
    pollAndRender();
  }

  // 10 ms : il faut échantillonner nettement plus vite que l'anti-rebond de
  // 50 ms, sinon un appui bref passerait inaperçu.
  delay(10);
}
