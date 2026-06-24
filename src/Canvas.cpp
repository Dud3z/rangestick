#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"
#include <WiFi.h>

int drawBigNumber(const char* text, int centerX, int y, uint16_t color, int textSize) {
    canvas.setTextColor(color, Theme::BG);
    canvas.setFont(&fonts::Font7);
    canvas.setTextSize(textSize);
    int h = canvas.fontHeight();
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(text, centerX, y);
    canvas.setTextDatum(TL_DATUM);
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    return h;
}

void drawBatteryIndicator() {
    // PMIC query (I2C) throttled instead of on every redraw -- see AppSettings::battReadIntervalMs.
    static int cachedLevel = 0;
    static bool cachedCharging = false;
    static bool haveReading = false;
    static uint32_t lastReadMs = 0;

    uint32_t now = millis();
    if (!haveReading || AppSettings::battReadIntervalMs == 0 ||
        now - lastReadMs >= AppSettings::battReadIntervalMs) {
        cachedLevel = M5.Power.getBatteryLevel();
        cachedCharging = M5.Power.isCharging();
        haveReading = true;
        lastReadMs = now;
    }

    int level = cachedLevel;
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    bool charging = cachedCharging;

    int w = 18, h = 9;
    int x = canvas.width() - w - 6;
    int y = 3;

    uint16_t fillColor = Theme::GOOD;
    if (charging) fillColor = Theme::ACCENT;
    else if (level < 20) fillColor = Theme::BAD;
    else if (level < 50) fillColor = Theme::WARN;

    canvas.drawRect(x, y, w, h, Theme::SUBTEXT);
    canvas.fillRect(x + w, y + 2, 2, h - 4, Theme::SUBTEXT);
    int fillW = (w - 4) * level / 100;
    if (fillW > 0) canvas.fillRect(x + 2, y + 2, fillW, h - 4, fillColor);

    // "Connected" indicator left of the battery icon -- a real WiFi glyph (dot + nested arcs),
    // built from full circles clipped to their upper half. That sidesteps drawArc()'s angle
    // convention, which can't be visually verified without the device in front of you -- a
    // previous concentric-full-circles version ended up looking like crosshair rings instead.
    // Shown in a fixed blue whenever a phone is actively connected to the setup AP (regardless
    // of the chosen accent color), or in the normal accent color while merely connected as a
    // WiFi client (e.g. during an update check).
    bool apClientConnected = WiFi.softAPgetStationNum() > 0;
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    if (apClientConnected || staConnected) {
        constexpr uint16_t kWifiBlue = rgb565(40, 150, 255);
        uint16_t wifiColor = apClientConnected ? kWifiBlue : Theme::ACCENT;

        int wx = x - 14;
        int wy = y + h - 1;
        constexpr int kMaxR = 11;
        canvas.setClipRect(wx - kMaxR, wy - kMaxR, kMaxR * 2, kMaxR);
        canvas.drawCircle(wx, wy, 3, wifiColor);
        canvas.drawCircle(wx, wy, 7, wifiColor);
        canvas.drawCircle(wx, wy, kMaxR, wifiColor);
        canvas.clearClipRect();
        canvas.fillCircle(wx, wy, 1, wifiColor);
    }
}
