// Tests du parseur météo. La fixture est la sortie réelle du template Jinja de
// homeassistant/weather_to_mqtt.yaml, évaluée par le moteur de Home Assistant.
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "core/weather.h"

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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_current_conditions);
  RUN_TEST(test_parses_hourly_forecast);
  RUN_TEST(test_detects_stale_report);
  RUN_TEST(test_rejects_malformed_payload);
  RUN_TEST(test_condition_vocabulary);
  return UNITY_END();
}
