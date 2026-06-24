#include "ComboView.h"
#include <M5Unified.h>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Globals.h"
#include "CantStyles.h"
#include <cmath>

void ComboView::onEnter() {
    cantStyle_ = 0;
    lastDrawMs_ = 0;
    gStabilityCalc.resetSession(); // cant calibration is deliberately kept, only stability resets
}

void ComboView::onExit() {
}

void ComboView::loop() {
    // Read the sensor exactly once per frame and feed the same reading to both calculations --
    // if each called M5.Imu.update() on its own, they could snatch the "new data" flag away from
    // each other, so stability would only get real samples sporadically and never settle down.
    // Small pause before polling, so the CPU can idle between the (much rarer) real IMU samples
    // instead of busy-spinning at full clock all the time in battery mode.
    delay(AppSettings::imuPollDelayMs);
    if (M5.Imu.update()) {
        auto d = M5.Imu.getImuData();
        gCantCalc.update(d.accel.x, d.accel.y, d.accel.z, d.gyro.x, d.gyro.y, d.gyro.z);
        gStabilityCalc.update(d.accel.x, d.accel.y, d.accel.z);
    }

    if (M5.BtnA.wasReleased()) {
        gCantCalc.confirm();
    }

    if (gCantCalc.state() == CantCalculator::State::READY) {
        if (M5.BtnB.wasPressed()) {
            cantStyle_ = (cantStyle_ - 1 + kCantStyleCount) % kCantStyleCount; // up
        } else if (M5.BtnPWR.wasClicked()) {
            cantStyle_ = (cantStyle_ + 1) % kCantStyleCount; // down
        }
    }

    uint32_t now = millis();
    if (now - lastDrawMs_ >= 70) {
        lastDrawMs_ = now;
        draw();
    }
}

void ComboView::draw() {
    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("COMBO");

    auto state = gCantCalc.state();
    if (state == CantCalculator::State::LEVEL_COUNTDOWN) {
        canvas.setTextColor(Theme::TEXT, Theme::BG);
        canvas.setTextSize(2);
        canvas.setCursor(8, 50);
        canvas.print("Hold");
        canvas.setCursor(8, 75);
        canvas.print("steady...");
        char cdBuf[4];
        snprintf(cdBuf, sizeof(cdBuf), "%d", gCantCalc.countdownSecondsLeft());
        drawBigNumber(cdBuf, canvas.width() / 2, 120, Theme::ACCENT2, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    if (state != CantCalculator::State::READY) {
        bool rock = (state == CantCalculator::State::ROCK_GESTURE);
        canvas.setTextColor(rock ? Theme::ACCENT2 : Theme::TEXT, Theme::BG);
        canvas.setTextSize(2);
        canvas.setCursor(8, 60);
        canvas.print(rock ? "Now" : "Hold");
        canvas.setCursor(8, 85);
        canvas.print(rock ? "tilt" : "weapon");
        canvas.setCursor(8, 110);
        canvas.print(rock ? "briefly," : "level,");
        canvas.setCursor(8, 135);
        canvas.print("then A");
        canvas.setTextSize(1);
        canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
        canvas.setCursor(4, 220);
        canvas.print(rock ? "A = adopt bore axis" : "A = set zero point");
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    // -- Anti-cant: top, prominent area --
    float angleDeg = gCantCalc.angleDeg();
    uint16_t cantColor;
    float a = fabsf(angleDeg);
    if (a < AppSettings::cantGreenDeg) cantColor = Theme::GOOD;
    else if (a < AppSettings::cantYellowDeg) cantColor = Theme::WARN;
    else cantColor = Theme::BAD;

    int cx = canvas.width() / 2;

    // Number + degree symbol as one centered unit, instead of "slapped on" top left.
    char buf[8];
    snprintf(buf, sizeof(buf), "%+.1f", static_cast<double>(angleDeg));
    canvas.setTextSize(2);
    int numW = canvas.textWidth(buf);
    canvas.setTextSize(1);
    char degStr[2] = {static_cast<char>(247), 0};
    int degW = canvas.textWidth(degStr);
    int startX = cx - (numW + degW) / 2;

    canvas.setTextColor(cantColor, Theme::BG);
    canvas.setTextSize(2);
    canvas.setCursor(startX, 16);
    canvas.print(buf);
    canvas.setTextSize(1);
    canvas.setCursor(startX + numW, 16);
    canvas.print(degStr);

    int cantCy = 95;
    drawCantStyle(cantStyle_, cx, cantCy, 55, angleDeg, cantColor);

    canvas.drawFastHLine(4, 158, canvas.width() - 8, Theme::PANEL);

    // -- Stability: bottom, just as a slim indicator --
    float wobbleMoa = gStabilityCalc.wobbleMoa();
    uint16_t stabColor;
    if (wobbleMoa < AppSettings::stabilityGreenMoa) stabColor = Theme::GOOD;
    else if (wobbleMoa < AppSettings::stabilityYellowMoa) stabColor = Theme::WARN;
    else stabColor = Theme::BAD;

    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 166);
    canvas.print("STABILITY");

    canvas.setTextColor(stabColor, Theme::BG);
    canvas.setTextSize(2);
    canvas.setCursor(4, 180);
    canvas.printf("%.1f", static_cast<double>(wobbleMoa));
    canvas.setTextSize(1);
    canvas.print(" MOA");

    int barX = 4, barY = 204, barW = 127, barH = 10;
    float graphMax = AppSettings::stabilityGraphMaxMoa;
    canvas.drawRect(barX, barY, barW, barH, Theme::SUBTEXT);
    int fillW = static_cast<int>(constrain(wobbleMoa / graphMax, 0.0f, 1.0f) * (barW - 4));
    if (fillW > 0) canvas.fillRect(barX + 2, barY + 2, fillW, barH - 4, stabColor);

    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}
