#include "StabilityTracker.h"
#include <M5Unified.h>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include "Globals.h"
#include <cmath>

void StabilityTracker::onEnter() {
    displayStyle_ = 0;
    lastDrawMs_ = 0;
    gStabilityCalc.resetSession();
}

void StabilityTracker::onExit() {
}

void StabilityTracker::loop() {
    if (M5.BtnA.wasReleased()) {
        gStabilityCalc.resetSession();
    } else if (M5.BtnB.wasPressed()) {
        displayStyle_ = (displayStyle_ - 1 + STYLE_COUNT) % STYLE_COUNT; // Hoch
    } else if (M5.BtnPWR.wasClicked()) {
        displayStyle_ = (displayStyle_ + 1) % STYLE_COUNT; // Runter
    }

    // Kleine Pause vor dem Poll, damit die CPU zwischen den (deutlich selteneren) echten
    // IMU-Samples idlen kann, statt im Akku-Modus dauerhaft auf vollem Takt zu busy-spinnen.
    delay(AppSettings::imuPollDelayMs);
    if (M5.Imu.update()) {
        auto d = M5.Imu.getImuData();
        gStabilityCalc.update(d.accel.x, d.accel.y, d.accel.z);
    }

    uint32_t now = millis();
    if (now - lastDrawMs_ >= 70) {
        lastDrawMs_ = now;
        draw();
    }
}

void StabilityTracker::drawSparklineStyle(int x, int y, int w, int h, uint16_t color) {
    float graphMaxMoa = AppSettings::stabilityGraphMaxMoa;
    canvas.drawRect(x, y, w, h, Theme::SUBTEXT);
    int historyCount = gStabilityCalc.historyCount();
    if (historyCount > 1) {
        const float* history = gStabilityCalc.history();
        int historyIndex = gStabilityCalc.historyIndex();
        int oldest = (historyIndex - historyCount + StabilityCalculator::HISTORY) % StabilityCalculator::HISTORY;
        int prevX = 0, prevY = 0;
        for (int i = 0; i < historyCount; ++i) {
            int idx = (oldest + i) % StabilityCalculator::HISTORY;
            float v = constrain(history[idx], 0.0f, graphMaxMoa);
            int px = x + (w * i) / (StabilityCalculator::HISTORY - 1);
            int py = y + h - static_cast<int>((v / graphMaxMoa) * (h - 2)) - 1;
            if (i > 0) canvas.drawLine(prevX, prevY, px, py, Theme::ACCENT);
            prevX = px;
            prevY = py;
        }
        canvas.fillCircle(prevX, prevY, 2, color);
    }
}

void StabilityTracker::drawScatterStyle(int cx, int cy, uint16_t color) {
    int r = 48;
    canvas.drawCircle(cx, cy, r, Theme::SUBTEXT);
    canvas.drawCircle(cx, cy, r * 2 / 3, Theme::SUBTEXT);
    canvas.drawCircle(cx, cy, r / 3, Theme::SUBTEXT);
    canvas.drawFastHLine(cx - r, cy, r * 2, Theme::SUBTEXT);
    canvas.drawFastVLine(cx, cy - r, r * 2, Theme::SUBTEXT);

    constexpr float kScaleMoaPerPixel = 0.18f; // wie viel MOA ein Pixel Auslenkung darstellt
    int scatterCount = gStabilityCalc.scatterCount();
    const float* sx = gStabilityCalc.scatterX();
    const float* sy = gStabilityCalc.scatterY();
    for (int i = 0; i < scatterCount; ++i) {
        int px = cx + static_cast<int>(sx[i] / kScaleMoaPerPixel);
        int py = cy + static_cast<int>(sy[i] / kScaleMoaPerPixel);
        px = constrain(px, cx - r, cx + r);
        py = constrain(py, cy - r, cy + r);
        canvas.fillCircle(px, py, 2, Theme::SUBTEXT);
    }
    if (scatterCount > 0) {
        int last = (gStabilityCalc.scatterIndex() - 1 + StabilityCalculator::SCATTER_COUNT) % StabilityCalculator::SCATTER_COUNT;
        int px = constrain(cx + static_cast<int>(sx[last] / kScaleMoaPerPixel), cx - r, cx + r);
        int py = constrain(cy + static_cast<int>(sy[last] / kScaleMoaPerPixel), cy - r, cy + r);
        canvas.fillCircle(px, py, 4, color);
    }
}

void StabilityTracker::drawPulseStyle(int cx, int cy, uint16_t color) {
    float graphMaxMoa = AppSettings::stabilityGraphMaxMoa;
    float frac = constrain(gStabilityCalc.wobbleMoa() / graphMaxMoa, 0.0f, 1.0f);
    int maxR = 46;
    int baseR = 12 + static_cast<int>(frac * (maxR - 12));

    // Leichtes "Atmen" zusaetzlich zur Wobble-Groesse, damit der Kreis auch bei sehr ruhiger
    // Lage sichtbar lebendig bleibt statt komplett statisch zu stehen.
    float pulse = sinf(millis() / 250.0f) * 3.0f;
    int r = baseR + static_cast<int>(pulse);
    if (r < 6) r = 6;

    for (int ring = 0; ring < 3; ++ring) {
        int rr = r - ring * 8;
        if (rr <= 0) break;
        canvas.drawCircle(cx, cy, rr, color);
    }
    canvas.fillCircle(cx, cy, 4, Theme::TEXT);
}

void StabilityTracker::draw() {
    float wobbleMoa = gStabilityCalc.wobbleMoa();
    uint16_t color;
    if (wobbleMoa < AppSettings::stabilityGreenMoa) color = Theme::GOOD;
    else if (wobbleMoa < AppSettings::stabilityYellowMoa) color = Theme::WARN;
    else color = Theme::BAD;

    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("STABILITY");

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(wobbleMoa));
    int numY = 14;
    int numH = drawBigNumber(buf, canvas.width() / 2, numY, color, 1);
    int textY = numY + numH + 6;

    canvas.setTextColor(Theme::TEXT, Theme::BG);
    canvas.setCursor(4, textY);
    canvas.print("MOA wobble");
    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, textY + 12);
    canvas.print("(lower = steadier)");

    int areaTop = textY + 24; // unter den zwei Label-Zeilen
    constexpr int kAreaHeight = 96;
    switch (displayStyle_) {
        case 0: drawSparklineStyle(4, areaTop, 127, kAreaHeight, color); break;
        case 1: drawScatterStyle(canvas.width() / 2, areaTop + kAreaHeight / 2, color); break;
        case 2: drawPulseStyle(canvas.width() / 2, areaTop + kAreaHeight / 2, color); break;
    }

    canvas.setTextColor(Theme::TEXT, Theme::BG);
    canvas.setCursor(4, areaTop + kAreaHeight + 8);
    canvas.printf("Peak: %.1f MOA", static_cast<double>(gStabilityCalc.peakMoa()));
    canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
    canvas.setCursor(4, 220);
    canvas.printf("A=reset  Stil%d: B/PWR", displayStyle_ + 1);
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}
