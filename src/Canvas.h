#pragma once

#include <M5Unified.h>

// Gemeinsamer Off-Screen-Sprite fuer alle Module: jedes draw() zeichnet hierhin und
// schiebt das fertige Bild per pushSprite() in einem Schritt aufs Display. Vermeidet
// das Schwarz-Aufblitzen, das ein direktes fillScreen()+Redraw auf M5.Display verursacht.
extern M5Canvas canvas;

// Grosse 7-Segment-Zahl zentriert zeichnen (Schriftart/Datum danach zuruecksetzen, sonst
// bleiben nachfolgende Texte im falschen Font/Datum haengen -- canvas ist global). Gibt die
// tatsaechlich gerenderte Hoehe in Pixeln zurueck, damit nachfolgende Elemente nicht raten
// muessen, wie viel Platz die Zahl braucht, sondern sich danach richten koennen.
int drawBigNumber(const char* text, int centerX, int y, uint16_t color, int textSize);

// Kleines Batterie-Icon oben rechts, von jedem Screen am Ende seines draw() aufzurufen.
void drawBatteryIndicator();
