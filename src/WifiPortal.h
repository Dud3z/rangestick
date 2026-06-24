#pragma once

#include <WebServer.h>
#include <DNSServer.h>

// Captive portal flow for WiFi setup without a keyboard on the device: starts an open SoftAP
// ("RangeStick") that you connect to with your phone, plus a small web page to pick the
// network and enter the password. Driven from Settings (NETWORK > "Set up WiFi") -- start()/
// stop() switch WiFi on/off specifically just for this flow, so the stick otherwise keeps
// running radio-free as usual (battery).
class WifiPortal {
public:
    enum class State { IDLE, RUNNING, SAVED_OK, SAVED_FAIL };

    void start();
    void loop(); // must be called regularly during RUNNING/SAVED_* (DNS+HTTP)
    void stop();

    State state() const { return state_; }
    static const char* apSsid() { return kApSsid; }
    const char* apIp() const { return apIpStr_; }

private:
    static constexpr const char* kApSsid = "RangeStick";
    static constexpr uint32_t kConnectTestTimeoutMs = 8000;
    static constexpr uint32_t kIdleTimeoutMs = 180000; // auto-off after 3 min without a request

    State state_ = State::IDLE;
    char apIpStr_[16] = {0};
    uint32_t lastRequestMs_ = 0;
    WebServer server_{80};
    DNSServer dns_;

    void handleRoot();
    void handleSave();
    void handleNotFound();
};
