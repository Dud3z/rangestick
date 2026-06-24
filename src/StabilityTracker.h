#pragma once

#include "AppModule.h"
#include <cstdint>

// Stability-Tracker (UI). Die eigentliche Berechnung liegt in der global geteilten
// StabilityCalculator-Instanz (siehe main.cpp) -- dieselbe Messung/Session gilt dadurch auch
// im Kombi-Modus (ComboView).
class StabilityTracker : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "STABILITY"; }
    bool isBusy() const override { return true; } // Live-Anzeige, soll nicht dimmen

private:
    static constexpr int STYLE_COUNT = 3;

    int displayStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void drawSparklineStyle(int x, int y, int w, int h, uint16_t color);
    void drawScatterStyle(int cx, int cy, uint16_t color);
    void drawPulseStyle(int cx, int cy, uint16_t color);
    void draw();
};
