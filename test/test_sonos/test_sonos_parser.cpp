// Tests du parseur Sonos, sur des captures réelles anonymisées (test/fixtures/).
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "core/sonos_parser.h"

namespace {

std::string fixture(const std::string& name) {
  const std::string path = std::string(FIXTURE_DIR) + "/" + name;
  std::ifstream file(path);
  TEST_ASSERT_TRUE_MESSAGE(file.is_open(), path.c_str());
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

}  // namespace

// --- Cas nominal : Spotify Connect en lecture -------------------------------
//
// Le mode Spotify Connect (URI `x-sonos-vli:`) ne renvoie les métadonnées que
// pendant la lecture effective. C'est le cas d'usage principal du projet.
void test_playing_spotify_track() {
  const sonos::TrackInfo track = sonos::parsePositionInfo(
      fixture("position_playing_spotify.xml"));

  TEST_ASSERT_TRUE(track.has_metadata);
  TEST_ASSERT_EQUAL_STRING("Brain Stew", track.title.c_str());
  TEST_ASSERT_EQUAL_STRING("Green Day", track.artist.c_str());
  TEST_ASSERT_EQUAL_STRING("Insomniac", track.album.c_str());
  TEST_ASSERT_EQUAL_INT(193, track.duration_s);  // 0:03:13
  TEST_ASSERT_EQUAL_INT(191, track.position_s);  // 0:03:11
  TEST_ASSERT_TRUE(track.coordinator_uuid.empty());
}

// L'URI de pochette pointe vers le CDN Spotify en HTTPS : inutilisable tel quel
// depuis l'ESP32. On passe par le proxy de l'enceinte, en HTTP simple.
void test_album_art_goes_through_the_speaker() {
  const sonos::TrackInfo track = sonos::parsePositionInfo(
      fixture("position_playing_spotify.xml"));

  TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/image/ab67616d0000b273ac9a652335cf34de9a65292a",
                           track.art_uri.c_str());

  // La pochette doit être demandée avec l'URI de la ressource, pas le
  // TrackURI : en Spotify Connect ce dernier est un identifiant de session
  // `x-sonos-vli:`, et l'enceinte répond 404. Constaté sur le matériel.
  TEST_ASSERT_TRUE(track.res_uri.rfind("x-sonos-spotify:", 0) == 0);
  TEST_ASSERT_TRUE(track.track_uri.rfind("x-sonos-vli:", 0) == 0);

  const std::string url = sonos::albumArtUrl("192.0.2.10", track);
  TEST_ASSERT_TRUE(url.rfind("http://192.0.2.10:1400/getaa?s=1&u=", 0) == 0);
  TEST_ASSERT_TRUE(url.find("://i.scdn.co") == std::string::npos);
  TEST_ASSERT_TRUE(url.find("x-sonos-spotify%3A") != std::string::npos);
  TEST_ASSERT_TRUE(url.find("vli") == std::string::npos);
}

// --- Le piège du groupe -----------------------------------------------------
//
// Une enceinte esclave ne sait rien du morceau, mais son TrackURI désigne le
// coordinateur : c'est ce qui permet de rebondir sans relire la topologie.
void test_slave_reports_its_coordinator() {
  const sonos::TrackInfo track = sonos::parsePositionInfo(fixture("position_slave.xml"));

  TEST_ASSERT_FALSE(track.has_metadata);
  TEST_ASSERT_EQUAL_STRING("RINCON_00000000000201400", track.coordinator_uuid.c_str());
}

// --- Absence de métadonnées -------------------------------------------------

void test_idle_speaker_has_no_track() {
  const sonos::TrackInfo track = sonos::parsePositionInfo(fixture("position_idle.xml"));

  TEST_ASSERT_FALSE(track.has_metadata);
  TEST_ASSERT_TRUE(track.title.empty());
  TEST_ASSERT_EQUAL_INT(0, track.duration_s);
}

// Spotify Connect à l'arrêt : l'URI est bien présente, mais les métadonnées
// valent NOT_IMPLEMENTED. Il ne faut surtout pas afficher un écran vide dans ce
// cas — d'où le has_metadata distinct de "l'enceinte joue".
void test_paused_spotify_connect_has_no_metadata() {
  const sonos::TrackInfo track = sonos::parsePositionInfo(fixture("position_vli_paused.xml"));

  TEST_ASSERT_FALSE(track.has_metadata);
  TEST_ASSERT_TRUE(track.track_uri.rfind("x-sonos-vli:", 0) == 0);
  TEST_ASSERT_TRUE(track.coordinator_uuid.empty());  // ce n'est pas un esclave
}

// --- Topologie --------------------------------------------------------------

void test_zone_groups_resolve_coordinators() {
  const std::vector<sonos::ZoneGroup> groups = sonos::parseZoneGroups(fixture("topology.xml"));

  TEST_ASSERT_TRUE(groups.size() >= 7);

  size_t grouped = 0;
  for (const sonos::ZoneGroup& group : groups) {
    const sonos::ZoneMember* coordinator = group.coordinator();
    TEST_ASSERT_NOT_NULL(coordinator);
    TEST_ASSERT_FALSE(coordinator->ip.empty());
    TEST_ASSERT_FALSE(coordinator->name.empty());
    if (group.members.size() > 1) ++grouped;
  }

  // La capture contient deux paires stéréo : sans elles, le test ne prouverait
  // rien sur le cas qui pose problème.
  TEST_ASSERT_EQUAL_UINT(2, grouped);
}

// --- Utilitaires ------------------------------------------------------------

// La Beam sert aussi d'enceinte de télévision : elle est légitimement en
// lecture, sans titre, quand la TV parle. Ce n'est pas une anomalie, et le
// firmware ne doit pas la présenter comme telle.
void test_source_kind_distinguishes_tv_from_music() {
  TEST_ASSERT_EQUAL(sonos::SourceKind::kTvInput,
                    sonos::sourceKind("x-sonos-htastream:RINCON_0001:spdif"));
  TEST_ASSERT_EQUAL(sonos::SourceKind::kLineIn,
                    sonos::sourceKind("x-rincon-stream:RINCON_0001"));
  TEST_ASSERT_EQUAL(sonos::SourceKind::kSlave,
                    sonos::sourceKind("x-rincon:RINCON_0002"));
  TEST_ASSERT_EQUAL(sonos::SourceKind::kTrack,
                    sonos::sourceKind("x-sonos-vli:RINCON_0001:2,spotify:abc"));
  TEST_ASSERT_EQUAL(sonos::SourceKind::kTrack,
                    sonos::sourceKind("x-sonos-spotify:spotify:track:xyz"));
  TEST_ASSERT_EQUAL(sonos::SourceKind::kUnknown, sonos::sourceKind(""));
}

void test_parse_duration() {
  TEST_ASSERT_EQUAL_INT(193, sonos::parseDuration("0:03:13"));
  TEST_ASSERT_EQUAL_INT(3661, sonos::parseDuration("1:01:01"));
  TEST_ASSERT_EQUAL_INT(0, sonos::parseDuration("NOT_IMPLEMENTED"));
  TEST_ASSERT_EQUAL_INT(0, sonos::parseDuration(""));
}

void test_xml_unescape() {
  TEST_ASSERT_EQUAL_STRING("<a b=\"c\">", sonos::xmlUnescape("&lt;a b=&quot;c&quot;&gt;").c_str());
  TEST_ASSERT_EQUAL_STRING("Rock & Roll", sonos::xmlUnescape("Rock &amp; Roll").c_str());
  // Entité inconnue : laissée telle quelle plutôt que perdue.
  TEST_ASSERT_EQUAL_STRING("&nbsp;", sonos::xmlUnescape("&nbsp;").c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_playing_spotify_track);
  RUN_TEST(test_album_art_goes_through_the_speaker);
  RUN_TEST(test_slave_reports_its_coordinator);
  RUN_TEST(test_idle_speaker_has_no_track);
  RUN_TEST(test_paused_spotify_connect_has_no_metadata);
  RUN_TEST(test_zone_groups_resolve_coordinators);
  RUN_TEST(test_source_kind_distinguishes_tv_from_music);
  RUN_TEST(test_parse_duration);
  RUN_TEST(test_xml_unescape);
  return UNITY_END();
}
