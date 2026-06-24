#pragma once

#include <cstdint>

// Central configuration persisted in NVS. Values here are already resolved (e.g. milliseconds,
// brightness 0-255), not stored as indices -- the Settings module knows the discrete option
// lists and writes the chosen value directly here.
namespace AppSettings {

constexpr uint32_t kTimeoutNever = 0xFFFFFFFFu;

extern uint8_t brightnessActive;   // 0-255
extern uint8_t brightnessDim;      // 0-255
extern uint32_t dimTimeoutMs;      // kTimeoutNever = never dim
extern uint32_t sleepTimeoutMs;    // kTimeoutNever = never turn off
extern bool screenFlipped;         // false = rotation 0, true = rotation 2 (180 degrees)
extern int accentColorIndex;       // index into kAccentPalette (see AppSettings.cpp)

extern int16_t shotThreshold;      // microphone peak threshold (0..32767) -- directly editable as a number
extern float shotDelayMinS;
extern float shotDelayMaxS;
extern uint8_t buzzerVolume;       // 0-255, 0 = mute
extern uint32_t shotEchoLockoutMs; // minimum gap between two detected shots
extern float shotEchoRatio;        // minimum loudness (relative to the last shot) within the soft echo window
extern bool shotSpectralEnabled;   // use frequency analysis to tell clicks/button presses apart from shots
extern float shotSpectralRatio;    // minimum fraction of high-frequency energy to count as "shot-like"

extern int shotDetectMode;            // 0 = microphone, 1 = recoil (IMU acceleration spike)
extern int16_t recoilThresholdMilliG; // peak deviation of total acceleration above 1g, in milli-g
extern uint32_t recoilLockoutMs;      // minimum gap between two detected shots (suppresses recoil settling)
extern float recoilRatio;             // minimum deviation (relative to the last shot) within the soft settling window
extern bool recoilSharpnessEnabled;   // check rise speed to tell bumps/shake apart from a real recoil impulse
extern float recoilSharpness;         // minimum sharpness (0..1) of the rise to count as recoil

extern float cantGreenDeg;
extern float cantYellowDeg;
extern float cantCalibCountdownS; // seconds of countdown after "confirm" before the reference measurement (0 = off)

extern float stabilityGreenMoa;
extern float stabilityYellowMoa;
extern float stabilityGraphMaxMoa;
extern float stabilityDeadzoneMoa; // below this threshold the reading counts as "0" (at rest)

extern uint16_t cpuFreqMhz;        // CPU clock rate (80/160/240) -- lower = less power at the same APB clock
extern uint16_t imuPollDelayMs;    // pause per iteration in the IMU live loops, so the CPU can idle in between
extern uint16_t battReadIntervalMs; // minimum gap between two battery-level reads from the PMIC (0 = every redraw)

constexpr int kAccentPaletteCount = 8;
uint16_t accentColor(int index);

void load();           // load from NVS (default if not present)
void save();           // write current values to NVS
void resetDefaults();  // reset RAM values to defaults (not yet saved)
void applyDisplay();   // apply brightness/rotation/accent color immediately
void applyCpuFreq();   // apply CPU clock rate immediately

} // namespace AppSettings
