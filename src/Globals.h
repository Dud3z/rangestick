#pragma once

#include "CantCalculator.h"
#include "StabilityCalculator.h"

// Geteilte Berechnungs-Engines: sowohl die Einzelmodule (CantLevel, StabilityTracker) als auch
// der Kombi-Modus (ComboView) greifen auf dieselben Instanzen zu, damit z.B. eine einmal
// durchgefuehrte Anti-Cant-Kalibrierung in beiden Ansichten gilt. Definiert in main.cpp.
extern CantCalculator gCantCalc;
extern StabilityCalculator gStabilityCalc;
