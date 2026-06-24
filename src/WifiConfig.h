#pragma once

#include <cstddef>

// Persisted WiFi credentials, own Preferences namespace ("wifi") -- deliberately separate from
// AppSettings' "cfg" namespace, so Settings::resetDefaults() (RESET_DEFAULTS) doesn't
// accidentally wipe the WiFi credentials too.
namespace WifiConfig {

constexpr size_t kMaxSsidLen = 32;
constexpr size_t kMaxPasswordLen = 64;

bool hasCredentials();
// ssidBuf/passBuf must be at least kMaxSsidLen+1 / kMaxPasswordLen+1 bytes large.
void load(char* ssidBuf, char* passBuf);
void save(const char* ssid, const char* password);

} // namespace WifiConfig
