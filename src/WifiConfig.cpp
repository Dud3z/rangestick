#include "WifiConfig.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

namespace WifiConfig {

namespace {
void keyFor(char* buf, const char* prefix, int index) {
    snprintf(buf, 8, "%s%d", prefix, index);
}
} // namespace

int count() {
    Preferences prefs;
    prefs.begin("wifi", true);
    int n = prefs.getInt("cnt", 0);
    prefs.end();
    if (n < 0) n = 0;
    if (n > kMaxNetworks) n = kMaxNetworks;
    return n;
}

void get(int index, char* ssidBuf, char* passBuf) {
    Preferences prefs;
    prefs.begin("wifi", true);
    char sKey[8], pKey[8];
    keyFor(sKey, "s", index);
    keyFor(pKey, "p", index);
    String ssid = prefs.getString(sKey, "");
    String pass = prefs.getString(pKey, "");
    prefs.end();
    strncpy(ssidBuf, ssid.c_str(), kMaxSsidLen);
    ssidBuf[kMaxSsidLen] = 0;
    strncpy(passBuf, pass.c_str(), kMaxPasswordLen);
    passBuf[kMaxPasswordLen] = 0;
}

void remember(const char* ssid, const char* password) {
    Preferences prefs;
    prefs.begin("wifi", false);
    int n = prefs.getInt("cnt", 0);
    if (n < 0) n = 0;
    if (n > kMaxNetworks) n = kMaxNetworks;

    char sKey[8], pKey[8];
    int existing = -1;
    for (int i = 0; i < n; ++i) {
        keyFor(sKey, "s", i);
        if (prefs.getString(sKey, "") == ssid) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        keyFor(pKey, "p", existing);
        prefs.putString(pKey, password);
    } else if (n < kMaxNetworks) {
        keyFor(sKey, "s", n);
        keyFor(pKey, "p", n);
        prefs.putString(sKey, ssid);
        prefs.putString(pKey, password);
        prefs.putInt("cnt", n + 1);
    } else {
        // Full -- drop the oldest (index 0), shift the rest down, append the new one at the end.
        for (int i = 1; i < kMaxNetworks; ++i) {
            char sFrom[8], pFrom[8], sTo[8], pTo[8];
            keyFor(sFrom, "s", i);
            keyFor(pFrom, "p", i);
            keyFor(sTo, "s", i - 1);
            keyFor(pTo, "p", i - 1);
            prefs.putString(sTo, prefs.getString(sFrom, ""));
            prefs.putString(pTo, prefs.getString(pFrom, ""));
        }
        keyFor(sKey, "s", kMaxNetworks - 1);
        keyFor(pKey, "p", kMaxNetworks - 1);
        prefs.putString(sKey, ssid);
        prefs.putString(pKey, password);
    }
    prefs.end();
}

} // namespace WifiConfig
