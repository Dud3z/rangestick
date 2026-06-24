#include "AppSettings.h"
#include <M5Unified.h>
#include <Preferences.h>
#include "Theme.h"

namespace AppSettings {

uint8_t brightnessActive = 180;
uint8_t brightnessDim = 20;
uint32_t dimTimeoutMs = 20000;
uint32_t sleepTimeoutMs = 180000;
bool screenFlipped = false;
int accentColorIndex = 0;

int16_t shotThreshold = 12000;
float shotDelayMinS = 1.0f;
float shotDelayMaxS = 4.0f;
uint8_t buzzerVolume = 200;
uint32_t shotEchoLockoutMs = 150;
float shotEchoRatio = 0.55f;
bool shotSpectralEnabled = true;
float shotSpectralRatio = 0.35f;

int shotDetectMode = 0;
int16_t recoilThresholdMilliG = 1500;
uint32_t recoilLockoutMs = 150;
float recoilRatio = 0.55f;
bool recoilSharpnessEnabled = true;
float recoilSharpness = 0.5f;

float cantGreenDeg = 1.0f;
float cantYellowDeg = 3.0f;
float cantCalibCountdownS = 3.0f;

float stabilityGreenMoa = 1.5f;
float stabilityYellowMoa = 4.0f;
float stabilityGraphMaxMoa = 8.0f;
float stabilityDeadzoneMoa = 0.3f;

uint16_t cpuFreqMhz = 80;
uint16_t imuPollDelayMs = 5;
uint16_t battReadIntervalMs = 1000;

namespace {
constexpr uint16_t kAccentPalette[kAccentPaletteCount] = {
    rgb565(0, 200, 255),   // Cyan
    rgb565(255, 150, 20),  // Orange
    rgb565(60, 220, 130),  // Gruen
    rgb565(170, 90, 255),  // Lila
    rgb565(255, 70, 70),   // Rot
    rgb565(70, 130, 255),  // Blau
    rgb565(255, 220, 40),  // Gelb
    rgb565(255, 90, 180),  // Pink
};
} // namespace

uint16_t accentColor(int index) {
    if (index < 0 || index >= kAccentPaletteCount) index = 0;
    return kAccentPalette[index];
}

void resetDefaults() {
    brightnessActive = 180;
    brightnessDim = 20;
    dimTimeoutMs = 20000;
    sleepTimeoutMs = 180000;
    screenFlipped = false;
    accentColorIndex = 0;
    shotThreshold = 12000;
    shotDelayMinS = 1.0f;
    shotDelayMaxS = 4.0f;
    buzzerVolume = 200;
    shotEchoLockoutMs = 150;
    shotEchoRatio = 0.55f;
    shotSpectralEnabled = true;
    shotSpectralRatio = 0.35f;
    shotDetectMode = 0;
    recoilThresholdMilliG = 1500;
    recoilLockoutMs = 150;
    recoilRatio = 0.55f;
    recoilSharpnessEnabled = true;
    recoilSharpness = 0.5f;
    cantGreenDeg = 1.0f;
    cantYellowDeg = 3.0f;
    cantCalibCountdownS = 3.0f;
    stabilityGreenMoa = 1.5f;
    stabilityYellowMoa = 4.0f;
    stabilityGraphMaxMoa = 8.0f;
    stabilityDeadzoneMoa = 0.3f;
    cpuFreqMhz = 80;
    imuPollDelayMs = 5;
    battReadIntervalMs = 1000;
}

void load() {
    Preferences prefs;
    prefs.begin("cfg", true);
    brightnessActive = prefs.getUChar("brAct", brightnessActive);
    brightnessDim = prefs.getUChar("brDim", brightnessDim);
    dimTimeoutMs = prefs.getUInt("dimTo", dimTimeoutMs);
    sleepTimeoutMs = prefs.getUInt("sleepTo", sleepTimeoutMs);
    screenFlipped = prefs.getBool("flip", screenFlipped);
    accentColorIndex = prefs.getInt("accent", accentColorIndex);
    shotThreshold = prefs.getShort("shotThr", shotThreshold);
    shotDelayMinS = prefs.getFloat("dlyMin", shotDelayMinS);
    shotDelayMaxS = prefs.getFloat("dlyMax", shotDelayMaxS);
    buzzerVolume = prefs.getUChar("vol", buzzerVolume);
    shotEchoLockoutMs = prefs.getUInt("echoLo", shotEchoLockoutMs);
    shotEchoRatio = prefs.getFloat("echoRt", shotEchoRatio);
    shotSpectralEnabled = prefs.getBool("specEn", shotSpectralEnabled);
    shotSpectralRatio = prefs.getFloat("specRt", shotSpectralRatio);
    shotDetectMode = prefs.getInt("detMode", shotDetectMode);
    recoilThresholdMilliG = prefs.getShort("recThr", recoilThresholdMilliG);
    recoilLockoutMs = prefs.getUInt("recLo", recoilLockoutMs);
    recoilRatio = prefs.getFloat("recRt", recoilRatio);
    recoilSharpnessEnabled = prefs.getBool("recShEn", recoilSharpnessEnabled);
    recoilSharpness = prefs.getFloat("recSh", recoilSharpness);
    cantGreenDeg = prefs.getFloat("cantG", cantGreenDeg);
    cantYellowDeg = prefs.getFloat("cantY", cantYellowDeg);
    cantCalibCountdownS = prefs.getFloat("cantCd", cantCalibCountdownS);
    stabilityGreenMoa = prefs.getFloat("stabG", stabilityGreenMoa);
    stabilityYellowMoa = prefs.getFloat("stabY", stabilityYellowMoa);
    stabilityGraphMaxMoa = prefs.getFloat("stabMax", stabilityGraphMaxMoa);
    stabilityDeadzoneMoa = prefs.getFloat("stabDz", stabilityDeadzoneMoa);
    cpuFreqMhz = prefs.getUShort("cpuFreq", cpuFreqMhz);
    imuPollDelayMs = prefs.getUShort("imuDelay", imuPollDelayMs);
    battReadIntervalMs = prefs.getUShort("battInt", battReadIntervalMs);
    prefs.end();
}

void save() {
    Preferences prefs;
    prefs.begin("cfg", false);
    prefs.putUChar("brAct", brightnessActive);
    prefs.putUChar("brDim", brightnessDim);
    prefs.putUInt("dimTo", dimTimeoutMs);
    prefs.putUInt("sleepTo", sleepTimeoutMs);
    prefs.putBool("flip", screenFlipped);
    prefs.putInt("accent", accentColorIndex);
    prefs.putShort("shotThr", shotThreshold);
    prefs.putFloat("dlyMin", shotDelayMinS);
    prefs.putFloat("dlyMax", shotDelayMaxS);
    prefs.putUChar("vol", buzzerVolume);
    prefs.putUInt("echoLo", shotEchoLockoutMs);
    prefs.putFloat("echoRt", shotEchoRatio);
    prefs.putBool("specEn", shotSpectralEnabled);
    prefs.putFloat("specRt", shotSpectralRatio);
    prefs.putInt("detMode", shotDetectMode);
    prefs.putShort("recThr", recoilThresholdMilliG);
    prefs.putUInt("recLo", recoilLockoutMs);
    prefs.putFloat("recRt", recoilRatio);
    prefs.putBool("recShEn", recoilSharpnessEnabled);
    prefs.putFloat("recSh", recoilSharpness);
    prefs.putFloat("cantG", cantGreenDeg);
    prefs.putFloat("cantY", cantYellowDeg);
    prefs.putFloat("cantCd", cantCalibCountdownS);
    prefs.putFloat("stabG", stabilityGreenMoa);
    prefs.putFloat("stabY", stabilityYellowMoa);
    prefs.putFloat("stabMax", stabilityGraphMaxMoa);
    prefs.putFloat("stabDz", stabilityDeadzoneMoa);
    prefs.putUShort("cpuFreq", cpuFreqMhz);
    prefs.putUShort("imuDelay", imuPollDelayMs);
    prefs.putUShort("battInt", battReadIntervalMs);
    prefs.end();
}

void applyDisplay() {
    M5.Display.setBrightness(brightnessActive);
    M5.Display.setRotation(screenFlipped ? 2 : 0);
    Theme::ACCENT = accentColor(accentColorIndex);
}

void applyCpuFreq() {
    setCpuFrequencyMhz(cpuFreqMhz);
}

} // namespace AppSettings
