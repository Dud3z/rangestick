#include "WebUI.h"
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include "AppSettings.h"

namespace WebUI {

namespace {

// Set once per call to registerSettingsRoutes() and read while rendering the shell -- safe
// because only one of the AP server / STA server is ever actually answering requests at a time
// (entering one WiFi flow on-device tears down the other's radio mode first).
bool gShowWifiSetupLink = false;

constexpr const char* kLogoSvg =
    "<svg width='26' height='26' viewBox='0 0 32 32' style='vertical-align:middle;margin-right:8px'>"
    "<circle cx='16' cy='16' r='13' stroke='#0c8' stroke-width='2' fill='none'/>"
    "<line x1='16' y1='1' x2='16' y2='9' stroke='#0c8' stroke-width='2'/>"
    "<line x1='16' y1='23' x2='16' y2='31' stroke='#0c8' stroke-width='2'/>"
    "<line x1='1' y1='16' x2='9' y2='16' stroke='#0c8' stroke-width='2'/>"
    "<line x1='23' y1='16' x2='31' y2='16' stroke='#0c8' stroke-width='2'/>"
    "<circle cx='16' cy='16' r='3' fill='#0c8'/></svg>";

constexpr const char* kCss =
    ":root{--bg:#111;--panel:#1b1b1f;--fg:#eee;--sub:#999;--accent:#0c8;--border:#333}"
    "*{box-sizing:border-box}"
    "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg);margin:0}"
    "header{display:flex;align-items:center;background:var(--panel);padding:10px 14px;"
    "border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10}"
    "header .brand{font-weight:bold;font-size:18px;display:flex;align-items:center}"
    "header button{background:none;border:none;color:var(--fg);font-size:22px;margin-right:10px;cursor:pointer}"
    ".layout{display:flex}"
    "nav{background:var(--panel);width:220px;flex-shrink:0;border-right:1px solid var(--border);"
    "position:fixed;top:53px;bottom:0;left:-220px;transition:left .2s;overflow-y:auto;z-index:9}"
    "nav.open{left:0}"
    "nav a{display:block;padding:14px 18px;color:var(--fg);text-decoration:none;border-bottom:1px solid var(--border)}"
    "nav a.active{background:var(--accent);color:#000;font-weight:bold}"
    "main{flex:1;padding:18px;max-width:480px;margin:0 auto}"
    "@media (min-width:760px){nav{left:0;position:static}header button{display:none}}"
    "h2{margin-top:0}h3{color:var(--sub);font-size:14px;text-transform:uppercase;margin:22px 0 4px}"
    "label{display:block;margin-top:12px;font-size:13px;color:var(--sub)}"
    "select,input{width:100%;padding:9px;margin-top:4px;box-sizing:border-box;background:#222;"
    "color:#eee;border:1px solid var(--border);border-radius:6px;font-size:16px}"
    "button,.btn{display:block;width:100%;padding:12px;background:var(--accent);color:#000;border:none;"
    "font-weight:bold;margin-top:14px;text-align:center;text-decoration:none;border-radius:6px;"
    "box-sizing:border-box;font-size:16px;cursor:pointer}"
    ".btn2{background:#333;color:#eee}"
    ".danger{background:#a33;color:#fff}"
    ".card{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:4px 14px;margin-bottom:14px}"
    ".kv{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}"
    ".kv:last-child{border-bottom:none}"
    ".kv span:first-child{color:var(--sub)}";

String navLink(const char* href, const char* label, const char* activeId, const char* id) {
    String s = "<a href='";
    s += href;
    s += "'";
    if (strcmp(activeId, id) == 0) s += " class='active'";
    s += ">";
    s += label;
    s += "</a>";
    return s;
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

void handleStatus(WebServer& server) {
    bool apActive = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
    bool staConnected = (WiFi.status() == WL_CONNECTED);

    String body = "<h2>Status</h2><div class='card'>";
    body += "<div class='kv'><span>Mode</span><span>";
    body += apActive ? "Access point" : (staConnected ? "WiFi client" : "Off");
    body += "</span></div>";
    if (apActive) {
        body += "<div class='kv'><span>AP name</span><span>RangeStick</span></div>";
        body += "<div class='kv'><span>AP address</span><span>" + WiFi.softAPIP().toString() + "</span></div>";
        body += "<div class='kv'><span>Devices joined</span><span>" + String(WiFi.softAPgetStationNum()) + "</span></div>";
    }
    if (staConnected) {
        body += "<div class='kv'><span>Network</span><span>" + WiFi.SSID() + "</span></div>";
        body += "<div class='kv'><span>IP address</span><span>" + WiFi.localIP().toString() + "</span></div>";
        body += "<div class='kv'><span>Signal</span><span>" + String(WiFi.RSSI()) + " dBm</span></div>";
        body += "<div class='kv'><span>Hostname</span><span>" + String(WiFi.getHostname()) + "</span></div>";
    }
    if (!apActive && !staConnected) {
        body += "<div class='kv'><span>WiFi</span><span>disconnected</span></div>";
    }
    body += "</div>";
    server.send(200, "text/html", pageShell("Status", "status", body));
}

void handleSettingsDisplay(WebServer& server) {
    if (server.method() == HTTP_POST) {
        AppSettings::brightnessActive = static_cast<uint8_t>(server.arg("bract").toInt());
        AppSettings::brightnessDim = static_cast<uint8_t>(server.arg("brdim").toInt());
        // "never" is a text sentinel, not a literal number -- kTimeoutNever (4294967295) would
        // overflow the signed 32-bit long that String::toInt() parses into.
        String dimto = server.arg("dimto");
        AppSettings::dimTimeoutMs = (dimto == "never") ? AppSettings::kTimeoutNever : static_cast<uint32_t>(dimto.toInt());
        String sleepto = server.arg("sleepto");
        AppSettings::sleepTimeoutMs = (sleepto == "never") ? AppSettings::kTimeoutNever : static_cast<uint32_t>(sleepto.toInt());
        AppSettings::screenFlipped = formBool(server, "flip");
        AppSettings::accentColorIndex = server.arg("accent").toInt();
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

    String body = "<h2>Display</h2><form method='POST'>";
    body += selectField("bract", "Active brightness", bractV, bractV, 6, idxU32(bractN, 6, AppSettings::brightnessActive));
    body += selectField("brdim", "Dimmed brightness", brdimV, brdimV, 6, idxU32(brdimN, 6, AppSettings::brightnessDim));
    body += selectField("dimto", "Dim after", dimtoV, dimtoL, 5, idxU32(dimtoN, 5, AppSettings::dimTimeoutMs));
    body += selectField("sleepto", "Sleep after", sleeptoV, sleeptoL, 6, idxU32(sleeptoN, 6, AppSettings::sleepTimeoutMs));
    body += boolSelect("flip", "Screen", AppSettings::screenFlipped, "Normal", "180 deg");
    body += selectField("accent", "Accent color", accentV, accentL, 8, AppSettings::accentColorIndex);
    body += "<button type='submit'>Save</button></form>";
    server.send(200, "text/html", pageShell("Display", "display", body));
}

void handleSettingsShotTimer(WebServer& server) {
    if (server.method() == HTTP_POST) {
        AppSettings::shotDetectMode = formBool(server, "detmode") ? 1 : 0;
        int thr = server.arg("thr").toInt();
        if (thr < 2000) thr = 2000;
        if (thr > 30000) thr = 30000;
        AppSettings::shotThreshold = static_cast<int16_t>(thr);
        AppSettings::shotDelayMinS = server.arg("dlymin").toFloat();
        AppSettings::shotDelayMaxS = server.arg("dlymax").toFloat();
        if (AppSettings::shotDelayMaxS < AppSettings::shotDelayMinS) AppSettings::shotDelayMaxS = AppSettings::shotDelayMinS;
        AppSettings::buzzerVolume = static_cast<uint8_t>(server.arg("vol").toInt());
        AppSettings::shotEchoLockoutMs = static_cast<uint32_t>(server.arg("echolo").toInt());
        AppSettings::shotEchoRatio = server.arg("echort").toFloat();
        AppSettings::shotSpectralEnabled = formBool(server, "specen");
        AppSettings::shotSpectralRatio = server.arg("specrt").toFloat();
        int recThr = server.arg("recthr").toInt();
        if (recThr < 200) recThr = 200;
        if (recThr > 8000) recThr = 8000;
        AppSettings::recoilThresholdMilliG = static_cast<int16_t>(recThr);
        AppSettings::recoilLockoutMs = static_cast<uint32_t>(server.arg("reclo").toInt());
        AppSettings::recoilRatio = server.arg("recrt").toFloat();
        AppSettings::recoilSharpnessEnabled = formBool(server, "recshen");
        AppSettings::recoilSharpness = server.arg("recsh").toFloat();
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

    String body = "<h2>Shot Timer</h2><form method='POST'>";
    body += boolSelect("detmode", "Detect mode", AppSettings::shotDetectMode == 1, "Microphone", "Recoil");
    body += numberField("thr", "Mic threshold (0-32767)", AppSettings::shotThreshold, 2000, 30000, 500);
    body += selectField("dlymin", "Delay min", dlyminV, dlyminV, 6, idxFloat(dlyminN, 6, AppSettings::shotDelayMinS));
    body += selectField("dlymax", "Delay max", dlymaxV, dlymaxV, 7, idxFloat(dlymaxN, 7, AppSettings::shotDelayMaxS));
    body += selectField("vol", "Buzzer volume", volV, volL, 5, idxU32(volN, 5, AppSettings::buzzerVolume));
    body += "<h3>Microphone detection</h3>";
    body += selectField("echolo", "Echo lockout", lockoutV, lockoutV, 6, idxU32(lockoutN, 6, AppSettings::shotEchoLockoutMs));
    body += selectField("echort", "Echo ratio", ratioV, ratioL, 8, idxFloat(ratioN, 8, AppSettings::shotEchoRatio));
    body += boolSelect("specen", "Spectral filter", AppSettings::shotSpectralEnabled, "Off", "On");
    body += selectField("specrt", "Spectral threshold", specV, specL, 9, idxFloat(specN, 9, AppSettings::shotSpectralRatio));
    body += "<h3>Recoil detection</h3>";
    body += numberField("recthr", "Recoil threshold (milli-g)", AppSettings::recoilThresholdMilliG, 200, 8000, 100);
    body += selectField("reclo", "Recoil lockout", lockoutV, lockoutV, 6, idxU32(lockoutN, 6, AppSettings::recoilLockoutMs));
    body += selectField("recrt", "Recoil ratio", ratioV, ratioL, 8, idxFloat(ratioN, 8, AppSettings::recoilRatio));
    body += boolSelect("recshen", "Sharpness filter", AppSettings::recoilSharpnessEnabled, "Off", "On");
    body += selectField("recsh", "Sharpness threshold", sharpV, sharpL, 9, idxFloat(sharpN, 9, AppSettings::recoilSharpness));
    body += "<button type='submit'>Save</button></form>";
    server.send(200, "text/html", pageShell("Shot Timer", "shottimer", body));
}

void handleSettingsAntiCant(WebServer& server) {
    if (server.method() == HTTP_POST) {
        AppSettings::cantGreenDeg = server.arg("green").toFloat();
        AppSettings::cantYellowDeg = server.arg("yellow").toFloat();
        if (AppSettings::cantYellowDeg < AppSettings::cantGreenDeg) AppSettings::cantYellowDeg = AppSettings::cantGreenDeg;
        AppSettings::cantCalibCountdownS = server.arg("countdown").toFloat();
        AppSettings::save();
    }

    const char* greenV[] = {"0.5", "1.0", "1.5", "2.0"};
    const float greenN[] = {0.5f, 1.0f, 1.5f, 2.0f};
    const char* yellowV[] = {"2.0", "3.0", "4.0", "5.0"};
    const float yellowN[] = {2.0f, 3.0f, 4.0f, 5.0f};
    const char* cdV[] = {"0.0", "1.0", "2.0", "3.0", "5.0"};
    const char* cdL[] = {"Off", "1s", "2s", "3s", "5s"};
    const float cdN[] = {0.0f, 1.0f, 2.0f, 3.0f, 5.0f};

    String body = "<h2>Anti-Cant</h2><form method='POST'>";
    body += selectField("green", "Green threshold (deg)", greenV, greenV, 4, idxFloat(greenN, 4, AppSettings::cantGreenDeg));
    body += selectField("yellow", "Yellow threshold (deg)", yellowV, yellowV, 4, idxFloat(yellowN, 4, AppSettings::cantYellowDeg));
    body += selectField("countdown", "Calibration countdown", cdV, cdL, 5, idxFloat(cdN, 5, AppSettings::cantCalibCountdownS));
    body += "<button type='submit'>Save</button></form>";
    server.send(200, "text/html", pageShell("Anti-Cant", "anticant", body));
}

void handleSettingsStability(WebServer& server) {
    if (server.method() == HTTP_POST) {
        AppSettings::stabilityGreenMoa = server.arg("green").toFloat();
        AppSettings::stabilityYellowMoa = server.arg("yellow").toFloat();
        if (AppSettings::stabilityYellowMoa < AppSettings::stabilityGreenMoa) {
            AppSettings::stabilityGreenMoa = AppSettings::stabilityYellowMoa;
        }
        AppSettings::stabilityGraphMaxMoa = server.arg("graphmax").toFloat();
        AppSettings::stabilityDeadzoneMoa = server.arg("deadzone").toFloat();
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

    String body = "<h2>Stability</h2><form method='POST'>";
    body += selectField("green", "Green threshold (MOA)", greenV, greenV, 5, idxFloat(greenN, 5, AppSettings::stabilityGreenMoa));
    body += selectField("yellow", "Yellow threshold (MOA)", yellowV, yellowV, 5, idxFloat(yellowN, 5, AppSettings::stabilityYellowMoa));
    body += selectField("graphmax", "Graph max (MOA)", graphV, graphV, 5, idxFloat(graphN, 5, AppSettings::stabilityGraphMaxMoa));
    body += selectField("deadzone", "Deadzone (MOA)", dzV, dzV, 8, idxFloat(dzN, 8, AppSettings::stabilityDeadzoneMoa));
    body += "<button type='submit'>Save</button></form>";
    server.send(200, "text/html", pageShell("Stability", "stability", body));
}

void handleSettingsSystem(WebServer& server) {
    if (server.method() == HTTP_POST) {
        AppSettings::cpuFreqMhz = static_cast<uint16_t>(server.arg("cpu").toInt());
        AppSettings::applyCpuFreq();
        AppSettings::imuPollDelayMs = static_cast<uint16_t>(server.arg("imu").toInt());
        AppSettings::battReadIntervalMs = static_cast<uint16_t>(server.arg("batt").toInt());
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

    String body = "<h2>System</h2><form method='POST'>";
    body += selectField("cpu", "CPU clock", cpuV, cpuL, 3, idxU32(cpuN, 3, AppSettings::cpuFreqMhz));
    body += selectField("imu", "IMU poll delay", imuV, imuL, 5, idxU32(imuN, 5, AppSettings::imuPollDelayMs));
    body += selectField("batt", "Battery read interval", battV, battL, 6, idxU32(battN, 6, AppSettings::battReadIntervalMs));
    body += "<button type='submit'>Save</button></form>";
    body += "<form method='POST' action='/settings/system/reset' onsubmit=\"return confirm('Reset all settings to defaults?')\">";
    body += "<button type='submit' class='danger'>Reset to defaults</button></form>";
    server.send(200, "text/html", pageShell("System", "system", body));
}

void handleSettingsReset(WebServer& server) {
    AppSettings::resetDefaults();
    AppSettings::applyDisplay();
    AppSettings::applyCpuFreq();
    AppSettings::save();
    server.sendHeader("Location", "/settings/system", true);
    server.send(302, "text/plain", "");
}

} // namespace

String pageShell(const char* title, const char* activeNavId, const String& bodyHtml) {
    String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>RangeStick - ";
    s += title;
    s += "</title><style>";
    s += kCss;
    s += "</style></head><body>";
    s += "<header><button onclick=\"document.getElementById('nav').classList.toggle('open')\">&#9776;</button>";
    s += "<span class='brand'>";
    s += kLogoSvg;
    s += "RangeStick</span></header>";
    s += "<div class='layout'><nav id='nav'>";
    s += navLink("/status", "Status", activeNavId, "status");
    if (gShowWifiSetupLink) {
        s += navLink("/", "WiFi Setup", activeNavId, "wifi");
    }
    s += navLink("/settings/display", "Display", activeNavId, "display");
    s += navLink("/settings/shottimer", "Shot Timer", activeNavId, "shottimer");
    s += navLink("/settings/anticant", "Anti-Cant", activeNavId, "anticant");
    s += navLink("/settings/stability", "Stability", activeNavId, "stability");
    s += navLink("/settings/system", "System", activeNavId, "system");
    s += "</nav><main>";
    s += bodyHtml;
    s += "</main></div></body></html>";
    return s;
}

void registerSettingsRoutes(WebServer& server, bool includeWifiSetupLink) {
    gShowWifiSetupLink = includeWifiSetupLink;
    server.on("/status", HTTP_GET, [&server]() { handleStatus(server); });
    server.on("/settings", HTTP_GET, [&server]() { handleStatus(server); });
    server.on("/settings/display", HTTP_GET, [&server]() { handleSettingsDisplay(server); });
    server.on("/settings/display", HTTP_POST, [&server]() { handleSettingsDisplay(server); });
    server.on("/settings/shottimer", HTTP_GET, [&server]() { handleSettingsShotTimer(server); });
    server.on("/settings/shottimer", HTTP_POST, [&server]() { handleSettingsShotTimer(server); });
    server.on("/settings/anticant", HTTP_GET, [&server]() { handleSettingsAntiCant(server); });
    server.on("/settings/anticant", HTTP_POST, [&server]() { handleSettingsAntiCant(server); });
    server.on("/settings/stability", HTTP_GET, [&server]() { handleSettingsStability(server); });
    server.on("/settings/stability", HTTP_POST, [&server]() { handleSettingsStability(server); });
    server.on("/settings/system", HTTP_GET, [&server]() { handleSettingsSystem(server); });
    server.on("/settings/system", HTTP_POST, [&server]() { handleSettingsSystem(server); });
    server.on("/settings/system/reset", HTTP_POST, [&server]() { handleSettingsReset(server); });
}

} // namespace WebUI
