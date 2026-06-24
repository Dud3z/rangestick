#include "WifiConfig.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

namespace WifiConfig {

bool hasCredentials() {
    Preferences prefs;
    prefs.begin("wifi", true);
    bool has = prefs.isKey("ssid");
    prefs.end();
    return has;
}

void load(char* ssidBuf, char* passBuf) {
    Preferences prefs;
    prefs.begin("wifi", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();
    strncpy(ssidBuf, ssid.c_str(), kMaxSsidLen);
    ssidBuf[kMaxSsidLen] = 0;
    strncpy(passBuf, pass.c_str(), kMaxPasswordLen);
    passBuf[kMaxPasswordLen] = 0;
}

void save(const char* ssid, const char* password) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
}

} // namespace WifiConfig
