#pragma once

// Logique pure, testable sur machine de dev : tout ce qui vit dans src/core/
// doit compiler sans Arduino (voir env:native dans platformio.ini).
namespace epaper_spotify {

constexpr const char* kFirmwareVersion = "0.2.2";

}  // namespace epaper_spotify
