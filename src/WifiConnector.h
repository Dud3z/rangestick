#pragma once

#include <cstdint>
#include <WebServer.h>
#include "WifiConfig.h"

// Lets the user pick one of the WiFi networks remembered via WifiConfig and connect to it --
// connecting is always this explicit, separate action (Settings > NETWORK > "Connect to WiFi"),
// never automatic just because a network happens to be remembered. Selecting the
// currently-connected network again disconnects it.
//
// Unlike WifiPortal (whose AP+server only run while its own setup screen is open), a connection
// made here is meant to persist while the user goes off and uses other modules -- so the
// settings web server started on a successful connection needs servicing independently of
// whatever screen is currently active. That's why this class lives as a global instance (see
// Globals.h) and gets a lightweight serviceServer() tick from main.cpp's loop(), separate from
// the button-handling loop() that only runs while its own list screen is shown.
class WifiConnector {
public:
    enum class State { LIST, CONNECTING };

    void start();                 // (re)enter the list -- keeps an existing connection, if any
    void loop();                  // button handling; must be called while the list screen is shown
    void serviceServer();         // call unconditionally, every frame, regardless of active module
    void moveSelection(int dir);   // B/PWR while the list is shown
    void confirmSelection();       // A on the list: connect to / disconnect from the selection
    void disconnect();             // explicit WiFi + web server off (e.g. when leaving Settings)

    State state() const { return state_; }
    int networkCount() const;
    int selectedIndex() const { return selectedIndex_; }
    void networkSsid(int index, char* buf) const; // buf must hold WifiConfig::kMaxSsidLen+1 bytes
    bool isConnectedTo(int index) const;
    const char* errorMessage() const { return errorMsg_; }

private:
    static constexpr uint32_t kConnectTimeoutMs = 15000;
    static constexpr const char* kHostname = "RangeStick";

    State state_ = State::LIST;
    int selectedIndex_ = 0;
    int connectingIndex_ = -1;
    uint32_t connectStartMs_ = 0;
    char connectedSsid_[WifiConfig::kMaxSsidLen + 1] = {0};
    char errorMsg_[32] = {0};

    bool serverRunning_ = false;
    WebServer settingsServer_{80};

    void startSettingsServer();
    void stopSettingsServer();
};
