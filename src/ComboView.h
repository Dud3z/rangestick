#pragma once

#include "AppModule.h"
#include <cstdint>

// Kombi-Ansicht: Anti-Cant oben (prominent, mit durchschaltbaren Stilen), Stability unten als
// schlanker Indikator (Zahl + Balken, keine eigene Bedienung). Nutzt dieselben global geteilten
// Berechnungs-Engines wie die Einzelmodule (siehe Globals.h) -- eine einmal durchgefuehrte
// Anti-Cant-Kalibrierung gilt also auch hier.
class ComboView : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "COMBO"; }
    bool isBusy() const override { return true; } // Live-Anzeige, soll nicht dimmen

private:
    int cantStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void draw();
};
