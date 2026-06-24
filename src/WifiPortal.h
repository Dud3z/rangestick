#pragma once

#include <WebServer.h>
#include <DNSServer.h>

// Captive-Portal-Flow zur WLAN-Einrichtung ohne Tastatur am Geraet: startet einen offenen
// SoftAP ("RangeStick-Setup"), zu dem man sich per Handy verbindet, und eine kleine Webseite
// zur Auswahl des Netzwerks + Passworteingabe. Wird aus Settings (SYSTEM > "WLAN einrichten")
// gesteuert -- start()/stop() schalten WLAN gezielt nur fuer diesen Flow ein/aus, damit der
// Stick ausserhalb davon wie gewohnt ohne Funk laeuft (Akku).
class WifiPortal {
public:
    enum class State { IDLE, RUNNING, SAVED_OK, SAVED_FAIL };

    void start();
    void loop(); // muss waehrend RUNNING/SAVED_* regelmaessig aufgerufen werden (DNS+HTTP)
    void stop();

    State state() const { return state_; }
    static const char* apSsid() { return kApSsid; }
    const char* apIp() const { return apIpStr_; }

private:
    static constexpr const char* kApSsid = "RangeStick-Setup";
    static constexpr uint32_t kConnectTestTimeoutMs = 8000;

    State state_ = State::IDLE;
    char apIpStr_[16] = {0};
    WebServer server_{80};
    DNSServer dns_;

    void handleRoot();
    void handleSave();
    void handleNotFound();
};
