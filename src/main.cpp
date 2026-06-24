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
#include <cmath>
#include <cstring>

M5Canvas canvas(&M5.Display);

// Geteilte Berechnungs-Engines fuer Anti-Cant/Stability -- siehe Globals.h. Hier definiert,
// damit sowohl die Einzelmodule als auch ComboView dieselbe Instanz (und damit denselben
// Kalibrierungs-/Session-Zustand) verwenden.
CantCalculator gCantCalc;
StabilityCalculator gStabilityCalc;

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

// Tastenbelegung app-weit: B = Hoch, Power-Knopf = Runter, A kurz = Bestaetigen,
// A lang = Zurueck/Menue (zentral hier behandelt, siehe loop()).
bool aLongFired = false;

// Display-Energiesparen: nach Inaktivitaet erst dimmen, dann ganz abdunkeln (Akku ist mit
// ~200mAh klein). Jeder Tastendruck weckt sofort wieder auf -- der weckende Druck selbst
// wird dabei verworfen, damit man durch das Aufwecken nicht versehentlich z.B. den
// Shot-Timer scharf schaltet. Zeiten/Helligkeiten kommen aus AppSettings (Settings-Modul).
enum class PowerState { ACTIVE, DIMMED, ASLEEP };
PowerState powerState = PowerState::ACTIVE;
uint32_t lastActivityMs = 0;

// Gibt true zurueck, wenn ein Tastendruck in diesem Frame nur zum Aufwecken diente und
// von der eigentlichen Menue-/Modul-Logik ignoriert werden soll. "busy" (z.B. laufender
// Shot-Timer, Live-Messung in Anti-Cant/Stability) haelt den Bildschirm hell, auch ohne
// Tastendruck -- nur echte Untaetigkeit (nichts gedrueckt UND nichts aktiv) dimmt.
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

// Kleine Vektor-Icons je Menuepunkt, gezeichnet links neben dem Listentext.
void drawIcon(int index, int cx, int cy, uint16_t color) {
    switch (index) {
        case 0: // Shot Timer -- Stoppuhr
            canvas.drawCircle(cx, cy, 9, color);
            canvas.fillRect(cx - 2, cy - 13, 4, 3, color);
            canvas.drawLine(cx, cy, cx, cy - 5, color);
            canvas.drawLine(cx, cy, cx + 4, cy - 2, color);
            break;
        case 1: // Anti-Cant -- Wasserwaage
            canvas.drawRoundRect(cx - 11, cy - 6, 22, 12, 3, color);
            canvas.drawFastHLine(cx - 8, cy, 16, color);
            canvas.fillCircle(cx + 3, cy, 2, color);
            break;
        case 2: // Stability -- Fadenkreuz
            canvas.drawCircle(cx, cy, 9, color);
            canvas.drawCircle(cx, cy, 4, color);
            canvas.drawFastHLine(cx - 13, cy, 6, color);
            canvas.drawFastHLine(cx + 7, cy, 6, color);
            canvas.drawFastVLine(cx, cy - 13, 6, color);
            canvas.drawFastVLine(cx, cy + 7, 6, color);
            break;
        case 3: // Combo -- gestapelte Wasserwaage + Fadenkreuz
            canvas.drawRoundRect(cx - 11, cy - 10, 22, 8, 2, color);
            canvas.drawFastHLine(cx - 8, cy - 6, 16, color);
            canvas.drawCircle(cx, cy + 6, 6, color);
            canvas.drawFastHLine(cx - 4, cy + 6, 8, color);
            canvas.drawFastVLine(cx, cy + 2, 8, color);
            break;
        case 4: // Settings -- Zahnrad
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

// Bootanimation passend zum Shooting-Thema: ein Fadenkreuz zoomt wie ein einrastendes
// Zielfernrohr nach aussen, "loest" mit Blitz+Beep aus, dann wird RANGESTICK Buchstabe fuer
// Buchstabe "eingeschossen" -- je ein kleiner Einschuss-Punkt ueber dem Buchstaben markiert den
// Treffer.
void splash() {
    constexpr int cx = 67;
    constexpr int cy = 104;

    constexpr int kZoomSteps = 24;
    for (int step = 0; step <= kZoomSteps; ++step) {
        float t = static_cast<float>(step) / kZoomSteps;
        int r = 4 + static_cast<int>(t * 70.0f);
        float rotDeg = (1.0f - t) * 50.0f; // dreht sich beim Einrasten auf 0 Grad ein
        canvas.fillScreen(Theme::BG);
        canvas.drawCircle(cx, cy, r, Theme::ACCENT);
        for (int a = 0; a < 360; a += 90) {
            float rad = (a + rotDeg) * 3.14159265f / 180.0f;
            int x0 = cx + static_cast<int>((r - 14) * cosf(rad));
            int y0 = cy + static_cast<int>((r - 14) * sinf(rad));
            int x1 = cx + static_cast<int>((r + 14) * cosf(rad));
            int y1 = cy + static_cast<int>((r + 14) * sinf(rad));
            canvas.drawLine(x0, y0, x1, y1, Theme::ACCENT);
        }
        canvas.pushSprite(0, 0);
        delay(28);
    }
    delay(250); // kurz auf dem eingerasteten Fadenkreuz verweilen, bevor "geschossen" wird

    M5.Speaker.begin();
    M5.Speaker.setVolume(AppSettings::buzzerVolume);
    M5.Speaker.tone(2600, 80);
    canvas.fillScreen(Theme::ACCENT2);
    canvas.pushSprite(0, 0);
    delay(100);
    M5.Speaker.end();
    canvas.fillScreen(Theme::BG);
    canvas.pushSprite(0, 0);
    delay(120);

    const char* line1 = "RANGE";
    const char* line2 = "STICK";
    constexpr int kCharW = 18; // Breite je Buchstabe bei textSize(3) (6px Basisbreite * 3)
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
            canvas.fillCircle(x1 + static_cast<int>(i) * kCharW + 8, y1 - 6, 2, Theme::ACCENT2);
        }
        if (i < len2) {
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setCursor(x2 + static_cast<int>(i) * kCharW, y2);
            canvas.print(line2[i]);
            canvas.fillCircle(x2 + static_cast<int>(i) * kCharW + 8, y2 - 6, 2, Theme::ACCENT2);
        }
        canvas.pushSprite(0, 0);
        delay(90);
    }
    delay(150);

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(35, 200);
    canvas.print("shooting buddy");
    canvas.pushSprite(0, 0);
    delay(700);
}

void drawMenu() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setCursor(4, 4);
    canvas.print("RANGESTICK");
    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 16);
    canvas.print("select [A]");

    // Animierter Auswahlbalken, gleitet zur Zielzeile statt zu springen.
    canvas.fillRoundRect(2, static_cast<int>(selAnimY) - 8, canvas.width() - 4, 30, 4, Theme::PANEL);
    canvas.drawRoundRect(2, static_cast<int>(selAnimY) - 8, canvas.width() - 4, 30, 4, Theme::ACCENT);

    canvas.setTextSize(1);
    for (int i = 0; i < kModuleCount; ++i) {
        // Highlight-Band fuer Reihe i liegt bei [rowY(i)-8, rowY(i)+22], Mitte = rowY(i)+7.
        int y = rowY(i) + 3; // 8px hoher Text mittig in diesem Band
        bool sel = (i == menuIndex);
        uint16_t color = sel ? Theme::ACCENT : Theme::TEXT;
        drawIcon(i, 18, rowY(i) + 7, color);
        canvas.setTextColor(color, sel ? Theme::PANEL : Theme::BG);
        // Text zweimal mit 1px Versatz gedruckt -- billiger "Bold"-Effekt, da
        // setTextSize(2) bei 135px Breite die Labels in die naechste Zeile umbricht.
        canvas.setCursor(34, y);
        canvas.print(kModules[i]->name());
        canvas.setCursor(35, y);
        canvas.print(kModules[i]->name());
    }

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 210);
    canvas.print("B=Hoch PWR=Runter");
    canvas.setCursor(4, 224);
    canvas.print("A = waehlen");
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void loopMenu() {
    if (M5.BtnB.wasPressed()) {
        menuIndex = (menuIndex - 1 + kModuleCount) % kModuleCount; // Hoch
        menuAnimating = true;
    } else if (M5.BtnPWR.wasClicked()) {
        menuIndex = (menuIndex + 1) % kModuleCount; // Runter
        menuAnimating = true;
    } else if (M5.BtnA.wasReleased()) {
        activeModule = kModules[menuIndex];
        activeModule->onEnter();
        return;
    }

    uint32_t now = millis();
    if (now - lastMenuDrawMs < 16) return; // ~60fps Deckel
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
    AppSettings::applyCpuFreq();  // Akku-Einstellung: Taktrate vor allem weiteren Setup setzen
    AppSettings::applyDisplay(); // Rotation, Helligkeit, Akzentfarbe aus den gespeicherten Settings
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    splash();
    selAnimY = static_cast<float>(rowY(menuIndex));
    lastActivityMs = millis();
    drawMenu();
}

void loop() {
    // Einziger M5.update()-Aufruf pro Durchlauf: Module rufen es nicht selbst auf,
    // sonst wuerde der zweite Aufruf die gerade erkannte Tastenflanke ueberschreiben.
    M5.update();

    bool buttonActivity = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() ||
                           M5.BtnPWR.wasClicked() || M5.BtnPWR.wasHold();
    bool busy = (activeModule != nullptr) && activeModule->isBusy();
    if (updatePowerState(buttonActivity, busy)) {
        return; // dieser Tastendruck war nur das Aufwecken, nicht weiterverarbeiten
    }

    // Langer Druck auf A: erst fragen, ob das Modul selbst eine Ebene zurueck hat (z.B.
    // Settings' Feld-Liste -> Kategorie-Liste); erst wenn nicht, verlaesst es ins Hauptmenue.
    // Zentral hier erkannt, damit Module selbst nur den kurzen Druck (wasReleased) als
    // "Bestaetigen" sehen. Wird der lange Druck hier ausgeloest, bekommt das Modul/Menue
    // diesen Frame gar nicht mehr zu sehen (return) -- so kann ein langer Druck nie
    // zusaetzlich auch noch die Kurz-Druck-Aktion des Moduls anstossen.
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
        if (wasLong) return; // war ein langer Druck -- keine Kurz-Aktion mehr ausloesen
    }

    if (activeModule == nullptr) {
        loopMenu();
        return;
    }

    activeModule->loop();
}
