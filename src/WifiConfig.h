#pragma once

#include <cstddef>

// Persistierte WLAN-Zugangsdaten, eigener Preferences-Namespace ("wifi") -- bewusst getrennt von
// AppSettings' "cfg"-Namespace, damit Settings::resetDefaults() (RESET_DEFAULTS) die WLAN-Daten
// nicht versehentlich mitloescht.
namespace WifiConfig {

constexpr size_t kMaxSsidLen = 32;
constexpr size_t kMaxPasswordLen = 64;

bool hasCredentials();
// ssidBuf/passBuf muessen mindestens kMaxSsidLen+1 / kMaxPasswordLen+1 Bytes gross sein.
void load(char* ssidBuf, char* passBuf);
void save(const char* ssid, const char* password);

} // namespace WifiConfig
