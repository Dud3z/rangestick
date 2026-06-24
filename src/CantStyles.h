#pragma once

#include <cstdint>

// Anti-cant display styles as parameterized free functions (size via the size parameter), so
// both the standalone anti-cant module (large) and the combo mode (compact) can use the same
// drawing logic without duplication.
constexpr int kCantStyleCount = 3;

void drawCantStyle(int style, int cx, int cy, int size, float angleDeg, uint16_t color);
