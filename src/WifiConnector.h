#pragma once

#include <cstdint>
#include "WifiConfig.h"

// Lets the user pick one of the WiFi networks remembered via WifiConfig and connect to it --
// connecting is always this explicit, separate action (Settings > NETWORK > "Connect to WiFi"),
// never automatic just because a network happens to be remembered. Selecting the
// currently-connected network again disconnects it.
class WifiConnector {
public:
    enum class State { LIST, CONNECTING };

    void start();                 // (re)enter the list -- keeps an existing connection, if any
    void loop();                  // must be called regularly while CONNECTING
    void moveSelection(int dir);   // B/PWR while the list is shown
    void confirmSelection();       // A on the list: connect to / disconnect from the selection
    void disconnect();             // explicit WiFi off (e.g. when leaving Settings entirely)

    State state() const { return state_; }
    int networkCount() const;
    int selectedIndex() const { return selectedIndex_; }
    void networkSsid(int index, char* buf) const; // buf must hold WifiConfig::kMaxSsidLen+1 bytes
    bool isConnectedTo(int index) const;
    const char* errorMessage() const { return errorMsg_; }

private:
    static constexpr uint32_t kConnectTimeoutMs = 15000;

    State state_ = State::LIST;
    int selectedIndex_ = 0;
    int connectingIndex_ = -1;
    uint32_t connectStartMs_ = 0;
    char connectedSsid_[WifiConfig::kMaxSsidLen + 1] = {0};
    char errorMsg_[32] = {0};
};
