#pragma once

#include <cstdint>

// Validation du timing MQTT pour les cycles rapides de deep sleep. La connexion
// MQTT doit survivre au délai entre deux réveils, sinon les entités Home
// Assistant oscillent entre disponible et indisponible.
//
// Logique pure pour être testable sans réseau.
namespace mqtt_timing {

// Intervalle de sondage avec deep sleep.
constexpr uint32_t kSleepIntervalMs = 60 * 1000;

// Keep-alive MQTT. Le broker déclare la connexion morte si pas de trame
// en ce délai.
constexpr uint32_t kKeepAliveS = 90;
constexpr uint32_t kKeepAliveMs = kKeepAliveS * 1000;

// Coûts mesurés ou estimés pour un réveil.
// Wi-Fi : mesuré à 1,7 s (L2).
// Sonos : 250-500 ms (L4).
// MQTT reconnect + rediscovery : à mesurer, estimé ~1-2 s en conditions réelles.
// Marge de sécurité : 0,5 s.
constexpr uint32_t kMaxWakeupCostMs = 2500;

// Validation : le cycle complet (sleep + réveil) doit tenir dans le keepalive.
static_assert(kSleepIntervalMs + kMaxWakeupCostMs < kKeepAliveMs,
              "Cycle too long: device would be declared dead by broker");

// Marge restante après cycle.
constexpr uint32_t kSafetyMarginMs =
    kKeepAliveMs - (kSleepIntervalMs + kMaxWakeupCostMs);

// Sous ce seuil, le sommeil est considéré court : on garde un statut retained
// « online » pour éviter des bascules inutiles dans Home Assistant.
constexpr uint32_t kShortSleepMaxMs = 2 * 60 * 1000;

enum class AvailabilityPolicy {
    kKeepOnlineRetained,
    kPublishOfflineRetained,
};

inline AvailabilityPolicy availabilityPolicyForSleep(uint32_t sleep_duration_ms) {
    return sleep_duration_ms <= kShortSleepMaxMs
                         ? AvailabilityPolicy::kKeepOnlineRetained
                         : AvailabilityPolicy::kPublishOfflineRetained;
}

}  // namespace mqtt_timing
