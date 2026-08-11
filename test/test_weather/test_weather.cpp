// Tests du parseur météo. La fixture est la sortie réelle du template Jinja de
// homeassistant/weather_to_mqtt.yaml, évaluée par le moteur de Home Assistant.
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "core/weather.h"
#include "core/weather_rtc.h"
#include "core/weather_view.h"

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

void test_parses_current_conditions() {
  const weather::Report report = weather::parse(fixture("weather.json"));

  TEST_ASSERT_TRUE(report.valid);
  TEST_ASSERT_EQUAL(weather::Condition::kSunny, report.condition);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.6f, report.temperature_c);
  TEST_ASSERT_EQUAL_INT(23, report.humidity_pct);
  TEST_ASSERT_EQUAL_INT(10, report.wind_kmh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.6f, report.uv_index);
}

void test_parses_hourly_forecast() {
  const weather::Report report = weather::parse(fixture("weather.json"));

  TEST_ASSERT_EQUAL_UINT(6, report.hourly.size());
  TEST_ASSERT_EQUAL_STRING("16", report.hourly[0].label.c_str());
  TEST_ASSERT_EQUAL(weather::Condition::kPartlyCloudy, report.hourly[0].condition);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.3f, report.hourly[0].temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, report.hourly[2].precipitation_mm);
  // Le passage à minuit ne doit pas casser le libellé.
  TEST_ASSERT_EQUAL_STRING("00", report.hourly[4].label.c_str());
}

// Le sujet MQTT est retenu par le broker : sans contrôle d'âge, une
// automatisation arrêtée depuis trois jours ferait afficher des valeurs
// anciennes avec l'aplomb des valeurs fraîches.
void test_detects_stale_report() {
  const weather::Report report = weather::parse(fixture("weather.json"));

  TEST_ASSERT_FALSE(weather::isStale(report, report.published_at + 3600));
  TEST_ASSERT_TRUE(weather::isStale(report, report.published_at + 86400));
}

void test_rejects_malformed_payload() {
  TEST_ASSERT_FALSE(weather::parse("").valid);
  TEST_ASSERT_FALSE(weather::parse("pas du json").valid);
  // JSON valide mais sans le bloc attendu : ne doit pas passer pour un rapport.
  TEST_ASSERT_FALSE(weather::parse("{\"ts\":1}").valid);
  TEST_ASSERT_TRUE(weather::isStale(weather::parse(""), 0));
}

void test_condition_vocabulary() {
  TEST_ASSERT_EQUAL(weather::Condition::kClearNight,
                    weather::conditionFromString("clear-night"));
  TEST_ASSERT_EQUAL(weather::Condition::kLightning,
                    weather::conditionFromString("lightning-rainy"));
  TEST_ASSERT_EQUAL(weather::Condition::kUnknown,
                    weather::conditionFromString("condition-inventee"));
  TEST_ASSERT_EQUAL_STRING("--", weather::conditionLabel(weather::Condition::kUnknown));
}

// --- Mise en forme de l'écran -----------------------------------------------

namespace {

// Un instant contemporain de la fixture, donc un rapport frais.
constexpr long kFixtureNow = 1786284627 + 600;

weather::Report fixtureReport() { return weather::parse(fixture("weather.json")); }

}  // namespace

void test_view_formats_current_conditions() {
  const weather::View view = weather::plan(fixtureReport(), kFixtureNow);

  TEST_ASSERT_FALSE(view.stale);
  TEST_ASSERT_EQUAL_STRING("Ensoleille", view.condition_label.c_str());
  // Le pictogramme se choisit sur l'énumération, pas sur le libellé traduit.
  TEST_ASSERT_EQUAL(weather::Condition::kSunny, view.condition);
  TEST_ASSERT_EQUAL_STRING("33 C", view.temperature.c_str());  // 32,6 arrondi
  TEST_ASSERT_EQUAL_STRING("23 %   10 km/h   UV 5.6", view.details.c_str());
}

void test_view_caps_the_forecast_at_six_columns() {
  weather::Report report = fixtureReport();
  for (int i = 0; i < 4; ++i) report.hourly.push_back(report.hourly.front());

  const weather::View view = weather::plan(report, kFixtureNow);
  TEST_ASSERT_EQUAL_INT(weather::kMaxColumns, static_cast<int>(view.columns.size()));
  TEST_ASSERT_EQUAL_STRING("16h", view.columns.front().hour.c_str());
}

// Une colonne « 0 mm » à chaque créneau d'une journée sèche n'apprend rien.
void test_view_shows_rain_only_when_it_rains() {
  const weather::View view = weather::plan(fixtureReport(), kFixtureNow);
  TEST_ASSERT_TRUE(view.columns[0].precipitation.empty());
  TEST_ASSERT_EQUAL_STRING("0.4 mm", view.columns[2].precipitation.c_str());
}

// Le sujet MQTT est retenu : sans cette garde, un broker resservirait la météo
// d'avant-hier avec l'aplomb d'une mesure fraîche.
void test_view_hides_figures_when_the_report_is_stale() {
  const weather::View view = weather::plan(fixtureReport(), kFixtureNow + 3 * 3600);

  TEST_ASSERT_TRUE(view.stale);
  TEST_ASSERT_EQUAL_STRING("Meteo perimee", view.condition_label.c_str());
  TEST_ASSERT_TRUE(view.temperature.empty());
  TEST_ASSERT_TRUE(view.columns.empty());
  // Aucun pictogramme non plus : dessiner un soleil au-dessus de « Météo
  // périmée » reviendrait à affirmer ce qu'on vient de démentir.
  TEST_ASSERT_EQUAL(weather::Condition::kUnknown, view.condition);
}

void test_view_without_any_report_says_so() {
  const weather::View view = weather::plan(weather::Report{}, kFixtureNow);
  TEST_ASSERT_TRUE(view.stale);
  TEST_ASSERT_EQUAL_STRING("Meteo indisponible", view.condition_label.c_str());
}

// L'intérieur reste affiché même quand la météo manque : il vient du SHT4x, pas
// du réseau. Et sans capteur, la ligne disparaît au lieu d'annoncer 0 °C.
void test_view_indoor_line_is_independent_of_the_forecast() {
  const weather::View with_sensor =
      weather::plan(weather::Report{}, kFixtureNow, true, 21.4f, 48);
  TEST_ASSERT_EQUAL_STRING("Interieur 21.4 C   48 %", with_sensor.indoor.c_str());

  const weather::View without_sensor = weather::plan(fixtureReport(), kFixtureNow);
  TEST_ASSERT_TRUE(without_sensor.indoor.empty());
}

// --- Survie au deep sleep ----------------------------------------------------

// Le boîtier se rendort toutes les minutes. Sans conservation, le bulletin
// était perdu à chaque réveil et l'écran annonçait « Météo indisponible »
// alors que la donnée existait.
void test_report_survives_a_round_trip_through_rtc_memory() {
  const weather::Report source = fixtureReport();
  const weather::Report restored = weather::fromRtc(weather::toRtc(source));

  TEST_ASSERT_TRUE(restored.valid);
  TEST_ASSERT_EQUAL(source.condition, restored.condition);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, source.temperature_c, restored.temperature_c);
  TEST_ASSERT_EQUAL_INT(source.humidity_pct, restored.humidity_pct);
  TEST_ASSERT_EQUAL_INT(source.wind_kmh, restored.wind_kmh);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, source.uv_index, restored.uv_index);
  TEST_ASSERT_EQUAL_INT(source.published_at, restored.published_at);

  TEST_ASSERT_EQUAL_UINT(source.hourly.size(), restored.hourly.size());
  TEST_ASSERT_EQUAL_STRING(source.hourly[0].label.c_str(),
                           restored.hourly[0].label.c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.05f, source.hourly[2].precipitation_mm,
                           restored.hourly[2].precipitation_mm);
}

// Au tout premier démarrage, la mémoire RTC contient n'importe quoi. Elle ne
// doit pas passer pour un bulletin.
void test_uninitialised_rtc_memory_is_not_a_report() {
  weather::RtcReport garbage;
  garbage.magic = 0xDEADBEEF;
  garbage.temperature_dc = 999;
  TEST_ASSERT_FALSE(weather::fromRtc(garbage).valid);

  // Un rapport invalide ne s'écrit pas non plus : il écraserait le précédent.
  TEST_ASSERT_FALSE(weather::fromRtc(weather::toRtc(weather::Report{})).valid);
}

// L'horodatage doit traverser intact, sinon le contrôle de péremption devient
// faux au premier réveil.
void test_publication_timestamp_is_preserved() {
  const weather::Report restored = weather::fromRtc(weather::toRtc(fixtureReport()));
  TEST_ASSERT_FALSE(weather::isStale(restored, kFixtureNow));
  TEST_ASSERT_TRUE(weather::isStale(restored, kFixtureNow + 86400));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_report_survives_a_round_trip_through_rtc_memory);
  RUN_TEST(test_uninitialised_rtc_memory_is_not_a_report);
  RUN_TEST(test_publication_timestamp_is_preserved);
  RUN_TEST(test_parses_current_conditions);
  RUN_TEST(test_parses_hourly_forecast);
  RUN_TEST(test_detects_stale_report);
  RUN_TEST(test_rejects_malformed_payload);
  RUN_TEST(test_condition_vocabulary);
  RUN_TEST(test_view_formats_current_conditions);
  RUN_TEST(test_view_caps_the_forecast_at_six_columns);
  RUN_TEST(test_view_shows_rain_only_when_it_rains);
  RUN_TEST(test_view_hides_figures_when_the_report_is_stale);
  RUN_TEST(test_view_without_any_report_says_so);
  RUN_TEST(test_view_indoor_line_is_independent_of_the_forecast);
  return UNITY_END();
}
