#pragma once

#include <cstdint>

// Gemeinsames Farbschema fuer alle Screens, statt purem Schwarz/Weiss + TFT_*-Grundfarben.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace Theme {
constexpr uint16_t BG      = rgb565(8, 10, 20);    // dunkles Blauschwarz statt reinem Schwarz
constexpr uint16_t PANEL   = rgb565(26, 30, 46);   // Flaeche fuer Listen-/Feld-Highlights
extern uint16_t ACCENT;                            // einstellbar (Settings), Default Cyan
constexpr uint16_t ACCENT2 = rgb565(255, 150, 20);  // Orange -- Edit-/Aktiv-Zustand
constexpr uint16_t TEXT    = rgb565(235, 240, 245);
constexpr uint16_t SUBTEXT = rgb565(130, 140, 155);
constexpr uint16_t GOOD    = rgb565(60, 220, 130);
constexpr uint16_t WARN    = rgb565(255, 200, 40);
constexpr uint16_t BAD     = rgb565(255, 80, 80);
} // namespace Theme
