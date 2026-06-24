#pragma once

#include <cstdint>

// Pure calculation/calibration logic for the cant (roll) angle, without UI. Used both by the
// standalone anti-cant module and by the combo mode (ComboView) -- via a shared global instance
// (see main.cpp), so a calibration performed once applies in both views instead of having to be
// done twice.
//
// The stick's mounting orientation on the rifle is arbitrary -- which IMU axis corresponds to
// the bore axis is therefore not known in advance. A single static calibration measurement is
// NOT reliable enough for this (see CantCalculator.cpp for details). Instead, the user performs
// a brief tilt motion (simulating cant); the axis with the largest gyroscope deflection during
// that motion IS the bore axis, regardless of mounting position.
//
// Step 1 (LEVEL_REF) requires "hold the weapon level AND press the button" at the same time --
// that's hard to do reliably with one hand. After the button press, a configurable countdown
// therefore runs first (AppSettings::cantCalibCountdownS, 0 = off) and the actual reference
// measurement is only taken at its end -- the user can press and then calmly hold still
// afterwards, instead of having to do both at the exact same moment.
class CantCalculator {
public:
    enum class State { LEVEL_REF, LEVEL_COUNTDOWN, ROCK_GESTURE, READY };

    // Call with an already-fetched IMU reading (read exactly once per frame by the respective
    // AppModule::loop()) -- this class does NOT read the sensor itself. This way, multiple
    // consumers (anti-cant + stability in combo mode) never read from the same sensor twice per
    // frame, which would otherwise let them snatch the "new data" flag away from each other.
    void update(float ax, float ay, float az, float gx, float gy, float gz);
    void confirm();       // "confirm" button: advance to the next calibration step, or recalibrate
    void recalibrate();   // jump straight back to step 1

    State state() const { return state_; }
    float angleDeg() const { return angleDeg_; }
    // Seconds left until the countdown ends, rounded up (3,2,1); 0 if not in countdown.
    int countdownSecondsLeft() const;

private:
    static constexpr float ALPHA = 0.2f; // smoothing (exponential moving average) on the raw accel values

    State state_ = State::LEVEL_REF;
    bool initialized_ = false;
    float fx_ = 0.0f, fy_ = 0.0f, fz_ = 0.0f; // filtered accel values

    float refGravity_[3] = {0.0f, 0.0f, 0.0f}; // gravity at "weapon level" (step 1)
    float gyroAccum_[3] = {0.0f, 0.0f, 0.0f};  // accumulated |gyro| per axis during the tilt motion

    int boreAxis_ = 0;         // 0=x,1=y,2=z -- bore axis detected via the tilt motion
    float refAngleDeg_ = 0.0f; // roll angle at calibration time (= new zero point)
    float angleDeg_ = 0.0f;

    uint32_t countdownStartMs_ = 0;
    uint32_t countdownDurationMs_ = 0;

    float rollDegFor(const float v[3]) const;
    void captureLevelRef(); // adopt gravity now as the reference, advance to ROCK_GESTURE
};
