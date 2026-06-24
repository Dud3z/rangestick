#include "WifiConnector.h"
#include <WiFi.h>
#include <cstring>

void WifiConnector::start() {
    state_ = State::LIST;
    selectedIndex_ = 0;
    errorMsg_[0] = 0;
    // Reflect an already-active connection (e.g. left on from a previous visit) in the list.
    if (WiFi.status() == WL_CONNECTED) {
        strncpy(connectedSsid_, WiFi.SSID().c_str(), sizeof(connectedSsid_) - 1);
        connectedSsid_[sizeof(connectedSsid_) - 1] = 0;
    }
}

int WifiConnector::networkCount() const {
    return WifiConfig::count();
}

void WifiConnector::networkSsid(int index, char* buf) const {
    char pass[WifiConfig::kMaxPasswordLen + 1];
    WifiConfig::get(index, buf, pass);
}

bool WifiConnector::isConnectedTo(int index) const {
    if (WiFi.status() != WL_CONNECTED || connectedSsid_[0] == 0) return false;
    char ssid[WifiConfig::kMaxSsidLen + 1];
    networkSsid(index, ssid);
    return strcmp(ssid, connectedSsid_) == 0;
}

void WifiConnector::moveSelection(int dir) {
    int n = networkCount();
    if (n == 0) return;
    selectedIndex_ = (selectedIndex_ + dir + n) % n;
}

void WifiConnector::confirmSelection() {
    int n = networkCount();
    if (n == 0) return;
    errorMsg_[0] = 0;

    if (isConnectedTo(selectedIndex_)) {
        disconnect();
        return;
    }

    char ssid[WifiConfig::kMaxSsidLen + 1];
    char pass[WifiConfig::kMaxPasswordLen + 1];
    WifiConfig::get(selectedIndex_, ssid, pass);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    connectingIndex_ = selectedIndex_;
    connectStartMs_ = millis();
    state_ = State::CONNECTING;
}

void WifiConnector::loop() {
    if (state_ != State::CONNECTING) return;
    if (WiFi.status() == WL_CONNECTED) {
        char ssid[WifiConfig::kMaxSsidLen + 1];
        char pass[WifiConfig::kMaxPasswordLen + 1];
        WifiConfig::get(connectingIndex_, ssid, pass);
        strncpy(connectedSsid_, ssid, sizeof(connectedSsid_) - 1);
        connectedSsid_[sizeof(connectedSsid_) - 1] = 0;
        state_ = State::LIST;
    } else if (millis() - connectStartMs_ > kConnectTimeoutMs) {
        WiFi.disconnect();
        strncpy(errorMsg_, "Connection failed", sizeof(errorMsg_) - 1);
        errorMsg_[sizeof(errorMsg_) - 1] = 0;
        state_ = State::LIST;
    }
}

void WifiConnector::disconnect() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    connectedSsid_[0] = 0;
    state_ = State::LIST;
}
