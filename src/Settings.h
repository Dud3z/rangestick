#pragma once

#include "AppModule.h"
#include "WifiPortal.h"
#include "OtaUpdater.h"
#include <cstddef>
#include <cstdint>

// Einstellungsmenue mit echter zweistufiger Navigation, wie im Hauptmenue:
//   Kategorie-Liste (Display/Timer/Anti-Cant/Stability/System) -> Feld-Liste der Kategorie.
// Browse Kategorien:  B = Hoch, Power = Runter, A = Kategorie oeffnen
// Browse Felder:      B = Hoch (voriges Feld), Power = Runter (naechstes Feld), A = Edit oeffnen
// Edit:                B = Wert+, Power = Wert-, A = Edit verlassen + speichern
// A LANG: erst eine Ebene innerhalb von Settings zurueck (Feld-Liste -> Kategorie-Liste, siehe
// handleBack()), danach (auf der Kategorie-Liste) global zurueck ins Hauptmenue (main.cpp).
// "Reset Standard" ist eine eigene Kategorie mit nur einem Feld, A loest die Aktion direkt aus.
class Settings : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "SETTINGS"; }
    bool handleBack() override;

private:
    enum SettingId {
        BRIGHTNESS_ACTIVE,
        BRIGHTNESS_DIM,
        DIM_TIMEOUT,
        SLEEP_TIMEOUT,
        SCREEN_FLIP,
        ACCENT_COLOR,
        SHOT_DETECT_MODE,
        SHOT_THRESHOLD,
        SHOT_DELAY_MIN,
        SHOT_DELAY_MAX,
        BUZZER_VOLUME,
        SHOT_ECHO_LOCKOUT,
        SHOT_ECHO_RATIO,
        SHOT_SPECTRAL_ENABLED,
        SHOT_SPECTRAL_RATIO,
        RECOIL_THRESHOLD,
        RECOIL_LOCKOUT,
        RECOIL_RATIO,
        RECOIL_SHARPNESS_ENABLED,
        RECOIL_SHARPNESS,
        CANT_GREEN,
        CANT_YELLOW,
        CANT_CALIB_COUNTDOWN,
        STAB_GREEN,
        STAB_YELLOW,
        STAB_GRAPH_MAX,
        STAB_DEADZONE,
        CPU_FREQ,
        IMU_POLL_DELAY,
        BATT_READ_INTERVAL,
        WIFI_SETUP,
        UPDATE_CHECK,
        RESET_DEFAULTS,
        SETTING_COUNT
    };

    enum class View { CATEGORY_LIST, FIELD_LIST, WIFI_FLOW, OTA_FLOW };

    View view_ = View::CATEGORY_LIST;
    int categoryIndex_ = 0;
    int selectedIndex_ = 0;
    bool editing_ = false;

    uint32_t lastDrawMs_ = 0;

    void adjust(int dir);
    void formatValue(int id, char* buf, size_t n) const;
    const char* label(int id) const;
    int categoryOf(int id) const;
    const char* categoryName(int cat) const;
    int firstFieldOfCategory(int cat) const;
    int stepFieldInCategory(int current, int cat, int dir) const;
    void drawCategoryList();
    void drawFieldList();
    void drawWifiSetup();
    void drawOtaUpdate();
    void draw();

    WifiPortal wifiPortal_;
    OtaUpdater otaUpdater_;
};
