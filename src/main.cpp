// ePaper Spotify — afficheur Sonos sur reTerminal E1002.
// Livraison 6 : affichage du morceau en cours. Pochettes et capteurs suivent.
#include <Arduino.h>
#include <WiFi.h>

#include <cstring>  // strncpy, pour la sérialisation de l'état RTC
#include <vector>

#include "albumart.h"
#include "buttons_io.h"
#include "config.h"
#include "core/version.h"
#include "core/layout_plan.h"
#include "core/pause_timer.h"
#include "core/rtc_state.h"
#include "core/sleep_manager.h"
#include "core/uptime.h"
#include "core/wakeup.h"
#include "core/weather_rtc.h"
#include "core/weather_view.h"
#include "core/zone_picker.h"
#include "display.h"
#include "mqtt.h"
#include "ota.h"
#include "sensors.h"
#include "sonos_client.h"
#include "wifi_mgr.h"

// Option récente : une `src/config.h` écrite avant elle ne la définit pas.
#ifndef SONOS_SLEEP_WHILE_PLAYING
#define SONOS_SLEEP_WHILE_PLAYING 0
#endif

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
//
// Elle est conservée sous forme de condensé dans la mémoire RTC, qui reste
// alimentée pendant le deep sleep. Sans cela, chaque réveil repartirait de
// `setup()` avec une empreinte vide et redessinerait l'écran : un
// rafraîchissement de 37 s toutes les minutes, exactement ce que la veille est
// censée éviter. Le compteur de rafraîchissements y est joint, sans quoi il
// repartirait de zéro dans Home Assistant à chaque réveil.
RTC_DATA_ATTR uint32_t g_rtc_shown_hash = 0;
RTC_DATA_ATTR uint32_t g_rtc_refresh_count = 0;

// État persistent à travers le deep sleep : zone forcée, dernière zone connue,
// état des timers de pause et de sommeil.
RTC_DATA_ATTR rtc::State g_rtc_state = {};

// FNV-1a : quelques lignes, une répartition suffisante pour comparer des
// empreintes. Une collision se traduirait par un rafraîchissement manquant,
// jamais par un affichage faux.
uint32_t fingerprintHash(const std::string& text) {
  uint32_t hash = 2166136261u;
  for (const char c : text) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  // Zéro sert de « rien n'a encore été affiché » : on l'évite comme valeur.
  return hash == 0 ? 1 : hash;
}

bool alreadyShown(const std::string& fingerprint) {
  return g_rtc_shown_hash == fingerprintHash(fingerprint);
}

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

power::SleepManager g_sleep_manager;

// Vrai quand la zone retenue joue vraiment. Réévalué à *chaque* sondage : il
// était auparavant déduit de `g_last_zone`, qui garde la dernière zone connue
// pour les boutons et n'est jamais vidé — le boîtier se croyait donc en lecture
// pour toujours dès le premier morceau trouvé, et ne s'endormait jamais.
bool g_anything_playing = false;

// Horodatage du dernier rafraîchissement, au format attendu par Home Assistant
// (`device_class: timestamp`). Vide tant que l'heure n'a pas été synchronisée :
// mieux vaut pas de date qu'une date de 1970.
// En mémoire RTC pour la même raison que l'empreinte : sans cela, l'entité
// « Dernier rafraîchissement » de Home Assistant retombait sur « inconnu » dès
// le premier sommeil, puisque plus aucun réveil ne redessine.
RTC_DATA_ATTR char g_rtc_last_refresh_iso[40] = {};

// Dernier bulletin météo reçu. Voir core/weather_rtc : il vient du réseau mais
// doit survivre au sommeil, la livraison du sujet retenu ne tenant pas toujours
// dans les quelques secondes d'éveil.
RTC_DATA_ATTR weather::RtcReport g_rtc_weather = {};

// Horodatage du dernier appui de bouton, pour détecter une activité utilisateur
// récente (dans les 2 secondes). Elle réveille du sommeil ou réinitialise le
// compteur d'inactivité.
static uint32_t g_last_button_press_ms = 0;

std::string nowIso8601() {
  struct tm local;
  if (!getLocalTime(&local, 0)) return {};
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &local);
  return buffer;
}

void rememberRefreshTime() {
  const std::string now = nowIso8601();
  if (now.empty()) return;  // heure pas encore synchronisée : ne rien écraser
  std::strncpy(g_rtc_last_refresh_iso, now.c_str(), sizeof(g_rtc_last_refresh_iso) - 1);
  g_rtc_last_refresh_iso[sizeof(g_rtc_last_refresh_iso) - 1] = '\0';
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
  // Le sommeil compte dans le temps de fonctionnement : sans le cumul RTC,
  // cette entité retombait à deux secondes à chaque réveil et donnait à lire
  // un boîtier qui redémarre en boucle. Voir core/uptime.h.
  state.uptime_s =
      static_cast<long>(uptime::totalMs(g_rtc_state.uptime_accumulated_ms, millis()) / 1000);
  state.refresh_count = static_cast<int>(display::refreshCount());
  state.last_refresh_iso = g_rtc_last_refresh_iso;
  state.selected_zone = g_forced_zone;

  mqtt::publishState(state);
}

// Applique une commande de transport à la zone affichée. Le rafraîchissement
// n'est pas déclenché ici : il attend que la rafale d'appuis soit retombée.
void sendTransport(buttons::Action action) {
  // Enregistrer l'appui pour déterminer s'il y a une activité utilisateur
  // récente (réveille du sommeil, réinitialise l'inactivité).
  if (action != buttons::Action::kNone) {
    g_last_button_press_ms = millis();
  }

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
      g_rtc_shown_hash = 0;
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
    case ha::CommandKind::kOtaWindow:
      // Surtout pas de redessin ici : les 37 s d'un rafraîchissement
      // consommeraient une bonne part de la fenêtre, sans que `ota::handle()`
      // soit appelé une seule fois pendant ce temps.
      ota::openWindow();
      // La commande est publiée en retenu, faute de quoi un appui tombant
      // pendant un sommeil serait perdu. Il faut donc l'effacer maintenant
      // qu'elle est servie, sinon chaque réveil rouvrirait une fenêtre.
      mqtt::clearRetainedCommand("/cmd/ota");
      return;
    case ha::CommandKind::kNone:
      return;
  }

  // Oublier l'empreinte suffit à rendre le prochain rendu inconditionnel.
  g_rtc_shown_hash = 0;
  g_pending_render = true;
}

// Écran d'une entrée ligne. Même politique d'empreinte que partout ailleurs :
// tant que la zone ne change pas, l'écran ne bouge pas — et il ne bougera de
// toute façon pas au rythme de la musique, faute de la moindre métadonnée.
void renderLineIn(const std::string& zone) {
  const std::string fingerprint = "entree-ligne|" + zone;
  if (alreadyShown(fingerprint)) return;

  const sensors::Reading measures = sensors::read();

  display::Status status;
  status.zone = zone;
  status.playing = true;
  status.indoor_temperature_c = measures.temperature_c;
  status.indoor_humidity_pct = measures.has_climate ? measures.humidity_pct : 0;
  status.battery_pct = measures.battery_pct;
  status.charging = measures.charging;

  display::showLineIn(zone, status);
  g_rtc_shown_hash = fingerprintHash(fingerprint);
  g_rtc_refresh_count = display::refreshCount();
  rememberRefreshTime();
  publishMeasurements(measures);
}

// Écran de repli quand rien ne joue. Même politique que pour le morceau : une
// empreinte, et pas de rafraîchissement si rien n'a bougé.
void renderWeather() {
  const sensors::Reading measures = sensors::read();

  // Le bulletin vient du réseau, mais il doit survivre au deep sleep : le
  // sujet MQTT est retenu, l'abonnement le renvoie — sans que la livraison
  // tienne toujours dans les trois secondes d'éveil. Sans conservation,
  // l'écran annonçait « Météo indisponible » alors que la donnée existait.
  weather::Report report = mqtt::weatherReport();
  if (report.valid) {
    g_rtc_weather = weather::toRtc(report);
  } else {
    report = weather::fromRtc(g_rtc_weather);
  }

  const weather::View view = weather::plan(report, time(nullptr), measures.has_climate,
                                           measures.temperature_c,
                                           measures.humidity_pct);

  // Un écran déjà juste ne se remplace pas par un aveu d'ignorance : au tout
  // premier démarrage il n'y a rien à préserver, ensuite il vaut mieux garder
  // le dernier bulletin connu que d'afficher « Météo indisponible ».
  if (!report.valid && g_rtc_shown_hash != 0) {
    Serial.println("[meteo] aucun bulletin, ecran conserve");
    return;
  }

  // L'empreinte ne retient que ce qui se voit : deux relevés successifs à
  // 32,58 et 32,62 °C ne valent pas 37 s de rafraîchissement.
  std::string fingerprint = "meteo|" + view.condition_label + "|" + view.temperature;
  for (const weather::Column& column : view.columns) {
    fingerprint += "|" + column.hour + column.temperature;
  }
  if (alreadyShown(fingerprint)) return;

  display::Status status;
  status.indoor_temperature_c = measures.temperature_c;
  status.indoor_humidity_pct = measures.has_climate ? measures.humidity_pct : 0;
  status.battery_pct = measures.battery_pct;
  status.charging = measures.charging;

  display::showWeather(view, status);
  g_rtc_shown_hash = fingerprintHash(fingerprint);
  g_rtc_refresh_count = display::refreshCount();
  rememberRefreshTime();
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
    g_anything_playing = false;
    renderWeather();
    return;
  }

  sonos::Choice choice;
  sonos::TrackInfo track;

  // Une zone sur son entrée ligne — platine, ampli — ne livre aucune
  // métadonnée. On la retient de côté pour l'afficher si rien de mieux ne se
  // présente : dire « entrée ligne au Séjour » vaut mieux que la météo, qui
  // laisserait croire la maison silencieuse.
  //
  // L'état de transport n'y est pas fiable : le Séjour s'annonce « en pause »
  // alors que la platine tourne, Sonos ne « jouant » pas une entrée ligne comme
  // il joue un morceau. On affiche donc l'écran quel que soit cet état, mais on
  // ne le compte comme lecture — et donc comme raison de ne pas s'endormir —
  // que s'il annonce vraiment PLAYING. Sans quoi une entrée ligne oubliée
  // tiendrait le boîtier éveillé indéfiniment.
  std::string line_in_zone;
  bool line_in_playing = false;

  for (const sonos::Choice& candidate : ranked) {
    track = sonos_client::fetchPositionInfo(candidate.ip);
    if (track.has_metadata) {
      choice = candidate;
      break;
    }
    const char* raison = "ne sait rien";
    switch (sonos::sourceKind(track.track_uri)) {
      case sonos::SourceKind::kTvInput: raison = "diffuse la television"; break;
      case sonos::SourceKind::kLineIn:
        raison = "est sur son entree ligne";
        if (line_in_zone.empty()) {
          line_in_zone = candidate.name;
          line_in_playing = candidate.playing;
        }
        break;
      case sonos::SourceKind::kSlave: raison = "est esclave d'un groupe"; break;
      default: break;
    }
    Serial.printf("[sonos] %s ecarte : %s\n", candidate.name.c_str(), raison);
  }

  if (!choice.found && !line_in_zone.empty()) {
    Serial.printf("[sonos] entree ligne sur %s%s\n", line_in_zone.c_str(),
                  line_in_playing ? "" : " (transport inactif)");
    g_anything_playing = line_in_playing;
    renderLineIn(line_in_zone);
    return;
  }

  if (!choice.found) {
    // Aucune zone ne sait quoi que ce soit : la télévision, une entrée ligne à
    // l'arrêt, ou simplement le silence. L'écran passe à la météo.
    Serial.println("[sonos] aucune metadonnee nulle part, ecran meteo");
    g_anything_playing = false;
    renderWeather();
    return;
  }

  // Une zone en pause ne joue pas : c'est bien ce qui doit lancer le compte à
  // rebours de la veille.
  g_anything_playing = choice.playing;
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
  if (alreadyShown(fingerprint)) {
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
  status.charging = measures.charging;

  display::showTrack(track, status, art);
  g_rtc_shown_hash = fingerprintHash(fingerprint);
  g_rtc_refresh_count = display::refreshCount();
  rememberRefreshTime();
  publishMeasurements(measures);
}

}  // namespace

void setup() {
  Serial.begin(115200);

  // Deux secondes pour laisser le pont CH340 s'ouvrir, sans quoi les premières
  // lignes du démarrage se perdent. Mais `setup()` est aussi le chemin de
  // *chaque* réveil de deep sleep : au rythme d'un réveil toutes les vingt
  // secondes, cette attente représentait à elle seule près d'un dixième du
  // temps d'éveil, pour un confort de mise au point dont un réveil n'a que
  // faire. Elle est donc réservée au démarrage à froid.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) delay(2000);
  Serial.printf("\n[boot] ePaper Spotify %s\n", epaper_spotify::kFirmwareVersion);
  Serial.printf("[boot] PSRAM libre : %u octets\n", ESP.getFreePsram());

  // Un réveil de deep sleep repasse par ici : ce qui a survécu dans la mémoire
  // RTC évite de tout recommencer à zéro.
  const esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
  if (wakeup == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // Démarrage à froid : c'est le seul moment où le temps de fonctionnement
    // doit repartir de zéro. Explicite plutôt qu'implicite — le reste de l'état
    // RTC n'est lu que sur un réveil, celui-ci l'est à chaque publication.
    g_rtc_state.uptime_accumulated_ms = 0;
  }
  if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.printf("[veille] reveil par %s, %lu rafraichissement(s) deja compte(s)\n",
                  wakeup == ESP_SLEEP_WAKEUP_TIMER ? "minuterie" : "bouton",
                  static_cast<unsigned long>(g_rtc_refresh_count));

    // Restaurer l'état sauvé avant le deep sleep.
    g_forced_zone = g_rtc_state.forced_zone;
    g_last_zone = g_rtc_state.last_zone;
    g_last_ip = g_rtc_state.last_ip;

    idle::PauseTimerState pause_state;
    pause_state.paused = g_rtc_state.pause_timer_paused;
    pause_state.paused_ms = g_rtc_state.pause_timer_elapsed_ms;
    strncpy(pause_state.zone, g_rtc_state.pause_timer_zone, sizeof(pause_state.zone) - 1);
    pause_state.zone[sizeof(pause_state.zone) - 1] = '\0';
    // Le sommeil compte dans la pause : il s'est bel et bien écoulé. Un réveil
    // par bouton peut avoir écourté la tranche, on crédite alors un peu trop —
    // au plus une tranche, et l'appui provoque de toute façon un redessin.
    g_pause_timer.deserialize(pause_state, millis(), g_rtc_state.sleep_duration_ms);

    power::SleepManagerState sleep_state;
    sleep_state.inactive_since_ms = g_rtc_state.sleep_mgr_inactive_since_ms;
    sleep_state.wake_at_ms = g_rtc_state.sleep_mgr_wake_at_ms;
    g_sleep_manager.deserialize(sleep_state);

      // Si c'est un réveil par bouton (ext1), exécuter l'action du bouton qui a
      // déclenché. Les trois boutons réveillent maintenant, pas seulement GPIO4.
      if (wakeup == ESP_SLEEP_WAKEUP_EXT1) {
        uint32_t ext1_mask = esp_sleep_get_ext1_wakeup_status();
        buttons::Action button_action = wakeup::wakeupButtonToAction(ext1_mask);
        if (button_action != buttons::Action::kNone) {
          Serial.printf("[veille] action bouton au réveil : %d\n", static_cast<int>(button_action));
          sendTransport(button_action);
        }
      }
  }

  // Abaisser la fréquence du processeur à 80 MHz a été essayé et abandonné :
  // le rafraîchissement passait de 37,3 s à 41,0 s, mesuré sur cible. Or c'est
  // le panneau qui consomme pendant un redessin, et il restait alimenté 3,7 s
  // de plus — l'économie sur le processeur se payait sur le poste le plus
  // coûteux. Voir docs/hardware.md.

  // La LED de façade est inversée : sans consigne explicite, rien ne garantit
  // son état. Elle n'a aucun rôle ici, l'écran dit tout.
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);

  display::begin();
  display::restoreRefreshCount(g_rtc_refresh_count);
  sensors::begin();
  buttons_io::begin();
  mqtt::begin(onHomeAssistantCommand);

  const bool online = wifi_mgr::connect();
  if (online) {
    wifi_mgr::syncTime();
    ota::begin();
  } else {
    display::showBootScreen("Wi-Fi indisponible");
  }

  Serial.printf("[boot] termine, %lu rafraichissement(s)\n",
                static_cast<unsigned long>(display::refreshCount()));
}

void loop() {
  wifi_mgr::loop();
  mqtt::loop();
  ota::handle();

  // Pendant une fenêtre de mise à jour, le reste de la boucle est suspendu.
  // C'est le rafraîchissement qui l'impose : il bloque 37 s, pendant lesquelles
  // `ota::handle()` n'est jamais appelé et le téléversement expire. Le sommeil
  // est écarté par la même occasion — un boîtier endormi n'écoute rien.
  //
  // Les boutons sont donc inertes le temps de la fenêtre. C'est assumé : cinq
  // minutes, à un moment que l'utilisateur a lui-même choisi, et une commande
  // de transport ferait de toute façon une requête réseau au milieu d'un
  // téléversement.
  if (ota::isWindowOpen()) {
    delay(10);
    return;
  }

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

    // Réveil par la minuterie : le sondage vient d'avoir lieu, l'écran est à
    // jour. Si rien ne joue toujours, on se rendort sans attendre un nouveau
    // compte à rebours de dix minutes.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
      g_sleep_manager.resumeAfterWake(millis());
    }
    return;
  }

  if (wifi_mgr::isConnected() &&
      millis() - last_poll_ms >= SONOS_POLL_INTERVAL_S * 1000UL) {
    last_poll_ms = millis();
    pollAndRender();
  }

  // La veille se décide en fin de cycle, une fois le sondage fait : la placer
  // avant aurait rendormi le boîtier sans qu'il ait jamais interrogé Sonos.
  if (first_poll_done) {
    const bool user_activity_recent = (millis() - g_last_button_press_ms) < 2000UL;
    const power::Decision decision =
        g_sleep_manager.updateAndDecide(millis(), g_anything_playing, user_activity_recent,
                                        SONOS_SLEEP_WHILE_PLAYING != 0);

    if (decision.should_sleep) {
      Serial.printf("[veille] deep sleep pour %lu ms\n",
                    static_cast<unsigned long>(decision.duration_ms));
      
      // Sauvegarder l'état avant le deep sleep pour que le réveil le restaure.
      strncpy(g_rtc_state.forced_zone, g_forced_zone.c_str(), sizeof(g_rtc_state.forced_zone) - 1);
      g_rtc_state.forced_zone[sizeof(g_rtc_state.forced_zone) - 1] = '\0';
      
      strncpy(g_rtc_state.last_zone, g_last_zone.c_str(), sizeof(g_rtc_state.last_zone) - 1);
      g_rtc_state.last_zone[sizeof(g_rtc_state.last_zone) - 1] = '\0';
      
      strncpy(g_rtc_state.last_ip, g_last_ip.c_str(), sizeof(g_rtc_state.last_ip) - 1);
      g_rtc_state.last_ip[sizeof(g_rtc_state.last_ip) - 1] = '\0';

      g_rtc_state.sleep_duration_ms = decision.duration_ms;

      // Le sommeil à venir compte déjà : au réveil, `millis()` repartira de
      // zéro et cet éveil-ci serait sinon perdu deux fois.
      g_rtc_state.uptime_accumulated_ms = uptime::accumulate(
          g_rtc_state.uptime_accumulated_ms, millis(), decision.duration_ms);

      idle::PauseTimerState pause_state = g_pause_timer.serialize(millis());
      g_rtc_state.pause_timer_paused = pause_state.paused;
      g_rtc_state.pause_timer_elapsed_ms = pause_state.paused_ms;
      strncpy(g_rtc_state.pause_timer_zone, pause_state.zone, sizeof(g_rtc_state.pause_timer_zone) - 1);
      g_rtc_state.pause_timer_zone[sizeof(g_rtc_state.pause_timer_zone) - 1] = '\0';

      power::SleepManagerState sleep_state = g_sleep_manager.serialize();
      g_rtc_state.sleep_mgr_inactive_since_ms = sleep_state.inactive_since_ms;
      g_rtc_state.sleep_mgr_wake_at_ms = sleep_state.wake_at_ms;

      // Trois boutons, GPIO3/4/5, tous actifs bas. Au réveil, esp_sleep_get_ext1_wakeup_status()
      // indique quel(s) bouton(s) a(ont) déclenché. Décision prise : exécuter l'action du bouton
      // qui a réveillé. C'est plus utile que de toujours faire un sondage blanc — si l'utilisateur
      // appuie sur « suivant » pour changer de morceau pendant une lecture, il s'attend à un
      // changement immédiat, pas à un sondage qui pourrait le laisser sur le même morceau.
      // Voir docs/architecture.md pour la justification complète et les alternatives rejetées.
      esp_sleep_enable_ext1_wakeup(wakeup::kExt1MaskAll, ESP_EXT1_WAKEUP_ALL_LOW);

      // `esp_deep_sleep` arme lui-même le réveil par minuterie à partir de la
      // durée qu'on lui passe, en microsecondes. Elle ne revient jamais.
      mqtt::beforeDeepSleep(decision.duration_ms);
      Serial.flush();
      esp_deep_sleep(static_cast<uint64_t>(decision.duration_ms) * 1000ULL);
    }
  }

  // 10 ms : il faut échantillonner nettement plus vite que l'anti-rebond de
  // 50 ms, sinon un appui bref passerait inaperçu.
  delay(10);
}
