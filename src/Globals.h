#pragma once

#include "CantCalculator.h"
#include "StabilityCalculator.h"
#include "WifiConnector.h"

// Shared calculation engines: both the standalone modules (CantLevel, StabilityTracker) and the
// combo mode (ComboView) access the same instances, so e.g. an anti-cant calibration performed
// once applies in both views. Defined in main.cpp.
extern CantCalculator gCantCalc;
extern StabilityCalculator gStabilityCalc;

// A WiFi connection made via WifiConnector is meant to persist across module switches (e.g. you
// connect, then go use the Shot Timer while staying online) -- so it lives globally instead of
// as a Settings-owned member, and its settings-web-server needs servicing from main.cpp's loop()
// regardless of which module is currently active. Defined in main.cpp.
extern WifiConnector gWifiConnector;
