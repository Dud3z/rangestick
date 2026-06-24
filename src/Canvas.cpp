#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"

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
    // PMIC-Abfrage (I2C) gedrosselt statt bei jedem Redraw -- siehe AppSettings::battReadIntervalMs.
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
}
