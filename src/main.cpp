#include <M5Unified.h>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Globals.h"
#include "AppModule.h"
#include "ShotTimer.h"
#include "CantLevel.h"
#include "StabilityTracker.h"
#include "ComboView.h"
#include "Settings.h"
#include "Version.h"
#include <cmath>
#include <cstring>

M5Canvas canvas(&M5.Display);

// Shared calculation engines for anti-cant/stability -- see Globals.h. Defined here so both the
// standalone modules and ComboView use the same instance (and therefore the same calibration/
// session state).
CantCalculator gCantCalc;
StabilityCalculator gStabilityCalc;

// WiFi connection made via Settings > NETWORK > "Connect to WiFi" -- see Globals.h for why this
// is global instead of owned by Settings.
WifiConnector gWifiConnector;

namespace {

ShotTimer shotTimer;
CantLevel cantLevel;
StabilityTracker stabilityTracker;
ComboView comboView;
Settings settings;

AppModule* kModules[] = {&shotTimer, &cantLevel, &stabilityTracker, &comboView, &settings};
constexpr int kModuleCount = sizeof(kModules) / sizeof(kModules[0]);

constexpr int kRowSpacing = 32;
constexpr int kRowFirstY = 44;

int menuIndex = 0;
AppModule* activeModule = nullptr;

float selAnimY = kRowFirstY;
bool menuAnimating = false;
uint32_t lastMenuDrawMs = 0;

// App-wide button layout: B = up, power button = down, short A = confirm,
// long A = back/menu (handled centrally here, see loop()).
bool aLongFired = false;

// Display power saving: after inactivity, dim first, then go fully dark (the battery is small at
// ~200mAh). Any button press wakes it up immediately -- the waking press itself is discarded so
// that waking up doesn't accidentally do something like arm the shot timer. Times/brightness
// levels come from AppSettings (Settings module).
enum class PowerState { ACTIVE, DIMMED, ASLEEP };
PowerState powerState = PowerState::ACTIVE;
uint32_t lastActivityMs = 0;

// Returns true if a button press in this frame was only meant to wake the screen and should be
// ignored by the actual menu/module logic. "busy" (e.g. a running shot timer, live measurement in
// anti-cant/stability) keeps the screen bright even without a button press -- only genuine
// inactivity (nothing pressed AND nothing active) dims it.
bool updatePowerState(bool buttonPressed, bool busy) {
    uint32_t now = millis();
    bool wasAsleep = (powerState != PowerState::ACTIVE);

    if (buttonPressed || busy) {
        if (wasAsleep) {
            powerState = PowerState::ACTIVE;
            M5.Display.setBrightness(AppSettings::brightnessActive);
        }
        lastActivityMs = now;
        return buttonPressed && wasAsleep;
    }

    uint32_t idle = now - lastActivityMs;
    if (powerState == PowerState::ACTIVE &&
        AppSettings::dimTimeoutMs != AppSettings::kTimeoutNever &&
        idle > AppSettings::dimTimeoutMs) {
        powerState = PowerState::DIMMED;
        M5.Display.setBrightness(AppSettings::brightnessDim);
    } else if (powerState == PowerState::DIMMED &&
               AppSettings::sleepTimeoutMs != AppSettings::kTimeoutNever &&
               idle > AppSettings::sleepTimeoutMs) {
        powerState = PowerState::ASLEEP;
        M5.Display.setBrightness(0);
    }
    return false;
}

int rowY(int i) { return kRowFirstY + i * kRowSpacing; }

// Small vector icons per menu entry, drawn left of the list text.
void drawIcon(int index, int cx, int cy, uint16_t color) {
    switch (index) {
        case 0: // Shot Timer -- stopwatch
            canvas.drawCircle(cx, cy, 9, color);
            canvas.fillRect(cx - 2, cy - 13, 4, 3, color);
            canvas.drawLine(cx, cy, cx, cy - 5, color);
            canvas.drawLine(cx, cy, cx + 4, cy - 2, color);
            break;
        case 1: // Anti-Cant -- spirit level
            canvas.drawRoundRect(cx - 11, cy - 6, 22, 12, 3, color);
            canvas.drawFastHLine(cx - 8, cy, 16, color);
            canvas.fillCircle(cx + 3, cy, 2, color);
            break;
        case 2: // Stability -- crosshair
            canvas.drawCircle(cx, cy, 9, color);
            canvas.drawCircle(cx, cy, 4, color);
            canvas.drawFastHLine(cx - 13, cy, 6, color);
            canvas.drawFastHLine(cx + 7, cy, 6, color);
            canvas.drawFastVLine(cx, cy - 13, 6, color);
            canvas.drawFastVLine(cx, cy + 7, 6, color);
            break;
        case 3: // Combo -- stacked spirit level + crosshair
            canvas.drawRoundRect(cx - 11, cy - 10, 22, 8, 2, color);
            canvas.drawFastHLine(cx - 8, cy - 6, 16, color);
            canvas.drawCircle(cx, cy + 6, 6, color);
            canvas.drawFastHLine(cx - 4, cy + 6, 8, color);
            canvas.drawFastVLine(cx, cy + 2, 8, color);
            break;
        case 4: // Settings -- gear
            canvas.drawCircle(cx, cy, 8, color);
            canvas.drawCircle(cx, cy, 3, color);
            for (int a = 0; a < 360; a += 45) {
                float rad = a * 3.14159265f / 180.0f;
                int x0 = cx + static_cast<int>(8 * cosf(rad));
                int y0 = cy + static_cast<int>(8 * sinf(rad));
                int x1 = cx + static_cast<int>(12 * cosf(rad));
                int y1 = cy + static_cast<int>(12 * sinf(rad));
                canvas.drawLine(x0, y0, x1, y1, color);
            }
            break;
    }
}

// Boot animation matching the shooting theme: a round arcs across the screen and hits a target,
// leaving a fading trail, then RANGESTICK is "zeroed in" letter by letter. Silent -- no boot beep.
void splash() {
    constexpr int groundY = 150;
    constexpr int startX = 8;
    constexpr int targetX = 100;
    constexpr int peakLift = 85; // how far above groundY the arc peaks

    constexpr int kFlightSteps = 36;
    int trailX[kFlightSteps + 1];
    int trailY[kFlightSteps + 1];
    int trailCount = 0;

    for (int step = 0; step <= kFlightSteps; ++step) {
        float t = static_cast<float>(step) / kFlightSteps;
        int x = startX + static_cast<int>(t * (targetX - startX));
        int y = groundY - static_cast<int>(peakLift * 4.0f * t * (1.0f - t)); // parabolic arc

        canvas.fillScreen(Theme::BG);
        canvas.drawCircle(targetX, groundY, 14, Theme::SUBTEXT);
        canvas.drawCircle(targetX, groundY, 7, Theme::SUBTEXT);

        trailX[trailCount] = x;
        trailY[trailCount] = y;
        ++trailCount;
        for (int i = 0; i < trailCount - 1; ++i) {
            canvas.fillCircle(trailX[i], trailY[i], 1, Theme::SUBTEXT);
        }
        canvas.fillCircle(x, y, 3, Theme::ACCENT);

        canvas.pushSprite(0, 0);
        delay(16);
    }

    // Silent impact burst -- an expanding ring instead of the old flash+beep.
    for (int r = 2; r <= 32; r += 5) {
        canvas.fillScreen(Theme::BG);
        canvas.fillCircle(targetX, groundY, 7, Theme::ACCENT2);
        canvas.drawCircle(targetX, groundY, r, Theme::ACCENT2);
        canvas.pushSprite(0, 0);
        delay(22);
    }
    delay(150);
    canvas.fillScreen(Theme::BG);
    canvas.pushSprite(0, 0);
    delay(120);

    const char* line1 = "RANGE";
    const char* line2 = "STICK";
    constexpr int kCharW = 18; // width per character at textSize(3) (6px base width * 3)
    size_t len1 = strlen(line1);
    size_t len2 = strlen(line2);
    int x1 = (canvas.width() - static_cast<int>(len1) * kCharW) / 2;
    int x2 = (canvas.width() - static_cast<int>(len2) * kCharW) / 2;
    int y1 = 76;
    int y2 = y1 + 36;

    canvas.setTextSize(3);
    size_t maxLen = (len1 > len2) ? len1 : len2;
    for (size_t i = 0; i < maxLen; ++i) {
        if (i < len1) {
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setCursor(x1 + static_cast<int>(i) * kCharW, y1);
            canvas.print(line1[i]);
        }
        if (i < len2) {
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setCursor(x2 + static_cast<int>(i) * kCharW, y2);
            canvas.print(line2[i]);
        }
        canvas.pushSprite(0, 0);
        delay(90);
    }
    delay(150);

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setTextSize(1);
    char verBuf[16];
    snprintf(verBuf, sizeof(verBuf), "v%s", Version::FW_VERSION);
    int vw = canvas.textWidth(verBuf);
    canvas.setCursor((canvas.width() - vw) / 2, 200);
    canvas.print(verBuf);
    canvas.pushSprite(0, 0);
    delay(700);
}

void drawMenu() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setCursor(4, 4);
    canvas.print("RANGESTICK");

    // Animated selection bar, glides to the target row instead of jumping.
    canvas.fillRoundRect(2, static_cast<int>(selAnimY) - 8, canvas.width() - 4, 30, 4, Theme::PANEL);
    canvas.drawRoundRect(2, static_cast<int>(selAnimY) - 8, canvas.width() - 4, 30, 4, Theme::ACCENT);

    canvas.setTextSize(1);
    for (int i = 0; i < kModuleCount; ++i) {
        // Highlight band for row i spans [rowY(i)-8, rowY(i)+22], centered at rowY(i)+7.
        int y = rowY(i) + 3; // 8px tall text centered within this band
        bool sel = (i == menuIndex);
        uint16_t color = sel ? Theme::ACCENT : Theme::TEXT;
        drawIcon(i, 18, rowY(i) + 7, color);
        canvas.setTextColor(color, sel ? Theme::PANEL : Theme::BG);
        // Text printed twice with a 1px offset -- a cheap "bold" effect, since setTextSize(2)
        // would wrap the labels onto the next line at 135px width.
        canvas.setCursor(34, y);
        canvas.print(kModules[i]->name());
        canvas.setCursor(35, y);
        canvas.print(kModules[i]->name());
    }

    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void loopMenu() {
    if (M5.BtnB.wasPressed()) {
        menuIndex = (menuIndex - 1 + kModuleCount) % kModuleCount; // up
        menuAnimating = true;
    } else if (M5.BtnPWR.wasClicked()) {
        menuIndex = (menuIndex + 1) % kModuleCount; // down
        menuAnimating = true;
    } else if (M5.BtnA.wasReleased()) {
        activeModule = kModules[menuIndex];
        activeModule->onEnter();
        return;
    }

    uint32_t now = millis();
    if (now - lastMenuDrawMs < 16) return; // ~60fps cap
    lastMenuDrawMs = now;

    if (menuAnimating) {
        float target = static_cast<float>(rowY(menuIndex));
        selAnimY += (target - selAnimY) * 0.35f;
        if (fabsf(target - selAnimY) < 0.5f) {
            selAnimY = target;
            menuAnimating = false;
        }
        drawMenu();
    }
}

} // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    AppSettings::load();
    AppSettings::applyCpuFreq();  // battery setting: set clock rate before any further setup
    AppSettings::applyDisplay(); // rotation, brightness, accent color from saved settings
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    splash();
    selAnimY = static_cast<float>(rowY(menuIndex));
    lastActivityMs = millis();
    drawMenu();
}

void loop() {
    // The only M5.update() call per pass: modules don't call it themselves, otherwise a second
    // call would overwrite the just-detected button edge.
    M5.update();

    // Services the settings web server whenever a WifiConnector STA connection is active,
    // independent of which module/screen is currently shown -- see Globals.h.
    gWifiConnector.serviceServer();

    bool buttonActivity = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() ||
                           M5.BtnPWR.wasClicked() || M5.BtnPWR.wasHold();
    bool busy = (activeModule != nullptr) && activeModule->isBusy();
    if (updatePowerState(buttonActivity, busy)) {
        return; // this button press was only the wake-up, don't process it further
    }

    // Long press on A: first ask whether the module itself has a level to go back to (e.g.
    // Settings' field list -> category list); only if not, it exits to the main menu. Detected
    // centrally here so modules themselves only ever see the short press (wasReleased) as
    // "confirm". If the long press fires here, the module/menu doesn't get to see this frame at
    // all (return) -- so a long press can never also additionally trigger the module's short-
    // press action.
    if (M5.BtnA.pressedFor(600) && !aLongFired) {
        aLongFired = true;
        if (activeModule != nullptr) {
            if (!activeModule->handleBack()) {
                activeModule->onExit();
                activeModule = nullptr;
                menuAnimating = false;
                drawMenu();
            }
        }
        return;
    }
    if (M5.BtnA.wasReleased()) {
        bool wasLong = aLongFired;
        aLongFired = false;
        if (wasLong) return; // was a long press -- don't also trigger a short-press action
    }

    if (activeModule == nullptr) {
        loopMenu();
        return;
    }

    activeModule->loop();
}
