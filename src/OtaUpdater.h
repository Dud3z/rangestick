#pragma once

#include <cstdint>

// OTA update check + download/flash against the GitHub repo configured in Version.h. Reads
// ota/version.txt over HTTPS (raw.githubusercontent.com), compares it to Version::FW_VERSION,
// and on a mismatch downloads ota/firmware.bin straight into the next OTA partition (Update.h)
// and reboots. TLS certificate verification is deliberately disabled via setInsecure() --
// an accepted trade-off for a private device that only ever talks to its own repo (see
// Settings/plan). Download+flash run -- like main.cpp's splash() -- as a blocking function with
// its own progress draw directly on canvas/pushSprite, since the app is single-threaded/
// synchronous anyway.
class OtaUpdater {
public:
    enum class State { IDLE, CONNECTING, CHECKING, UP_TO_DATE, UPDATE_AVAILABLE, DOWNLOADING, DONE, ERROR };

    void start();   // connects using WifiConfig credentials, then checks for a new version
    void loop();    // must be called regularly during CONNECTING
    void confirm(); // in state UPDATE_AVAILABLE: kick off download+flash
    void stop();    // cancel / WiFi off again (e.g. on long-press back)

    State state() const { return state_; }
    const char* remoteVersion() const { return remoteVersion_; }
    const char* errorMessage() const { return errorMsg_; }

private:
    static constexpr uint32_t kConnectTimeoutMs = 15000;

    State state_ = State::IDLE;
    uint32_t connectStartMs_ = 0;
    char remoteVersion_[16] = {0};
    char errorMsg_[48] = {0};

    void checkVersion();
    void runDownload();
    void setError(const char* msg);
    void drawMessage(const char* line1, const char* line2);
};
