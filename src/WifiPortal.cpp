#include "WifiPortal.h"
#include <WiFi.h>
#include <cmath>
#include "WifiConfig.h"
#include "AppSettings.h"

namespace {

constexpr const char* kCss =
    "body{font-family:sans-serif;background:#111;color:#eee;padding:16px;max-width:480px;margin:0 auto}"
    "select,input{width:100%;padding:8px;margin:4px 0 14px;box-sizing:border-box;"
    "background:#222;color:#eee;border:1px solid #444;border-radius:4px;font-size:16px}"
    "label{display:block;margin-top:10px;font-size:14px;color:#aaa}"
    "button,.btn{display:block;width:100%;padding:12px;background:#0c8;color:#000;border:none;"
    "font-weight:bold;margin-top:10px;text-align:center;text-decoration:none;border-radius:4px;"
    "box-sizing:border-box;font-size:16px}"
    ".btn2{background:#333;color:#eee}"
    "a.back{color:#888;font-size:14px;text-decoration:none;display:inline-block;margin-top:14px}"
    "h2{margin-top:0}";

String pageHeader(const char* title) {
    String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>";
    s += title;
    s += "</title><style>";
    s += kCss;
    s += "</style></head><body>";
    return s;
}

String pageFooter() {
    return "</body></html>";
}

String opt(const char* value, const char* label, bool selected) {
    String s = "<option value='";
    s += value;
    s += "'";
    if (selected) s += " selected";
    s += ">";
    s += label;
    s += "</option>";
    return s;
}

// values/labels are parallel arrays of string literals -- values are submitted verbatim and
// parsed back with toInt()/toFloat(), labels are what the user sees.
String selectField(const char* name, const char* labelText, const char* const* values,
                    const char* const* labels, int n, int selectedIdx) {
    String s = "<label>";
    s += labelText;
    s += "</label><select name='";
    s += name;
    s += "'>";
    for (int i = 0; i < n; ++i) {
        s += opt(values[i], labels[i], i == selectedIdx);
    }
    s += "</select>";
    return s;
}

String numberField(const char* name, const char* labelText, long current, long lo, long hi, long step) {
    String s = "<label>";
    s += labelText;
    s += "</label><input type='number' name='";
    s += name;
    s += "' value='";
    s += String(current);
    s += "' min='";
    s += String(lo);
    s += "' max='";
    s += String(hi);
    s += "' step='";
    s += String(step);
    s += "'>";
    return s;
}

int idxU32(const uint32_t* a, int n, uint32_t v) {
    for (int i = 0; i < n; ++i) if (a[i] == v) return i;
    return 0;
}

int idxFloat(const float* a, int n, float v) {
    for (int i = 0; i < n; ++i) if (fabsf(a[i] - v) < 0.001f) return i;
    return 0;
}

String boolSelect(const char* name, const char* labelText, bool current,
                   const char* falseLabel, const char* trueLabel) {
    const char* values[] = {"0", "1"};
    const char* labels[] = {falseLabel, trueLabel};
    return selectField(name, labelText, values, labels, 2, current ? 1 : 0);
}

bool formBool(WebServer& server, const char* name) {
    return server.arg(name) == "1";
}

} // namespace

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

    String html = pageHeader("RangeStick WiFi");
    html += "<h2>Set up RangeStick WiFi</h2><form method='POST' action='/save'>";
    html += "<label>Network</label><select name='ssid'>" + options + "</select>";
    html += "<label>Password</label><input type='password' name='pass' maxlength='63'>";
    html += "<button type='submit'>Save &amp; connect</button></form>";
    html += "<a class='btn btn2' href='/settings'>Edit device settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSave() {
    lastRequestMs_ = millis();
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    if (ssid.length() == 0) {
        server_.send(200, "text/html",
            "<body style='background:#111;color:#eee;font-family:sans-serif;padding:16px'>"
            "No network selected.</body>");
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

    String html = "<body style='background:#111;color:#eee;font-family:sans-serif;padding:16px'>";
    html += ok ? "Connected! You can close this window."
               : "Connection failed -- wrong password?";
    html += "</body>";
    server_.send(200, "text/html", html);
}

void WifiPortal::handleNotFound() {
    lastRequestMs_ = millis();
    server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server_.send(302, "text/plain", "");
}

void WifiPortal::handleSettingsHome() {
    lastRequestMs_ = millis();
    String html = pageHeader("RangeStick Settings");
    html += "<h2>Settings</h2>";
    html += "<a class='btn' href='/settings/display'>Display</a>";
    html += "<a class='btn' href='/settings/shottimer'>Shot Timer</a>";
    html += "<a class='btn' href='/settings/anticant'>Anti-Cant</a>";
    html += "<a class='btn' href='/settings/stability'>Stability</a>";
    html += "<a class='btn' href='/settings/system'>System</a>";
    html += "<a class='back' href='/'>&laquo; Back to WiFi setup</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsDisplay() {
    lastRequestMs_ = millis();
    if (server_.method() == HTTP_POST) {
        AppSettings::brightnessActive = static_cast<uint8_t>(server_.arg("bract").toInt());
        AppSettings::brightnessDim = static_cast<uint8_t>(server_.arg("brdim").toInt());
        // "never" is a text sentinel, not a literal number -- kTimeoutNever (4294967295) would
        // overflow the signed 32-bit long that String::toInt() parses into.
        String dimto = server_.arg("dimto");
        AppSettings::dimTimeoutMs = (dimto == "never") ? AppSettings::kTimeoutNever : static_cast<uint32_t>(dimto.toInt());
        String sleepto = server_.arg("sleepto");
        AppSettings::sleepTimeoutMs = (sleepto == "never") ? AppSettings::kTimeoutNever : static_cast<uint32_t>(sleepto.toInt());
        AppSettings::screenFlipped = formBool(server_, "flip");
        AppSettings::accentColorIndex = server_.arg("accent").toInt();
        AppSettings::applyDisplay();
        AppSettings::save();
    }

    const char* bractV[] = {"60", "100", "140", "180", "220", "255"};
    const uint32_t bractN[] = {60, 100, 140, 180, 220, 255};
    const char* brdimV[] = {"0", "10", "20", "40", "60", "80"};
    const uint32_t brdimN[] = {0, 10, 20, 40, 60, 80};
    const char* dimtoV[] = {"10000", "20000", "30000", "60000", "never"};
    const char* dimtoL[] = {"10s", "20s", "30s", "60s", "Off"};
    const uint32_t dimtoN[] = {10000, 20000, 30000, 60000, AppSettings::kTimeoutNever};
    const char* sleeptoV[] = {"60000", "120000", "180000", "300000", "600000", "never"};
    const char* sleeptoL[] = {"1 min", "2 min", "3 min", "5 min", "10 min", "Off"};
    const uint32_t sleeptoN[] = {60000, 120000, 180000, 300000, 600000, AppSettings::kTimeoutNever};
    const char* accentV[] = {"0", "1", "2", "3", "4", "5", "6", "7"};
    const char* accentL[] = {"Cyan", "Orange", "Green", "Purple", "Red", "Blue", "Yellow", "Pink"};

    String html = pageHeader("Display Settings");
    html += "<h2>Display</h2><form method='POST'>";
    html += selectField("bract", "Active brightness", bractV, bractV, 6, idxU32(bractN, 6, AppSettings::brightnessActive));
    html += selectField("brdim", "Dimmed brightness", brdimV, brdimV, 6, idxU32(brdimN, 6, AppSettings::brightnessDim));
    html += selectField("dimto", "Dim after", dimtoV, dimtoL, 5, idxU32(dimtoN, 5, AppSettings::dimTimeoutMs));
    html += selectField("sleepto", "Sleep after", sleeptoV, sleeptoL, 6, idxU32(sleeptoN, 6, AppSettings::sleepTimeoutMs));
    html += boolSelect("flip", "Screen", AppSettings::screenFlipped, "Normal", "180 deg");
    html += selectField("accent", "Accent color", accentV, accentL, 8, AppSettings::accentColorIndex);
    html += "<button type='submit'>Save</button></form>";
    html += "<a class='back' href='/settings'>&laquo; Back to settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsShotTimer() {
    lastRequestMs_ = millis();
    if (server_.method() == HTTP_POST) {
        AppSettings::shotDetectMode = formBool(server_, "detmode") ? 1 : 0;
        int thr = server_.arg("thr").toInt();
        if (thr < 2000) thr = 2000;
        if (thr > 30000) thr = 30000;
        AppSettings::shotThreshold = static_cast<int16_t>(thr);
        AppSettings::shotDelayMinS = server_.arg("dlymin").toFloat();
        AppSettings::shotDelayMaxS = server_.arg("dlymax").toFloat();
        if (AppSettings::shotDelayMaxS < AppSettings::shotDelayMinS) AppSettings::shotDelayMaxS = AppSettings::shotDelayMinS;
        AppSettings::buzzerVolume = static_cast<uint8_t>(server_.arg("vol").toInt());
        AppSettings::shotEchoLockoutMs = static_cast<uint32_t>(server_.arg("echolo").toInt());
        AppSettings::shotEchoRatio = server_.arg("echort").toFloat();
        AppSettings::shotSpectralEnabled = formBool(server_, "specen");
        AppSettings::shotSpectralRatio = server_.arg("specrt").toFloat();
        int recThr = server_.arg("recthr").toInt();
        if (recThr < 200) recThr = 200;
        if (recThr > 8000) recThr = 8000;
        AppSettings::recoilThresholdMilliG = static_cast<int16_t>(recThr);
        AppSettings::recoilLockoutMs = static_cast<uint32_t>(server_.arg("reclo").toInt());
        AppSettings::recoilRatio = server_.arg("recrt").toFloat();
        AppSettings::recoilSharpnessEnabled = formBool(server_, "recshen");
        AppSettings::recoilSharpness = server_.arg("recsh").toFloat();
        AppSettings::save();
    }

    const char* dlyminV[] = {"0.5", "1.0", "1.5", "2.0", "2.5", "3.0"};
    const float dlyminN[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    const char* dlymaxV[] = {"2.0", "2.5", "3.0", "3.5", "4.0", "5.0", "6.0"};
    const float dlymaxN[] = {2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f};
    const char* volV[] = {"0", "80", "150", "200", "255"};
    const char* volL[] = {"Mute", "80", "150", "200", "255"};
    const uint32_t volN[] = {0, 80, 150, 200, 255};
    const char* lockoutV[] = {"80", "120", "150", "200", "300", "400"};
    const uint32_t lockoutN[] = {80, 120, 150, 200, 300, 400};
    const char* ratioV[] = {"0.3", "0.4", "0.5", "0.55", "0.6", "0.7", "0.8", "0.9"};
    const char* ratioL[] = {"30%", "40%", "50%", "55%", "60%", "70%", "80%", "90%"};
    const float ratioN[] = {0.3f, 0.4f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f};
    const char* specV[] = {"0.1", "0.15", "0.2", "0.25", "0.3", "0.35", "0.45", "0.55", "0.7"};
    const char* specL[] = {"10%", "15%", "20%", "25%", "30%", "35%", "45%", "55%", "70%"};
    const float specN[] = {0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.45f, 0.55f, 0.7f};
    const char* sharpV[] = {"0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9"};
    const char* sharpL[] = {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%"};
    const float sharpN[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};

    String html = pageHeader("Shot Timer Settings");
    html += "<h2>Shot Timer</h2><form method='POST'>";
    html += boolSelect("detmode", "Detect mode", AppSettings::shotDetectMode == 1, "Microphone", "Recoil");
    html += numberField("thr", "Mic threshold (0-32767)", AppSettings::shotThreshold, 2000, 30000, 500);
    html += selectField("dlymin", "Delay min", dlyminV, dlyminV, 6, idxFloat(dlyminN, 6, AppSettings::shotDelayMinS));
    html += selectField("dlymax", "Delay max", dlymaxV, dlymaxV, 7, idxFloat(dlymaxN, 7, AppSettings::shotDelayMaxS));
    html += selectField("vol", "Buzzer volume", volV, volL, 5, idxU32(volN, 5, AppSettings::buzzerVolume));
    html += "<h3>Microphone detection</h3>";
    html += selectField("echolo", "Echo lockout", lockoutV, lockoutV, 6, idxU32(lockoutN, 6, AppSettings::shotEchoLockoutMs));
    html += selectField("echort", "Echo ratio", ratioV, ratioL, 8, idxFloat(ratioN, 8, AppSettings::shotEchoRatio));
    html += boolSelect("specen", "Spectral filter", AppSettings::shotSpectralEnabled, "Off", "On");
    html += selectField("specrt", "Spectral threshold", specV, specL, 9, idxFloat(specN, 9, AppSettings::shotSpectralRatio));
    html += "<h3>Recoil detection</h3>";
    html += numberField("recthr", "Recoil threshold (milli-g)", AppSettings::recoilThresholdMilliG, 200, 8000, 100);
    html += selectField("reclo", "Recoil lockout", lockoutV, lockoutV, 6, idxU32(lockoutN, 6, AppSettings::recoilLockoutMs));
    html += selectField("recrt", "Recoil ratio", ratioV, ratioL, 8, idxFloat(ratioN, 8, AppSettings::recoilRatio));
    html += boolSelect("recshen", "Sharpness filter", AppSettings::recoilSharpnessEnabled, "Off", "On");
    html += selectField("recsh", "Sharpness threshold", sharpV, sharpL, 9, idxFloat(sharpN, 9, AppSettings::recoilSharpness));
    html += "<button type='submit'>Save</button></form>";
    html += "<a class='back' href='/settings'>&laquo; Back to settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsAntiCant() {
    lastRequestMs_ = millis();
    if (server_.method() == HTTP_POST) {
        AppSettings::cantGreenDeg = server_.arg("green").toFloat();
        AppSettings::cantYellowDeg = server_.arg("yellow").toFloat();
        if (AppSettings::cantYellowDeg < AppSettings::cantGreenDeg) AppSettings::cantYellowDeg = AppSettings::cantGreenDeg;
        AppSettings::cantCalibCountdownS = server_.arg("countdown").toFloat();
        AppSettings::save();
    }

    const char* greenV[] = {"0.5", "1.0", "1.5", "2.0"};
    const float greenN[] = {0.5f, 1.0f, 1.5f, 2.0f};
    const char* yellowV[] = {"2.0", "3.0", "4.0", "5.0"};
    const float yellowN[] = {2.0f, 3.0f, 4.0f, 5.0f};
    const char* cdV[] = {"0.0", "1.0", "2.0", "3.0", "5.0"};
    const char* cdL[] = {"Off", "1s", "2s", "3s", "5s"};
    const float cdN[] = {0.0f, 1.0f, 2.0f, 3.0f, 5.0f};

    String html = pageHeader("Anti-Cant Settings");
    html += "<h2>Anti-Cant</h2><form method='POST'>";
    html += selectField("green", "Green threshold (deg)", greenV, greenV, 4, idxFloat(greenN, 4, AppSettings::cantGreenDeg));
    html += selectField("yellow", "Yellow threshold (deg)", yellowV, yellowV, 4, idxFloat(yellowN, 4, AppSettings::cantYellowDeg));
    html += selectField("countdown", "Calibration countdown", cdV, cdL, 5, idxFloat(cdN, 5, AppSettings::cantCalibCountdownS));
    html += "<button type='submit'>Save</button></form>";
    html += "<a class='back' href='/settings'>&laquo; Back to settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsStability() {
    lastRequestMs_ = millis();
    if (server_.method() == HTTP_POST) {
        AppSettings::stabilityGreenMoa = server_.arg("green").toFloat();
        AppSettings::stabilityYellowMoa = server_.arg("yellow").toFloat();
        if (AppSettings::stabilityYellowMoa < AppSettings::stabilityGreenMoa) {
            AppSettings::stabilityGreenMoa = AppSettings::stabilityYellowMoa;
        }
        AppSettings::stabilityGraphMaxMoa = server_.arg("graphmax").toFloat();
        AppSettings::stabilityDeadzoneMoa = server_.arg("deadzone").toFloat();
        AppSettings::save();
    }

    const char* greenV[] = {"1.0", "1.5", "2.0", "2.5", "3.0"};
    const float greenN[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    const char* yellowV[] = {"3.0", "4.0", "5.0", "6.0", "8.0"};
    const float yellowN[] = {3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
    const char* graphV[] = {"4.0", "6.0", "8.0", "10.0", "12.0"};
    const float graphN[] = {4.0f, 6.0f, 8.0f, 10.0f, 12.0f};
    const char* dzV[] = {"0.0", "0.2", "0.3", "0.5", "0.8", "1.2", "1.8", "2.5"};
    const float dzN[] = {0.0f, 0.2f, 0.3f, 0.5f, 0.8f, 1.2f, 1.8f, 2.5f};

    String html = pageHeader("Stability Settings");
    html += "<h2>Stability</h2><form method='POST'>";
    html += selectField("green", "Green threshold (MOA)", greenV, greenV, 5, idxFloat(greenN, 5, AppSettings::stabilityGreenMoa));
    html += selectField("yellow", "Yellow threshold (MOA)", yellowV, yellowV, 5, idxFloat(yellowN, 5, AppSettings::stabilityYellowMoa));
    html += selectField("graphmax", "Graph max (MOA)", graphV, graphV, 5, idxFloat(graphN, 5, AppSettings::stabilityGraphMaxMoa));
    html += selectField("deadzone", "Deadzone (MOA)", dzV, dzV, 8, idxFloat(dzN, 8, AppSettings::stabilityDeadzoneMoa));
    html += "<button type='submit'>Save</button></form>";
    html += "<a class='back' href='/settings'>&laquo; Back to settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsSystem() {
    lastRequestMs_ = millis();
    if (server_.method() == HTTP_POST) {
        AppSettings::cpuFreqMhz = static_cast<uint16_t>(server_.arg("cpu").toInt());
        AppSettings::applyCpuFreq();
        AppSettings::imuPollDelayMs = static_cast<uint16_t>(server_.arg("imu").toInt());
        AppSettings::battReadIntervalMs = static_cast<uint16_t>(server_.arg("batt").toInt());
        AppSettings::save();
    }

    const char* cpuV[] = {"80", "160", "240"};
    const char* cpuL[] = {"80 MHz", "160 MHz", "240 MHz"};
    const uint32_t cpuN[] = {80, 160, 240};
    const char* imuV[] = {"0", "2", "5", "10", "20"};
    const char* imuL[] = {"Off", "2 ms", "5 ms", "10 ms", "20 ms"};
    const uint32_t imuN[] = {0, 2, 5, 10, 20};
    const char* battV[] = {"0", "250", "500", "1000", "2000", "5000"};
    const char* battL[] = {"Always", "250 ms", "500 ms", "1 s", "2 s", "5 s"};
    const uint32_t battN[] = {0, 250, 500, 1000, 2000, 5000};

    String html = pageHeader("System Settings");
    html += "<h2>System</h2><form method='POST'>";
    html += selectField("cpu", "CPU clock", cpuV, cpuL, 3, idxU32(cpuN, 3, AppSettings::cpuFreqMhz));
    html += selectField("imu", "IMU poll delay", imuV, imuL, 5, idxU32(imuN, 5, AppSettings::imuPollDelayMs));
    html += selectField("batt", "Battery read interval", battV, battL, 6, idxU32(battN, 6, AppSettings::battReadIntervalMs));
    html += "<button type='submit'>Save</button></form>";
    html += "<form method='POST' action='/settings/system/reset' onsubmit=\"return confirm('Reset all settings to defaults?')\">";
    html += "<button type='submit' style='background:#a33;color:#fff'>Reset to defaults</button></form>";
    html += "<a class='back' href='/settings'>&laquo; Back to settings</a>";
    html += pageFooter();
    server_.send(200, "text/html", html);
}

void WifiPortal::handleSettingsReset() {
    lastRequestMs_ = millis();
    AppSettings::resetDefaults();
    AppSettings::applyDisplay();
    AppSettings::applyCpuFreq();
    AppSettings::save();
    server_.sendHeader("Location", "/settings/system", true);
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
    server_.on("/settings", HTTP_GET, [this]() { handleSettingsHome(); });
    server_.on("/settings/display", HTTP_GET, [this]() { handleSettingsDisplay(); });
    server_.on("/settings/display", HTTP_POST, [this]() { handleSettingsDisplay(); });
    server_.on("/settings/shottimer", HTTP_GET, [this]() { handleSettingsShotTimer(); });
    server_.on("/settings/shottimer", HTTP_POST, [this]() { handleSettingsShotTimer(); });
    server_.on("/settings/anticant", HTTP_GET, [this]() { handleSettingsAntiCant(); });
    server_.on("/settings/anticant", HTTP_POST, [this]() { handleSettingsAntiCant(); });
    server_.on("/settings/stability", HTTP_GET, [this]() { handleSettingsStability(); });
    server_.on("/settings/stability", HTTP_POST, [this]() { handleSettingsStability(); });
    server_.on("/settings/system", HTTP_GET, [this]() { handleSettingsSystem(); });
    server_.on("/settings/system", HTTP_POST, [this]() { handleSettingsSystem(); });
    server_.on("/settings/system/reset", HTTP_POST, [this]() { handleSettingsReset(); });
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
