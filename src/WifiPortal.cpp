#include "WifiPortal.h"
#include <WiFi.h>
#include "WifiConfig.h"
#include "WebUI.h"

void WifiPortal::handleRoot() {
    lastRequestMs_ = millis();
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

    String body = "<h2>Set up RangeStick WiFi</h2><form method='POST' action='/save'>";
    body += "<label>Network</label><select name='ssid'>" + options + "</select>";
    body += "<label>Password</label><input type='password' name='pass' maxlength='63'>";
    body += "<button type='submit'>Save &amp; connect</button></form>";
    server_.send(200, "text/html", WebUI::pageShell("WiFi Setup", "wifi", body));
}

void WifiPortal::handleSave() {
    lastRequestMs_ = millis();
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    if (ssid.length() == 0) {
        server_.send(200, "text/html", WebUI::pageShell("WiFi Setup", "wifi", "<p>No network selected.</p>"));
        return;
    }
    WifiConfig::remember(ssid.c_str(), pass.c_str());

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

    String body = "<h2>";
    body += ok ? "Connected!" : "Connection failed";
    body += "</h2><p>";
    body += ok ? "You can close this window or go to Settings." : "Wrong password? Try again.";
    body += "</p><a class='btn' href='/'>Back to WiFi setup</a>";
    server_.send(200, "text/html", WebUI::pageShell("WiFi Setup", "wifi", body));
}

void WifiPortal::handleNotFound() {
    lastRequestMs_ = millis();
    server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server_.send(302, "text/plain", "");
}

void WifiPortal::start() {
    WiFi.mode(WIFI_AP);
    WiFi.setHostname("RangeStick");
    WiFi.softAP(kApSsid);
    IPAddress ip = WiFi.softAPIP();
    snprintf(apIpStr_, sizeof(apIpStr_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    dns_.start(53, "*", ip);

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    WebUI::registerSettingsRoutes(server_, true);
    server_.onNotFound([this]() { handleNotFound(); });
    server_.begin();

    WiFi.scanNetworks(true);
    lastRequestMs_ = millis();
    state_ = State::RUNNING;
}

void WifiPortal::loop() {
    if (state_ == State::IDLE) return;
    dns_.processNextRequest();
    server_.handleClient();
    if (millis() - lastRequestMs_ > kIdleTimeoutMs) {
        stop(); // nobody used the web interface for a while -- turn WiFi back off
    }
}

void WifiPortal::stop() {
    if (state_ == State::IDLE) return;
    server_.stop();
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    state_ = State::IDLE;
}
