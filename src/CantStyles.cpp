#include "CantStyles.h"
#include "Canvas.h"
#include "Theme.h"
#include <cmath>

namespace {

void drawHorizon(int cx, int cy, int size, float angleDeg, uint16_t color) {
    int len = size;
    canvas.drawFastHLine(cx - len, cy, len * 2, Theme::SUBTEXT);
    for (int t = -2; t <= 2; ++t) {
        int tx = cx + t * (len / 2);
        canvas.drawFastVLine(tx, cy - 3, 7, Theme::SUBTEXT);
    }
    float rad = angleDeg * static_cast<float>(M_PI) / 180.0f;
    int dx = static_cast<int>(len * cosf(rad));
    int dy = static_cast<int>(len * sinf(rad));
    canvas.drawLine(cx - dx, cy - dy, cx + dx, cy + dy, color);
    canvas.fillCircle(cx, cy, 4, color);
    canvas.fillCircle(cx, cy, 2, Theme::BG);
}

void drawBubble(int cx, int cy, int size, float angleDeg, uint16_t color) {
    int tubeW = size * 2, tubeH = size / 2;
    if (tubeH < 18) tubeH = 18;
    int tx = cx - tubeW / 2, ty = cy - tubeH / 2;
    canvas.drawRoundRect(tx, ty, tubeW, tubeH, tubeH / 2, Theme::SUBTEXT);
    canvas.drawFastVLine(cx, cy - tubeH / 2 - 6, tubeH + 12, Theme::SUBTEXT);

    float clamped = angleDeg;
    if (clamped > 15.0f) clamped = 15.0f;
    if (clamped < -15.0f) clamped = -15.0f;
    int bubbleR = tubeH / 2 - 3;
    if (bubbleR < 6) bubbleR = 6;
    int bubbleX = cx + static_cast<int>((clamped / 15.0f) * (tubeW / 2 - bubbleR - 2));
    canvas.fillCircle(bubbleX, cy, bubbleR, color);
    canvas.drawCircle(bubbleX, cy, bubbleR, Theme::TEXT);
}

void drawDial(int cx, int cy, int size, float angleDeg, uint16_t color) {
    int r = size;
    canvas.drawCircle(cx, cy, r, Theme::SUBTEXT);
    for (int t = -20; t <= 20; t += 10) {
        float rad = (t - 90) * static_cast<float>(M_PI) / 180.0f;
        int x0 = cx + static_cast<int>((r - 8) * cosf(rad));
        int y0 = cy + static_cast<int>((r - 8) * sinf(rad));
        int x1 = cx + static_cast<int>(r * cosf(rad));
        int y1 = cy + static_cast<int>(r * sinf(rad));
        canvas.drawLine(x0, y0, x1, y1, Theme::SUBTEXT);
    }
    float rad = (angleDeg - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    int nx = cx + static_cast<int>((r - 12) * cosf(rad));
    int ny = cy + static_cast<int>((r - 12) * sinf(rad));
    canvas.drawLine(cx, cy, nx, ny, color);
    canvas.fillCircle(cx, cy, 6, color);
    canvas.fillCircle(cx, cy, 3, Theme::BG);
}

} // namespace

void drawCantStyle(int style, int cx, int cy, int size, float angleDeg, uint16_t color) {
    switch (style) {
        case 0: drawHorizon(cx, cy, size, angleDeg, color); break;
        case 1: drawBubble(cx, cy, size, angleDeg, color); break;
        case 2: drawDial(cx, cy, size, angleDeg, color); break;
    }
}
