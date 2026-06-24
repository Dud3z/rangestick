#pragma once

#include "AppModule.h"
#include <cstdint>

// Combo view: anti-cant on top (prominent, with switchable styles), stability at the bottom as a
// slim indicator (number + bar, no own controls). Uses the same globally shared calculation
// engines as the standalone modules (see Globals.h) -- so an anti-cant calibration performed
// once also applies here.
class ComboView : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "COMBO"; }
    bool isBusy() const override { return true; } // live display, should not dim

private:
    int cantStyle_ = 0;
    uint32_t lastDrawMs_ = 0;

    void draw();
};
