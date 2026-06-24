#pragma once

#include <WebServer.h>

// Shared web-UI building blocks used by both WifiPortal (AP captive portal) and WifiConnector
// (STA mode, once connected to a real network) -- the device's settings should be reachable the
// same way regardless of which radio mode got you onto the page. Registers /status plus one page
// per Settings category (mirroring the on-device menu) on whichever WebServer instance is passed
// in; the AP-specific WiFi-join form ("/", "/save") stays in WifiPortal itself, since it relies
// on AP-only behaviour (network scan, captive-portal redirect).
namespace WebUI {

// includeWifiSetupLink: whether the sidebar should link back to "/" (the WiFi-join page) --
// true when registered on WifiPortal's AP server, false on WifiConnector's STA server (there is
// no "/" WiFi-join page to link to there).
void registerSettingsRoutes(WebServer& server, bool includeWifiSetupLink);

// Wraps body HTML in the shared sidebar/logo page shell. Exposed so WifiPortal can give its own
// AP-specific pages ("/", "/save" results) the same look.
String pageShell(const char* title, const char* activeNavId, const String& bodyHtml);

} // namespace WebUI
