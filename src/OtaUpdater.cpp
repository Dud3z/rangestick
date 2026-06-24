#include "OtaUpdater.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <cstring>
#include "WifiConfig.h"
#include "Version.h"
#include "Canvas.h"
#include "Theme.h"

namespace {
String otaUrl(const char* file) {
    String url = "https://raw.githubusercontent.com/";
    url += Version::OTA_REPO_OWNER;
    url += "/";
    url += Version::OTA_REPO_NAME;
    url += "/";
    url += Version::OTA_REPO_BRANCH;
    url += "/ota/";
    url += file;
    return url;
}
} // namespace

void OtaUpdater::drawMessage(const char* line1, const char* line2) {
    canvas.fillScreen(Theme::BG);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.setTextSize(1);
    canvas.setCursor(4, 2);
    canvas.print("UPDATE");
    canvas.setTextColor(Theme::TEXT, Theme::BG);
    canvas.setTextSize(2);
    canvas.setCursor(8, 70);
    canvas.print(line1);
    if (line2 != nullptr) {
        canvas.setCursor(8, 95);
        canvas.print(line2);
    }
    canvas.pushSprite(0, 0);
}

void OtaUpdater::setError(const char* msg) {
    strncpy(errorMsg_, msg, sizeof(errorMsg_) - 1);
    errorMsg_[sizeof(errorMsg_) - 1] = 0;
    state_ = State::ERROR;
}

void OtaUpdater::checkVersion() {
    state_ = State::CHECKING;
    drawMessage("Pruefe", "Version...");

    WiFiClientSecure client;
    client.setInsecure(); // bewusster Trade-off, siehe Header-Kommentar
    HTTPClient http;
    if (!http.begin(client, otaUrl("version.txt"))) {
        setError("Verbindung zu GitHub");
        return;
    }
    int code = http.GET();
    if (code != 200) {
        char buf[48];
        snprintf(buf, sizeof(buf), "HTTP-Fehler %d", code);
        http.end();
        setError(buf);
        return;
    }
    String body = http.getString();
    http.end();
    body.trim();

    if (body.length() == 0) {
        setError("Leere Version erhalten");
        return;
    }
    strncpy(remoteVersion_, body.c_str(), sizeof(remoteVersion_) - 1);
    remoteVersion_[sizeof(remoteVersion_) - 1] = 0;
    state_ = (body == Version::FW_VERSION) ? State::UP_TO_DATE : State::UPDATE_AVAILABLE;
}

void OtaUpdater::runDownload() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, otaUrl("firmware.bin"))) {
        setError("Download fehlgeschlagen");
        return;
    }
    int code = http.GET();
    int total = http.getSize();
    if (code != 200 || total <= 0) {
        char buf[48];
        snprintf(buf, sizeof(buf), "HTTP-Fehler %d", code);
        http.end();
        setError(buf);
        return;
    }

    if (!Update.begin(static_cast<size_t>(total))) {
        http.end();
        setError("Zu wenig Platz");
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    int written = 0;
    int lastShownPct = -1;
    while (http.connected() && written < total) {
        size_t avail = stream->available();
        if (avail == 0) {
            delay(2);
            continue;
        }
        size_t chunk = (avail < sizeof(buf)) ? avail : sizeof(buf);
        size_t n = stream->readBytes(buf, chunk);
        if (n == 0) break;
        Update.write(buf, n);
        written += static_cast<int>(n);

        int pct = (written * 100) / total;
        if (pct != lastShownPct) {
            lastShownPct = pct;
            char line[16];
            snprintf(line, sizeof(line), "%d%%", pct);
            drawMessage("Lade Update...", line);
        }
    }
    http.end();

    if (written != total || !Update.end() || !Update.isFinished()) {
        Update.abort();
        setError("Update fehlgeschlagen");
        return;
    }

    state_ = State::DONE;
    drawMessage("Fertig!", "Neustart...");
    delay(1200);
    ESP.restart();
}

void OtaUpdater::start() {
    errorMsg_[0] = 0;
    remoteVersion_[0] = 0;

    if (!WifiConfig::hasCredentials()) {
        setError("Keine WLAN-Daten");
        return;
    }
    char ssid[WifiConfig::kMaxSsidLen + 1];
    char pass[WifiConfig::kMaxPasswordLen + 1];
    WifiConfig::load(ssid, pass);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    connectStartMs_ = millis();
    state_ = State::CONNECTING;
    drawMessage("Verbinde...", nullptr);
}

void OtaUpdater::loop() {
    if (state_ != State::CONNECTING) return;
    if (WiFi.status() == WL_CONNECTED) {
        checkVersion();
    } else if (millis() - connectStartMs_ > kConnectTimeoutMs) {
        setError("WLAN fehlgeschlagen");
    }
}

void OtaUpdater::confirm() {
    if (state_ != State::UPDATE_AVAILABLE) return;
    state_ = State::DOWNLOADING;
    runDownload();
}

void OtaUpdater::stop() {
    if (state_ == State::IDLE) return;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state_ = State::IDLE;
}
