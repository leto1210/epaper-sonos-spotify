// Tests de la politique de choix de zone. Aucune enceinte requise.
#include <unity.h>

#include <string>
#include <vector>

#include "core/zone_picker.h"

namespace {

using sonos::TransportState;
using sonos::ZoneStatus;

std::vector<ZoneStatus> maison() {
  return {
      {"uuid-1", "Salon", "192.0.2.10", TransportState::kStopped},
      {"uuid-2", "Cuisine", "192.0.2.11", TransportState::kPlaying},
      {"uuid-3", "Bureau", "192.0.2.12", TransportState::kPlaying},
      {"uuid-4", "Chambre", "192.0.2.13", TransportState::kStopped},
  };
}

const std::vector<std::string> kPriorite = {"Salon", "Bureau", "Cuisine"};

}  // namespace

void test_prefers_configured_priority_among_playing_zones() {
  // Le Salon est prioritaire mais à l'arrêt : c'est le Bureau qui gagne, pas la
  // Cuisine, bien qu'elle vienne avant dans la liste des zones.
  const sonos::Choice choice = sonos::pickZone(maison(), kPriorite);

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_TRUE(choice.playing);
  TEST_ASSERT_EQUAL_STRING("Bureau", choice.name.c_str());
  TEST_ASSERT_EQUAL_STRING("192.0.2.12", choice.ip.c_str());
}

void test_falls_back_to_any_playing_zone() {
  std::vector<ZoneStatus> zones = maison();
  zones[2].state = TransportState::kStopped;  // Bureau à l'arrêt

  const sonos::Choice choice = sonos::pickZone(zones, {"Terrasse", "Garage"});

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_EQUAL_STRING("Cuisine", choice.name.c_str());
}

// Une zone en pause reste la zone « en cours » : c'est ce que l'utilisateur
// veut voir. En Spotify Connect les métadonnées disparaissent à la pause, mais
// c'est un problème d'affichage, pas de choix de zone.
void test_paused_zone_still_counts_as_current() {
  std::vector<ZoneStatus> zones = maison();
  zones[1].state = TransportState::kStopped;
  zones[2].state = TransportState::kPaused;

  const sonos::Choice choice = sonos::pickZone(zones, kPriorite);

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_EQUAL_STRING("Bureau", choice.name.c_str());
}

// Cas relevé sur le matériel réel : la Cuisine, en pause et prioritaire, était
// retenue alors que le Beam jouait vraiment. Ce qui joue l'emporte toujours sur
// une préférence en pause.
void test_actually_playing_beats_paused_favourite() {
  std::vector<ZoneStatus> zones = {
      {"uuid-1", "Cuisine", "192.0.2.11", TransportState::kPaused},
      {"uuid-2", "Beam", "192.0.2.20", TransportState::kPlaying},
  };

  const sonos::Choice choice = sonos::pickZone(zones, {"Cuisine", "Beam"});

  TEST_ASSERT_EQUAL_STRING("Beam", choice.name.c_str());
  TEST_ASSERT_TRUE(choice.playing);
}

// Mais entre deux zones qui jouent, la préférence reprend ses droits.
void test_priority_decides_among_playing_zones() {
  std::vector<ZoneStatus> zones = {
      {"uuid-1", "Cuisine", "192.0.2.11", TransportState::kPlaying},
      {"uuid-2", "Beam", "192.0.2.20", TransportState::kPlaying},
  };

  TEST_ASSERT_EQUAL_STRING("Beam", sonos::pickZone(zones, {"Beam"}).name.c_str());
  TEST_ASSERT_EQUAL_STRING("Cuisine", sonos::pickZone(zones, {"Cuisine"}).name.c_str());
}

// Cas relevé sur le matériel : après un basculement Spotify Connect, l'enceinte
// précédente garde un état PLAYING résiduel sans plus rien savoir du morceau.
// Prioritaire, elle bloquait l'affichage sur une zone muette. Le classement
// permet à l'appelant de descendre jusqu'à une zone qui répond vraiment.
void test_ranking_offers_a_fallback_after_a_ghost_zone() {
  const std::vector<ZoneStatus> zones = {
      {"uuid-1", "Beam", "192.0.2.20", TransportState::kPlaying},     // fantôme
      {"uuid-2", "Holiday", "192.0.2.21", TransportState::kPlaying},  // joue vraiment
      {"uuid-3", "Cuisine", "192.0.2.11", TransportState::kPaused},
  };

  const std::vector<sonos::Choice> ranked = sonos::rankZones(zones, {"Beam"});

  TEST_ASSERT_TRUE(ranked.size() >= 2);
  TEST_ASSERT_EQUAL_STRING("Beam", ranked[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("Holiday", ranked[1].name.c_str());
  // La zone en pause reste proposée, mais en dernier.
  TEST_ASSERT_EQUAL_STRING("Cuisine", ranked.back().name.c_str());
}

// Aucune zone ne doit apparaître deux fois : l'appelant compte sur le
// classement pour épuiser les candidats, pas pour boucler.
void test_ranking_has_no_duplicates() {
  const std::vector<sonos::Choice> ranked =
      sonos::rankZones(maison(), kPriorite, "Cuisine");

  for (size_t i = 0; i < ranked.size(); ++i) {
    for (size_t j = i + 1; j < ranked.size(); ++j) {
      TEST_ASSERT_TRUE(ranked[i].name != ranked[j].name);
    }
  }
}

// Le drapeau `playing` doit dire la vérité : il servait à choisir l'icône du
// bandeau, et valait « en lecture » y compris pour une zone en pause.
void test_playing_flag_is_truthful_for_paused_zone() {
  std::vector<ZoneStatus> zones = maison();
  for (ZoneStatus& zone : zones) zone.state = TransportState::kStopped;
  zones[1].state = TransportState::kPaused;

  const sonos::Choice choice = sonos::pickZone(zones, kPriorite);

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_EQUAL_STRING("Cuisine", choice.name.c_str());
  TEST_ASSERT_FALSE(choice.playing);
}

// Rien ne joue : on garde la dernière zone affichée, en signalant qu'elle ne
// joue plus. L'écran conserve alors sa fiche au lieu de se vider.
void test_keeps_last_zone_when_nothing_plays() {
  std::vector<ZoneStatus> zones = maison();
  for (ZoneStatus& zone : zones) zone.state = TransportState::kStopped;

  const sonos::Choice choice = sonos::pickZone(zones, kPriorite, "Cuisine");

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_FALSE(choice.playing);
  TEST_ASSERT_EQUAL_STRING("Cuisine", choice.name.c_str());
}

void test_nothing_to_show_at_all() {
  std::vector<ZoneStatus> zones = maison();
  for (ZoneStatus& zone : zones) zone.state = TransportState::kStopped;

  TEST_ASSERT_FALSE(sonos::pickZone(zones, kPriorite).found);
  TEST_ASSERT_FALSE(sonos::pickZone({}, kPriorite, "Cuisine").found);
}

// Le sélecteur exposé dans Home Assistant l'emporte sur tout le reste.
void test_forced_zone_wins() {
  const sonos::Choice choice = sonos::pickZone(maison(), kPriorite, "Cuisine", "Chambre");

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_FALSE(choice.playing);
  TEST_ASSERT_EQUAL_STRING("Chambre", choice.name.c_str());
}

// Une zone forcée devenue introuvable — enceinte débranchée, renommée — ne doit
// pas faire basculer l'écran sur une autre pièce sans prévenir.
void test_forced_zone_missing_shows_nothing() {
  const sonos::Choice choice = sonos::pickZone(maison(), kPriorite, "Cuisine", "Grenier");

  TEST_ASSERT_FALSE(choice.found);
}

void test_auto_is_not_a_zone_name() {
  const sonos::Choice choice = sonos::pickZone(maison(), kPriorite, {}, "auto");

  TEST_ASSERT_TRUE(choice.found);
  TEST_ASSERT_EQUAL_STRING("Bureau", choice.name.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_prefers_configured_priority_among_playing_zones);
  RUN_TEST(test_falls_back_to_any_playing_zone);
  RUN_TEST(test_paused_zone_still_counts_as_current);
  RUN_TEST(test_actually_playing_beats_paused_favourite);
  RUN_TEST(test_priority_decides_among_playing_zones);
  RUN_TEST(test_ranking_offers_a_fallback_after_a_ghost_zone);
  RUN_TEST(test_ranking_has_no_duplicates);
  RUN_TEST(test_playing_flag_is_truthful_for_paused_zone);
  RUN_TEST(test_keeps_last_zone_when_nothing_plays);
  RUN_TEST(test_nothing_to_show_at_all);
  RUN_TEST(test_forced_zone_wins);
  RUN_TEST(test_forced_zone_missing_shows_nothing);
  RUN_TEST(test_auto_is_not_a_zone_name);
  return UNITY_END();
}
