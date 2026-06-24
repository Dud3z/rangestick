#pragma once

#include "AppModule.h"
#include <cstdint>

// Digitale Wasserwaage (UI). Die eigentliche Berechnung/Kalibrierung liegt in der global
// geteilten CantCalculator-Instanz (siehe main.cpp) -- dieselbe Kalibrierung gilt dadurch auch
// im Kombi-Modus (ComboView), ohne sie doppelt durchfuehren zu muessen.
// Im READY-Zustand kann mit B (Hoch) / Power (Runter) zwischen mehreren Anzeige-Stilen fuer
// den Winkel umgeschaltet werden (Horizont-Linie, Wasserwaage-Roehre, Drehrad).
class CantLevel : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "ANTI-CANT"; }
    bool isBusy() const override { return true; } // Live-Anzeige, soll nicht dimmen

private:
    int displayStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void draw();
};
