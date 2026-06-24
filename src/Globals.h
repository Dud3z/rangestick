#pragma once

#include "CantCalculator.h"
#include "StabilityCalculator.h"

// Shared calculation engines: both the standalone modules (CantLevel, StabilityTracker) and the
// combo mode (ComboView) access the same instances, so e.g. an anti-cant calibration performed
// once applies in both views. Defined in main.cpp.
extern CantCalculator gCantCalc;
extern StabilityCalculator gStabilityCalc;
