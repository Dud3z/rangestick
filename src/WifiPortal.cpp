#include "WifiPortal.h"
#include <WiFi.h>
#include "WifiConfig.h"

void WifiPortal::handleRoot() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);
        n = WIFI_SCAN_RUNNING;
    }
    if (n == WIFI_SCAN_RUNNING) {
        server_.send(200, "text/html",
            "<meta http-equiv='refresh' content='2'>"
            "<body style='background:#111;color:#eee;font-family:sans-serif;padding:16px'>"
            "Scanning for networks...</body>");
        return;
    }

    String options;
    for (int i = 0; i < n; ++i) {
        options += "<option>" + WiFi.SSID(i) + "</option>";
    }

    String html;
    html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>RangeStick WiFi</title><style>body{font-family:sans-serif;background:#111;color:#eee;padding:16px}";
    html += "select,input{width:100%;padding:8px;margin:6px 0;box-sizing:border-box}";
    html += "button{width:100%;padding:10px;background:#0c8;color:#000;border:none;font-weight:bold}</style></head><body>";
    html += "<h2>Set up RangeStick WiFi</h2><form method='POST' action='/save'>";
    html += "<label>Network</label><select name='ssid'>" + options + "</select>";
    html += "<label>Password</label><input type='password' name='pass' maxlength='63'>";
    html += "<button type='submit'>Save &amp; connect</button></form></body></html>";
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSave() {
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    if (ssid.length() == 0) {
        server_.send(200, "text/html",
            "<body style='background:#111;color:#eee;font-family:sans-serif;padding:16px'>"
            "No network selected.</body>");
        return;
    }
    WifiConfig::save(ssid.c_str(), pass.c_str());

    // Test the connection while the setup AP is still running -- the ESP32 can run AP+STA at
    // the same time.
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < kConnectTestTimeoutMs) {
        delay(100);
    }
    bool ok = (WiFi.status() == WL_CONNECTED);
    WiFi.disconnect(); // just a test connection, the AP stays up for a possible retry
    state_ = ok ? State::SAVED_OK : State::SAVED_FAIL;

    String html = "<body style='background:#111;color:#eee;font-family:sans-serif;padding:16px'>";
    html += ok ? "Connected! You can close this window."
               : "Connection failed -- wrong password?";
    html += "</body>";
    server_.send(200, "text/html", html);
}

void WifiPortal::handleNotFound() {
    server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server_.send(302, "text/plain", "");
}

void WifiPortal::start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);
    IPAddress ip = WiFi.softAPIP();
    snprintf(apIpStr_, sizeof(apIpStr_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    dns_.start(53, "*", ip);

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    server_.onNotFound([this]() { handleNotFound(); });
    server_.begin();

    WiFi.scanNetworks(true);
    state_ = State::RUNNING;
}

void WifiPortal::loop() {
    if (state_ == State::IDLE) return;
    dns_.processNextRequest();
    server_.handleClient();
}

void WifiPortal::stop() {
    if (state_ == State::IDLE) return;
    server_.stop();
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    state_ = State::IDLE;
}
