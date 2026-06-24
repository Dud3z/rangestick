#include "CantLevel.h"
#include <M5Unified.h>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Globals.h"
#include "CantStyles.h"
#include <cmath>

void CantLevel::onEnter() {
    displayStyle_ = 0;
    lastDrawMs_ = 0;
}

void CantLevel::onExit() {
}

void CantLevel::loop() {
    // Kleine Pause vor dem Poll, damit die CPU zwischen den (deutlich selteneren) echten
    // IMU-Samples idlen kann, statt im Akku-Modus dauerhaft auf vollem Takt zu busy-spinnen.
    delay(AppSettings::imuPollDelayMs);
    if (M5.Imu.update()) {
        auto d = M5.Imu.getImuData();
        gCantCalc.update(d.accel.x, d.accel.y, d.accel.z, d.gyro.x, d.gyro.y, d.gyro.z);
    }

    if (M5.BtnA.wasReleased()) {
        gCantCalc.confirm();
    }

    if (gCantCalc.state() == CantCalculator::State::READY) {
        if (M5.BtnB.wasPressed()) {
            displayStyle_ = (displayStyle_ - 1 + kCantStyleCount) % kCantStyleCount; // Hoch
        } else if (M5.BtnPWR.wasClicked()) {
            displayStyle_ = (displayStyle_ + 1) % kCantStyleCount; // Runter
        }
    }

    uint32_t now = millis();
    if (now - lastDrawMs_ >= 70) {
        lastDrawMs_ = now;
        draw();
    }
}

void CantLevel::draw() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("ANTI-CANT");

    auto state = gCantCalc.state();

    if (state == CantCalculator::State::LEVEL_REF) {
        canvas.setTextColor(Theme::TEXT, Theme::BG);
        canvas.setTextSize(2);
        canvas.setCursor(8, 60);
        canvas.print("Waffe");
        canvas.setCursor(8, 85);
        canvas.print("waagerecht");
        canvas.setCursor(8, 110);
        canvas.print("halten,");
        canvas.setCursor(8, 135);
        canvas.print("dann A");
        canvas.setTextSize(1);
        canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
        canvas.setCursor(4, 220);
        canvas.print("A = Nullpunkt setzen");
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    if (state == CantCalculator::State::LEVEL_COUNTDOWN) {
        canvas.setTextColor(Theme::TEXT, Theme::BG);
        canvas.setTextSize(2);
        canvas.setCursor(8, 50);
        canvas.print("Jetzt ruhig");
        canvas.setCursor(8, 75);
        canvas.print("halten...");
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", gCantCalc.countdownSecondsLeft());
        drawBigNumber(buf, canvas.width() / 2, 120, Theme::ACCENT2, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    if (state == CantCalculator::State::ROCK_GESTURE) {
        canvas.setTextColor(Theme::ACCENT2, Theme::BG);
        canvas.setTextSize(2);
        canvas.setCursor(8, 60);
        canvas.print("Jetzt");
        canvas.setCursor(8, 85);
        canvas.print("kurz");
        canvas.setCursor(8, 110);
        canvas.print("kippen,");
        canvas.setCursor(8, 135);
        canvas.print("dann A");
        canvas.setTextSize(1);
        canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
        canvas.setCursor(4, 220);
        canvas.print("A = Rohrachse uebernehmen");
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    float angleDeg = gCantCalc.angleDeg();
    uint16_t color;
    float a = fabsf(angleDeg);
    if (a < AppSettings::cantGreenDeg) color = Theme::GOOD;
    else if (a < AppSettings::cantYellowDeg) color = Theme::WARN;
    else color = Theme::BAD;

    // Zahl bewusst klein gehalten (Standardfont statt grosser 7-Segment-Anzeige) -- eine
    // staendig wechselnde grosse Zahl lenkt beim Beobachten der Animation nur ab.
    canvas.setTextColor(color, Theme::BG);
    canvas.setTextSize(2);
    canvas.setCursor(4, 16);
    canvas.printf("%+.1f", static_cast<double>(angleDeg));
    canvas.setTextSize(1);
    canvas.print((char)247); // Grad-Symbol

    int cx = canvas.width() / 2;
    int cy = 130;
    drawCantStyle(displayStyle_, cx, cy, 58, angleDeg, color);

    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 200);
    canvas.printf("Stil %d/%d (B/PWR)", displayStyle_ + 1, kCantStyleCount);
    canvas.setCursor(4, 220);
    canvas.print("A = neu kalibrieren");
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}
