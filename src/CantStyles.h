#pragma once

#include <cstdint>

// Anti-Cant Anzeige-Stile als parametrisierte freie Funktionen (Groesse via size-Parameter),
// damit sowohl das eigenstaendige Anti-Cant-Modul (gross) als auch der Kombi-Modus (kompakt)
// dieselbe Zeichenlogik ohne Duplikation nutzen koennen.
constexpr int kCantStyleCount = 3;

void drawCantStyle(int style, int cx, int cy, int size, float angleDeg, uint16_t color);
