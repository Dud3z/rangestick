#pragma once

#include <cstddef>

// Persisted WiFi credentials for several remembered networks, own Preferences namespace
// ("wifi") -- deliberately separate from AppSettings' "cfg" namespace, so
// Settings::resetDefaults() (RESET_DEFAULTS) doesn't accidentally wipe them too.
// Remembering a network (via the captive portal, see WifiPortal) never connects to it by
// itself -- connecting is always the explicit, separate action in WifiConnector.
namespace WifiConfig {

constexpr size_t kMaxSsidLen = 32;
constexpr size_t kMaxPasswordLen = 64;
constexpr int kMaxNetworks = 5;

int count(); // how many networks are remembered (0..kMaxNetworks)
// ssidBuf/passBuf must be at least kMaxSsidLen+1 / kMaxPasswordLen+1 bytes large.
void get(int index, char* ssidBuf, char* passBuf);
// Updates the password if ssid is already known, otherwise appends a new entry -- evicting the
// oldest one first if already at kMaxNetworks.
void remember(const char* ssid, const char* password);

} // namespace WifiConfig
