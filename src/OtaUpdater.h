#pragma once

#include <cstdint>

// OTA-Update-Check + Download/Flash gegen das in Version.h konfigurierte GitHub-Repo. Liest
// ota/version.txt per HTTPS (raw.githubusercontent.com), vergleicht mit Version::FW_VERSION,
// laedt bei Unterschied ota/firmware.bin direkt in die naechste OTA-Partition (Update.h) und
// startet neu. TLS-Zertifikatspruefung ist bewusst per setInsecure() deaktiviert -- akzeptierter
// Trade-off fuer ein privates Geraet, das nur das eigene Repo abruft (siehe Settings/Plan).
// Download+Flash laufen -- wie main.cpp's splash() -- als blockierende Funktion mit eigenem
// Progress-Draw direkt auf canvas/pushSprite, da die App ohnehin single-threaded/synchron ist.
class OtaUpdater {
public:
    enum class State { IDLE, CONNECTING, CHECKING, UP_TO_DATE, UPDATE_AVAILABLE, DOWNLOADING, DONE, ERROR };

    void start();   // verbindet per WifiConfig-Zugangsdaten, prueft danach auf neue Version
    void loop();    // muss waehrend CONNECTING regelmaessig aufgerufen werden
    void confirm(); // im Zustand UPDATE_AVAILABLE: Download+Flash anstossen
    void stop();    // bricht ab / WLAN wieder aus (z.B. bei Lang-Druck zurueck)

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
