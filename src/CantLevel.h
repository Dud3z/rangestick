#pragma once

#include "AppModule.h"
#include <cstdint>

// Digital level (UI). The actual calculation/calibration lives in the globally shared
// CantCalculator instance (see main.cpp) -- a calibration therefore also applies in combo mode
// (ComboView), without having to be performed twice.
// In the READY state, B (up) / power (down) cycles through several display styles for the angle
// (horizon line, level tube, dial).
class CantLevel : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "ANTI-CANT"; }
    bool isBusy() const override { return true; } // live display, should not dim

private:
    int displayStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void draw();
};
