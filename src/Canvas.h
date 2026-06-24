#pragma once

#include <M5Unified.h>

// Shared off-screen sprite for all modules: every draw() renders into this and pushes the
// finished image to the display via pushSprite() in one step. Avoids the black flicker that a
// direct fillScreen()+redraw on M5.Display would cause.
extern M5Canvas canvas;

// Draws a large 7-segment number centered (resets font/datum afterwards, otherwise subsequent
// text would stay stuck in the wrong font/datum -- canvas is global). Returns the actually
// rendered height in pixels, so following elements don't have to guess how much space the
// number needs and can position themselves accordingly.
int drawBigNumber(const char* text, int centerX, int y, uint16_t color, int textSize);

// Small battery icon top right, to be called by every screen at the end of its draw().
void drawBatteryIndicator();
