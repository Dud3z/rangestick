#pragma once

#include "AppModule.h"
#include <cstdint>

// Stability tracker (UI). The actual calculation lives in the globally shared
// StabilityCalculator instance (see main.cpp) -- the same measurement/session therefore also
// applies in combo mode (ComboView).
class StabilityTracker : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "STABILITY"; }
    bool isBusy() const override { return true; } // live display, should not dim

private:
    static constexpr int STYLE_COUNT = 3;

    int displayStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void drawSparklineStyle(int x, int y, int w, int h, uint16_t color);
    void drawScatterStyle(int cx, int cy, uint16_t color);
    void drawPulseStyle(int cx, int cy, uint16_t color);
    void draw();
};
