#include "core/uptime.h"

namespace uptime {

uint64_t totalMs(uint64_t accumulated_ms, uint32_t awake_ms) {
  return accumulated_ms + awake_ms;
}

uint64_t accumulate(uint64_t accumulated_ms, uint32_t awake_ms, uint32_t sleep_ms) {
  return accumulated_ms + awake_ms + sleep_ms;
}

}  // namespace uptime
