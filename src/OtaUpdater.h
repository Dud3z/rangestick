#pragma once

#include <cstdint>

// OTA update check + download/flash against the GitHub repo configured in Version.h. Assumes
// WiFi is already connected -- connecting is a separate, explicit user action (see
// WifiConnector); this class never initiates a connection itself. Reads ota/version.txt over
// HTTPS (raw.githubusercontent.com), compares it to Version::FW_VERSION, and on a mismatch
// downloads ota/firmware.bin straight into the next OTA partition (Update.h) and reboots. TLS
// certificate verification is deliberately disabled via setInsecure() -- an accepted trade-off
// for a private device that only ever talks to its own repo. Download+flash run -- like
// main.cpp's splash() -- as a blocking function with its own progress draw directly on
// canvas/pushSprite, since the app is single-threaded/synchronous anyway.
class OtaUpdater {
public:
    enum class State { IDLE, CHECKING, UP_TO_DATE, UPDATE_AVAILABLE, DOWNLOADING, DONE, ERROR };

    void start();   // requires an already-active WiFi connection; checks for a new version
    void loop();    // no-op (kept for symmetry with the other Settings sub-flows)
    void confirm(); // in state UPDATE_AVAILABLE: kick off download+flash
    void stop();    // dismiss -- does not touch the WiFi connection itself

    State state() const { return state_; }
    const char* remoteVersion() const { return remoteVersion_; }
    const char* errorMessage() const { return errorMsg_; }

private:
    State state_ = State::IDLE;
    char remoteVersion_[16] = {0};
    char errorMsg_[48] = {0};

    void checkVersion();
    void runDownload();
    void setError(const char* msg);
    void drawMessage(const char* line1, const char* line2);
};
