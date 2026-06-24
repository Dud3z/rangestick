#include "Settings.h"
#include <M5Unified.h>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Version.h"
#include <cstdio>
#include <cmath>

namespace {
constexpr int kBlockPitch = 30;
constexpr int kListStartY = 28;
constexpr int kCategoryCount = 5;
constexpr int kVisibleRows = 6; // Kategorien mit mehr Feldern scrollen (siehe draw())

int clampIdx(int idx, int n) {
    if (idx < 0) return 0;
    if (idx >= n) return n - 1;
    return idx;
}
int wrapIdx(int idx, int n) {
    idx %= n;
    if (idx < 0) idx += n;
    return idx;
}
int findU8(const uint8_t* a, int n, uint8_t v) {
    for (int i = 0; i < n; ++i) if (a[i] == v) return i;
    return 0;
}
int findU32(const uint32_t* a, int n, uint32_t v) {
    for (int i = 0; i < n; ++i) if (a[i] == v) return i;
    return 0;
}
int findU16(const uint16_t* a, int n, uint16_t v) {
    for (int i = 0; i < n; ++i) if (a[i] == v) return i;
    return 0;
}
int findFloat(const float* a, int n, float v) {
    for (int i = 0; i < n; ++i) if (fabsf(a[i] - v) < 0.001f) return i;
    return 0;
}
} // namespace

int Settings::categoryOf(int id) const {
    if (id <= SCREEN_FLIP || id == ACCENT_COLOR) return 0;       // Display
    if (id >= SHOT_DETECT_MODE && id <= RECOIL_SHARPNESS) return 1; // Shot-Timer
    if (id >= CANT_GREEN && id <= CANT_CALIB_COUNTDOWN) return 2;  // Anti-Cant
    if (id >= STAB_GREEN && id <= STAB_DEADZONE) return 3;        // Stability
    return 4;                                                     // System
}

const char* Settings::categoryName(int cat) const {
    static const char* names[kCategoryCount] = {"DISPLAY", "SHOT-TIMER", "ANTI-CANT", "STABILITY", "SYSTEM"};
    return names[clampIdx(cat, kCategoryCount)];
}

int Settings::firstFieldOfCategory(int cat) const {
    for (int idx = 0; idx < SETTING_COUNT; ++idx) {
        if (categoryOf(idx) == cat) return idx;
    }
    return 0;
}

int Settings::stepFieldInCategory(int current, int cat, int dir) const {
    int list[SETTING_COUNT];
    int count = 0;
    int curPos = 0;
    for (int idx = 0; idx < SETTING_COUNT; ++idx) {
        if (categoryOf(idx) != cat) continue;
        if (idx == current) curPos = count;
        list[count++] = idx;
    }
    if (count == 0) return current;
    int newPos = (curPos + dir + count) % count;
    return list[newPos];
}

void Settings::onEnter() {
    view_ = View::CATEGORY_LIST;
    categoryIndex_ = 0;
    selectedIndex_ = 0;
    editing_ = false;
    lastDrawMs_ = 0;
}

void Settings::onExit() {
    wifiPortal_.stop();
    otaUpdater_.stop();
    AppSettings::save();
}

bool Settings::handleBack() {
    if (view_ == View::WIFI_FLOW) {
        wifiPortal_.stop();
        view_ = View::FIELD_LIST;
        return true;
    }
    if (view_ == View::OTA_FLOW) {
        otaUpdater_.stop();
        view_ = View::FIELD_LIST;
        return true;
    }
    if (view_ == View::FIELD_LIST) {
        view_ = View::CATEGORY_LIST;
        editing_ = false;
        AppSettings::save(); // evtl. noch nicht gespeicherte Aenderung beim Zurueckgehen sichern
        return true; // selbst behandelt -- Settings bleibt aktiv, main.cpp soll nicht verlassen
    }
    return false; // schon auf der Kategorie-Liste -> main.cpp verlaesst Settings global
}

void Settings::adjust(int dir) {
    switch (selectedIndex_) {
        case BRIGHTNESS_ACTIVE: {
            static const uint8_t opts[] = {60, 100, 140, 180, 220, 255};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU8(opts, n, AppSettings::brightnessActive) + dir, n);
            AppSettings::brightnessActive = opts[idx];
            AppSettings::applyDisplay();
            break;
        }
        case BRIGHTNESS_DIM: {
            static const uint8_t opts[] = {0, 10, 20, 40, 60, 80};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU8(opts, n, AppSettings::brightnessDim) + dir, n);
            AppSettings::brightnessDim = opts[idx];
            break;
        }
        case DIM_TIMEOUT: {
            static const uint32_t opts[] = {10000, 20000, 30000, 60000, AppSettings::kTimeoutNever};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU32(opts, n, AppSettings::dimTimeoutMs) + dir, n);
            AppSettings::dimTimeoutMs = opts[idx];
            break;
        }
        case SLEEP_TIMEOUT: {
            static const uint32_t opts[] = {60000, 120000, 180000, 300000, 600000, AppSettings::kTimeoutNever};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU32(opts, n, AppSettings::sleepTimeoutMs) + dir, n);
            AppSettings::sleepTimeoutMs = opts[idx];
            break;
        }
        case SCREEN_FLIP:
            AppSettings::screenFlipped = !AppSettings::screenFlipped;
            AppSettings::applyDisplay();
            break;
        case ACCENT_COLOR:
            AppSettings::accentColorIndex = wrapIdx(AppSettings::accentColorIndex + dir, AppSettings::kAccentPaletteCount);
            AppSettings::applyDisplay();
            break;
        case SHOT_DETECT_MODE:
            AppSettings::shotDetectMode = 1 - AppSettings::shotDetectMode;
            break;
        case SHOT_THRESHOLD: {
            int v = static_cast<int>(AppSettings::shotThreshold) + dir * 500;
            if (v < 2000) v = 2000;
            if (v > 30000) v = 30000;
            AppSettings::shotThreshold = static_cast<int16_t>(v);
            break;
        }
        case SHOT_DELAY_MIN: {
            static const float opts[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::shotDelayMinS) + dir, n);
            AppSettings::shotDelayMinS = opts[idx];
            if (AppSettings::shotDelayMinS > AppSettings::shotDelayMaxS) AppSettings::shotDelayMaxS = AppSettings::shotDelayMinS;
            break;
        }
        case SHOT_DELAY_MAX: {
            static const float opts[] = {2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::shotDelayMaxS) + dir, n);
            AppSettings::shotDelayMaxS = opts[idx];
            if (AppSettings::shotDelayMaxS < AppSettings::shotDelayMinS) AppSettings::shotDelayMinS = AppSettings::shotDelayMaxS;
            break;
        }
        case BUZZER_VOLUME: {
            static const uint8_t opts[] = {0, 80, 150, 200, 255};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU8(opts, n, AppSettings::buzzerVolume) + dir, n);
            AppSettings::buzzerVolume = opts[idx];
            break;
        }
        case SHOT_ECHO_LOCKOUT: {
            static const uint32_t opts[] = {80, 120, 150, 200, 300, 400};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU32(opts, n, AppSettings::shotEchoLockoutMs) + dir, n);
            AppSettings::shotEchoLockoutMs = opts[idx];
            break;
        }
        case SHOT_ECHO_RATIO: {
            static const float opts[] = {0.3f, 0.4f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::shotEchoRatio) + dir, n);
            AppSettings::shotEchoRatio = opts[idx];
            break;
        }
        case SHOT_SPECTRAL_ENABLED:
            AppSettings::shotSpectralEnabled = !AppSettings::shotSpectralEnabled;
            break;
        case SHOT_SPECTRAL_RATIO: {
            static const float opts[] = {0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.45f, 0.55f, 0.7f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::shotSpectralRatio) + dir, n);
            AppSettings::shotSpectralRatio = opts[idx];
            break;
        }
        case RECOIL_THRESHOLD: {
            int v = static_cast<int>(AppSettings::recoilThresholdMilliG) + dir * 100;
            if (v < 200) v = 200;
            if (v > 8000) v = 8000;
            AppSettings::recoilThresholdMilliG = static_cast<int16_t>(v);
            break;
        }
        case RECOIL_LOCKOUT: {
            static const uint32_t opts[] = {80, 120, 150, 200, 300, 400};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU32(opts, n, AppSettings::recoilLockoutMs) + dir, n);
            AppSettings::recoilLockoutMs = opts[idx];
            break;
        }
        case RECOIL_RATIO: {
            static const float opts[] = {0.3f, 0.4f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::recoilRatio) + dir, n);
            AppSettings::recoilRatio = opts[idx];
            break;
        }
        case RECOIL_SHARPNESS_ENABLED:
            AppSettings::recoilSharpnessEnabled = !AppSettings::recoilSharpnessEnabled;
            break;
        case RECOIL_SHARPNESS: {
            static const float opts[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::recoilSharpness) + dir, n);
            AppSettings::recoilSharpness = opts[idx];
            break;
        }
        case CANT_GREEN: {
            static const float opts[] = {0.5f, 1.0f, 1.5f, 2.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::cantGreenDeg) + dir, n);
            AppSettings::cantGreenDeg = opts[idx];
            if (AppSettings::cantGreenDeg > AppSettings::cantYellowDeg) AppSettings::cantYellowDeg = AppSettings::cantGreenDeg;
            break;
        }
        case CANT_YELLOW: {
            static const float opts[] = {2.0f, 3.0f, 4.0f, 5.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::cantYellowDeg) + dir, n);
            AppSettings::cantYellowDeg = opts[idx];
            if (AppSettings::cantYellowDeg < AppSettings::cantGreenDeg) AppSettings::cantGreenDeg = AppSettings::cantYellowDeg;
            break;
        }
        case CANT_CALIB_COUNTDOWN: {
            static const float opts[] = {0.0f, 1.0f, 2.0f, 3.0f, 5.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::cantCalibCountdownS) + dir, n);
            AppSettings::cantCalibCountdownS = opts[idx];
            break;
        }
        case STAB_GREEN: {
            static const float opts[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::stabilityGreenMoa) + dir, n);
            AppSettings::stabilityGreenMoa = opts[idx];
            if (AppSettings::stabilityGreenMoa > AppSettings::stabilityYellowMoa) AppSettings::stabilityYellowMoa = AppSettings::stabilityGreenMoa;
            break;
        }
        case STAB_YELLOW: {
            static const float opts[] = {3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::stabilityYellowMoa) + dir, n);
            AppSettings::stabilityYellowMoa = opts[idx];
            if (AppSettings::stabilityYellowMoa < AppSettings::stabilityGreenMoa) AppSettings::stabilityGreenMoa = AppSettings::stabilityYellowMoa;
            break;
        }
        case STAB_GRAPH_MAX: {
            static const float opts[] = {4.0f, 6.0f, 8.0f, 10.0f, 12.0f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::stabilityGraphMaxMoa) + dir, n);
            AppSettings::stabilityGraphMaxMoa = opts[idx];
            break;
        }
        case STAB_DEADZONE: {
            static const float opts[] = {0.0f, 0.2f, 0.3f, 0.5f, 0.8f, 1.2f, 1.8f, 2.5f};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findFloat(opts, n, AppSettings::stabilityDeadzoneMoa) + dir, n);
            AppSettings::stabilityDeadzoneMoa = opts[idx];
            break;
        }
        case CPU_FREQ: {
            static const uint16_t opts[] = {80, 160, 240};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU16(opts, n, AppSettings::cpuFreqMhz) + dir, n);
            AppSettings::cpuFreqMhz = opts[idx];
            AppSettings::applyCpuFreq();
            break;
        }
        case IMU_POLL_DELAY: {
            static const uint16_t opts[] = {0, 2, 5, 10, 20};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU16(opts, n, AppSettings::imuPollDelayMs) + dir, n);
            AppSettings::imuPollDelayMs = opts[idx];
            break;
        }
        case BATT_READ_INTERVAL: {
            static const uint16_t opts[] = {0, 250, 500, 1000, 2000, 5000};
            int n = sizeof(opts) / sizeof(opts[0]);
            int idx = clampIdx(findU16(opts, n, AppSettings::battReadIntervalMs) + dir, n);
            AppSettings::battReadIntervalMs = opts[idx];
            break;
        }
        default:
            break;
    }
}

const char* Settings::label(int id) const {
    static const char* labels[SETTING_COUNT] = {
        "Helligkeit aktiv", "Helligkeit gedimmt", "Dimmen nach", "Ausschalten nach",
        "Bildschirm", "Akzentfarbe",
        "Erkennungsmodus", "Schwelle (Zahl)", "Delay min", "Delay max",
        "Buzzer-Lautstaerke", "Echo-Sperre", "Echo-Verhaeltnis", "Frequenzanalyse",
        "Spektral-Schwelle",
        "Rueckstoss-Schwelle", "Rueckstoss-Sperre", "Rueckstoss-Verh.", "Schaerfe-Filter", "Schaerfe-Schwelle",
        "Gruen-Schwelle", "Gelb-Schwelle",
        "Kalibrier-Countdown", "Gruen-Schwelle", "Gelb-Schwelle", "Graph-Maximum", "Totzone",
        "CPU-Takt", "IMU-Poll-Pause", "Akku-Leseintervall",
        "WLAN einrichten", "Update pruefen",
        "Zuruecksetzen",
    };
    return labels[id];
}

void Settings::formatValue(int id, char* buf, size_t n) const {
    switch (id) {
        case BRIGHTNESS_ACTIVE: snprintf(buf, n, "%d", AppSettings::brightnessActive); break;
        case BRIGHTNESS_DIM: snprintf(buf, n, "%d", AppSettings::brightnessDim); break;
        case DIM_TIMEOUT:
            if (AppSettings::dimTimeoutMs == AppSettings::kTimeoutNever) snprintf(buf, n, "Aus");
            else snprintf(buf, n, "%us", static_cast<unsigned>(AppSettings::dimTimeoutMs / 1000));
            break;
        case SLEEP_TIMEOUT:
            if (AppSettings::sleepTimeoutMs == AppSettings::kTimeoutNever) snprintf(buf, n, "Aus");
            else snprintf(buf, n, "%umin", static_cast<unsigned>(AppSettings::sleepTimeoutMs / 60000));
            break;
        case SCREEN_FLIP: snprintf(buf, n, "%s", AppSettings::screenFlipped ? "180 Grad" : "Normal"); break;
        case ACCENT_COLOR: {
            static const char* names[] = {"Cyan", "Orange", "Gruen", "Lila", "Rot", "Blau", "Gelb", "Pink"};
            snprintf(buf, n, "%s", names[AppSettings::accentColorIndex]);
            break;
        }
        case SHOT_DETECT_MODE: snprintf(buf, n, "%s", AppSettings::shotDetectMode == 0 ? "Mikrofon" : "Rueckstoss"); break;
        case SHOT_THRESHOLD: snprintf(buf, n, "%d", AppSettings::shotThreshold); break;
        case SHOT_DELAY_MIN: snprintf(buf, n, "%.1fs", static_cast<double>(AppSettings::shotDelayMinS)); break;
        case SHOT_DELAY_MAX: snprintf(buf, n, "%.1fs", static_cast<double>(AppSettings::shotDelayMaxS)); break;
        case BUZZER_VOLUME:
            if (AppSettings::buzzerVolume == 0) snprintf(buf, n, "Stumm");
            else snprintf(buf, n, "%d", AppSettings::buzzerVolume);
            break;
        case SHOT_ECHO_LOCKOUT: snprintf(buf, n, "%ums", static_cast<unsigned>(AppSettings::shotEchoLockoutMs)); break;
        case SHOT_ECHO_RATIO: snprintf(buf, n, "%.0f%%", static_cast<double>(AppSettings::shotEchoRatio * 100.0f)); break;
        case SHOT_SPECTRAL_ENABLED: snprintf(buf, n, "%s", AppSettings::shotSpectralEnabled ? "An" : "Aus"); break;
        case SHOT_SPECTRAL_RATIO: snprintf(buf, n, "%.0f%%", static_cast<double>(AppSettings::shotSpectralRatio * 100.0f)); break;
        case RECOIL_THRESHOLD: snprintf(buf, n, "%.1fg", static_cast<double>(AppSettings::recoilThresholdMilliG) / 1000.0); break;
        case RECOIL_LOCKOUT: snprintf(buf, n, "%ums", static_cast<unsigned>(AppSettings::recoilLockoutMs)); break;
        case RECOIL_RATIO: snprintf(buf, n, "%.0f%%", static_cast<double>(AppSettings::recoilRatio * 100.0f)); break;
        case RECOIL_SHARPNESS_ENABLED: snprintf(buf, n, "%s", AppSettings::recoilSharpnessEnabled ? "An" : "Aus"); break;
        case RECOIL_SHARPNESS: snprintf(buf, n, "%.0f%%", static_cast<double>(AppSettings::recoilSharpness * 100.0f)); break;
        case CANT_GREEN: snprintf(buf, n, "%.1f deg", static_cast<double>(AppSettings::cantGreenDeg)); break;
        case CANT_YELLOW: snprintf(buf, n, "%.1f deg", static_cast<double>(AppSettings::cantYellowDeg)); break;
        case CANT_CALIB_COUNTDOWN:
            if (AppSettings::cantCalibCountdownS <= 0.0f) snprintf(buf, n, "Aus");
            else snprintf(buf, n, "%.0fs", static_cast<double>(AppSettings::cantCalibCountdownS));
            break;
        case STAB_GREEN: snprintf(buf, n, "%.1f MOA", static_cast<double>(AppSettings::stabilityGreenMoa)); break;
        case STAB_YELLOW: snprintf(buf, n, "%.1f MOA", static_cast<double>(AppSettings::stabilityYellowMoa)); break;
        case STAB_GRAPH_MAX: snprintf(buf, n, "%.0f MOA", static_cast<double>(AppSettings::stabilityGraphMaxMoa)); break;
        case STAB_DEADZONE: snprintf(buf, n, "%.1f MOA", static_cast<double>(AppSettings::stabilityDeadzoneMoa)); break;
        case CPU_FREQ: snprintf(buf, n, "%uMHz", AppSettings::cpuFreqMhz); break;
        case IMU_POLL_DELAY:
            if (AppSettings::imuPollDelayMs == 0) snprintf(buf, n, "Aus");
            else snprintf(buf, n, "%ums", AppSettings::imuPollDelayMs);
            break;
        case BATT_READ_INTERVAL:
            if (AppSettings::battReadIntervalMs == 0) snprintf(buf, n, "Jeder Redraw");
            else snprintf(buf, n, "%ums", AppSettings::battReadIntervalMs);
            break;
        case WIFI_SETUP: snprintf(buf, n, "A druecken"); break;
        case UPDATE_CHECK: snprintf(buf, n, "A druecken"); break;
        case RESET_DEFAULTS: snprintf(buf, n, "A druecken"); break;
        default: buf[0] = 0; break;
    }
}

void Settings::loop() {
    // A lang = zuerst handleBack() (eine Ebene innerhalb von Settings zurueck), erst danach
    // global "zurueck ins Hauptmenue" (main.cpp) -- siehe Kommentar dort und in handleBack().
    if (view_ == View::WIFI_FLOW) {
        wifiPortal_.loop();
    } else if (view_ == View::OTA_FLOW) {
        otaUpdater_.loop();
        if (M5.BtnA.wasReleased() && otaUpdater_.state() == OtaUpdater::State::UPDATE_AVAILABLE) {
            otaUpdater_.confirm();
        }
    } else if (view_ == View::CATEGORY_LIST) {
        if (M5.BtnB.wasPressed()) {
            categoryIndex_ = (categoryIndex_ - 1 + kCategoryCount) % kCategoryCount; // Hoch
        } else if (M5.BtnPWR.wasClicked()) {
            categoryIndex_ = (categoryIndex_ + 1) % kCategoryCount; // Runter
        } else if (M5.BtnA.wasReleased()) {
            selectedIndex_ = firstFieldOfCategory(categoryIndex_);
            editing_ = false;
            view_ = View::FIELD_LIST;
        }
    } else { // View::FIELD_LIST
        if (M5.BtnA.wasReleased()) {
            if (selectedIndex_ == RESET_DEFAULTS) {
                AppSettings::resetDefaults();
                AppSettings::applyDisplay();
                AppSettings::save();
            } else if (selectedIndex_ == WIFI_SETUP) {
                wifiPortal_.start();
                view_ = View::WIFI_FLOW;
            } else if (selectedIndex_ == UPDATE_CHECK) {
                otaUpdater_.start();
                view_ = View::OTA_FLOW;
            } else if (!editing_) {
                editing_ = true;
            } else {
                editing_ = false;
                AppSettings::save();
            }
        } else if (M5.BtnB.wasPressed()) {
            if (!editing_) {
                selectedIndex_ = stepFieldInCategory(selectedIndex_, categoryIndex_, -1); // Hoch
            } else {
                adjust(+1); // Hoch = Wert+
            }
        } else if (M5.BtnPWR.wasClicked()) {
            if (!editing_) {
                selectedIndex_ = stepFieldInCategory(selectedIndex_, categoryIndex_, +1); // Runter
            } else {
                adjust(-1); // Runter = Wert-
            }
        }
    }

    uint32_t now = millis();
    if (now - lastDrawMs_ >= 80) {
        lastDrawMs_ = now;
        draw();
    }
}

void Settings::drawCategoryList() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setCursor(4, 2);
    canvas.print("SETTINGS");

    canvas.setTextSize(2);
    for (int i = 0; i < kCategoryCount; ++i) {
        int y = 32 + i * 32;
        bool sel = (i == categoryIndex_);
        if (sel) {
            canvas.fillRoundRect(0, y - 4, canvas.width(), 28, 3, Theme::PANEL);
            canvas.setTextColor(Theme::ACCENT, Theme::PANEL);
        } else {
            canvas.setTextColor(Theme::TEXT, Theme::BG);
        }
        canvas.setCursor(8, y);
        canvas.print(categoryName(i));
    }

    canvas.setTextSize(1);
    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 216);
    canvas.print("B=Hoch PWR=Runter");
    canvas.setCursor(4, 228);
    canvas.print("A = oeffnen");
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void Settings::drawFieldList() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setCursor(4, 2);
    canvas.print("SETTINGS");

    canvas.setTextColor(Theme::TEXT, Theme::BG);
    canvas.setCursor(4, 14);
    canvas.print(categoryName(categoryIndex_));

    // Lokalen Index von selectedIndex_ innerhalb der Kategorie und Feldanzahl der Kategorie
    // ermitteln, um -- falls mehr Felder vorhanden sind als Zeilen passen -- die Auswahl per
    // Scroll-Fenster sichtbar zu halten, statt unten aus dem Bildschirm zu laufen.
    int localSelected = 0;
    int categoryFieldCount = 0;
    for (int idx = 0; idx < SETTING_COUNT; ++idx) {
        if (categoryOf(idx) != categoryIndex_) continue;
        if (idx == selectedIndex_) localSelected = categoryFieldCount;
        categoryFieldCount++;
    }

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    char posBuf[8];
    snprintf(posBuf, sizeof(posBuf), "%d/%d", localSelected + 1, categoryFieldCount);
    int pw = canvas.textWidth(posBuf);
    canvas.setCursor(canvas.width() - pw - 4, 14);
    canvas.print(posBuf);

    int scrollOffset = 0;
    if (categoryFieldCount > kVisibleRows) {
        scrollOffset = localSelected - kVisibleRows / 2;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > categoryFieldCount - kVisibleRows) scrollOffset = categoryFieldCount - kVisibleRows;
    }

    int row = 0;
    int localIdx = 0;
    for (int idx = 0; idx < SETTING_COUNT; ++idx) {
        if (categoryOf(idx) != categoryIndex_) continue;
        if (localIdx < scrollOffset || localIdx >= scrollOffset + kVisibleRows) {
            ++localIdx;
            continue;
        }
        ++localIdx;
        int y = kListStartY + row * kBlockPitch;
        bool sel = (idx == selectedIndex_);

        if (sel) {
            uint16_t hl = editing_ ? Theme::ACCENT2 : Theme::PANEL;
            canvas.fillRoundRect(0, y - 2, canvas.width(), kBlockPitch - 2, 3, hl);
            canvas.setTextColor(editing_ ? Theme::BG : Theme::SUBTEXT, hl);
        } else {
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
        }
        canvas.setTextSize(1);
        canvas.setCursor(4, y);
        canvas.print(label(idx));

        char valBuf[16];
        formatValue(idx, valBuf, sizeof(valBuf));
        canvas.setTextSize(2);
        canvas.setTextColor(sel ? Theme::ACCENT : Theme::TEXT, sel ? (editing_ ? Theme::ACCENT2 : Theme::PANEL) : Theme::BG);
        canvas.setCursor(4, y + 10);
        canvas.print(valBuf);
        canvas.setTextSize(1);

        ++row;
    }

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    if (!editing_) {
        canvas.setCursor(4, 216);
        canvas.print("B=Hoch PWR=Runter");
        canvas.setCursor(4, 228);
        canvas.print("A = bearbeiten");
    } else {
        canvas.setCursor(4, 216);
        canvas.print("B=+  PWR=-");
        canvas.setCursor(4, 228);
        canvas.print("A = speichern");
    }
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void Settings::drawWifiSetup() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("WLAN EINRICHTEN");

    switch (wifiPortal_.state()) {
        case WifiPortal::State::SAVED_OK:
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 60);
            canvas.print("Verbunden!");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 100);
            canvas.print("WLAN-Daten gespeichert.");
            break;
        case WifiPortal::State::SAVED_FAIL:
            canvas.setTextColor(Theme::BAD, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 50);
            canvas.print("Fehlgeschlagen");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 90);
            canvas.print("Passwort falsch?");
            canvas.setCursor(4, 102);
            canvas.print("Im Browser neu versuchen.");
            break;
        default: // RUNNING
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, 40);
            canvas.print("1. Mit Handy verbinden:");
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 54);
            canvas.print(WifiPortal::apSsid());
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, 90);
            canvas.print("2. Browser oeffnen:");
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 104);
            canvas.print(wifiPortal_.apIp());
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 140);
            canvas.print("(oeffnet sich oft automatisch)");
            break;
    }

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 228);
    canvas.print("A halten = zurueck");
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void Settings::drawOtaUpdate() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("UPDATE");

    switch (otaUpdater_.state()) {
        case OtaUpdater::State::CONNECTING:
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 70);
            canvas.print("Verbinde...");
            break;
        case OtaUpdater::State::CHECKING:
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 70);
            canvas.print("Pruefe...");
            break;
        case OtaUpdater::State::UP_TO_DATE:
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 60);
            canvas.print("Aktuell");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 95);
            canvas.printf("Version %s", Version::FW_VERSION);
            break;
        case OtaUpdater::State::UPDATE_AVAILABLE:
            canvas.setTextColor(Theme::ACCENT2, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 50);
            canvas.print("Update!");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, 85);
            canvas.printf("%s -> %s", Version::FW_VERSION, otaUpdater_.remoteVersion());
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 105);
            canvas.print("A = jetzt laden");
            break;
        case OtaUpdater::State::DOWNLOADING:
        case OtaUpdater::State::DONE:
            // Diese Zustaende zeichnen waehrend des blockierenden Downloads ihr eigenes
            // Fortschritts-UI direkt (siehe OtaUpdater::drawMessage) -- hier nichts zu tun.
            break;
        case OtaUpdater::State::ERROR:
            canvas.setTextColor(Theme::BAD, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(8, 60);
            canvas.print("Fehler");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 95);
            canvas.print(otaUpdater_.errorMessage());
            break;
        default:
            break;
    }

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 228);
    canvas.print("A halten = zurueck");
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void Settings::draw() {
    switch (view_) {
        case View::CATEGORY_LIST: drawCategoryList(); break;
        case View::FIELD_LIST: drawFieldList(); break;
        case View::WIFI_FLOW: drawWifiSetup(); break;
        case View::OTA_FLOW: drawOtaUpdate(); break;
    }
}
