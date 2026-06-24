#pragma once

#include <cstdint>

// Shared color scheme for all screens, instead of plain black/white + TFT_* base colors.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace Theme {
constexpr uint16_t BG      = rgb565(8, 10, 20);    // dark blue-black instead of pure black
constexpr uint16_t PANEL   = rgb565(26, 30, 46);   // surface for list/field highlights
extern uint16_t ACCENT;                            // configurable (Settings), default cyan
constexpr uint16_t ACCENT2 = rgb565(255, 150, 20);  // orange -- edit/active state
constexpr uint16_t TEXT    = rgb565(235, 240, 245);
constexpr uint16_t SUBTEXT = rgb565(130, 140, 155);
constexpr uint16_t GOOD    = rgb565(60, 220, 130);
constexpr uint16_t WARN    = rgb565(255, 200, 40);
constexpr uint16_t BAD     = rgb565(255, 80, 80);
} // namespace Theme
