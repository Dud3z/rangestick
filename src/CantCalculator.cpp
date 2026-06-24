#include "CantCalculator.h"
#include "AppSettings.h"
#include <Arduino.h>
#include <cmath>

float CantCalculator::rollDegFor(const float v[3]) const {
    int p1 = (boreAxis_ + 1) % 3;
    int p2 = (boreAxis_ + 2) % 3;
    return atan2f(v[p1], v[p2]) * 180.0f / static_cast<float>(M_PI);
}

void CantCalculator::captureLevelRef() {
    refGravity_[0] = fx_;
    refGravity_[1] = fy_;
    refGravity_[2] = fz_;
    gyroAccum_[0] = gyroAccum_[1] = gyroAccum_[2] = 0.0f;
    state_ = State::ROCK_GESTURE;
}

void CantCalculator::update(float ax, float ay, float az, float gx, float gy, float gz) {
    if (!initialized_) {
        fx_ = ax;
        fy_ = ay;
        fz_ = az;
        initialized_ = true;
    } else {
        fx_ += ALPHA * (ax - fx_);
        fy_ += ALPHA * (ay - fy_);
        fz_ += ALPHA * (az - fz_);
    }

    if (state_ == State::LEVEL_COUNTDOWN) {
        if (millis() - countdownStartMs_ >= countdownDurationMs_) {
            captureLevelRef(); // Countdown abgelaufen -> jetzige (hoffentlich ruhige) Lage uebernehmen
        }
    } else if (state_ == State::ROCK_GESTURE) {
        gyroAccum_[0] += fabsf(gx);
        gyroAccum_[1] += fabsf(gy);
        gyroAccum_[2] += fabsf(gz);
    } else if (state_ == State::READY) {
        float v[3] = {fx_, fy_, fz_};
        float d = rollDegFor(v) - refAngleDeg_;
        while (d > 180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        angleDeg_ = d;
    }
}

void CantCalculator::confirm() {
    if (state_ == State::LEVEL_REF) {
        uint32_t ms = static_cast<uint32_t>(AppSettings::cantCalibCountdownS * 1000.0f);
        if (ms == 0) {
            captureLevelRef(); // Countdown deaktiviert -> sofort wie bisher
        } else {
            countdownDurationMs_ = ms;
            countdownStartMs_ = millis();
            state_ = State::LEVEL_COUNTDOWN;
        }
    } else if (state_ == State::ROCK_GESTURE) {
        // Schritt 2: Rohrachse = die Achse, um die beim Kippen tatsaechlich gedreht wurde.
        boreAxis_ = 0;
        if (gyroAccum_[1] > gyroAccum_[boreAxis_]) boreAxis_ = 1;
        if (gyroAccum_[2] > gyroAccum_[boreAxis_]) boreAxis_ = 2;
        refAngleDeg_ = rollDegFor(refGravity_);
        state_ = State::READY;
    } else if (state_ == State::READY) {
        state_ = State::LEVEL_REF; // neu kalibrieren
    }
    // State::LEVEL_COUNTDOWN: Tastendruck waehrend des Countdowns wird ignoriert -- der Nutzer
    // soll nach dem ersten Druck gerade nichts mehr anfassen muessen.
}

void CantCalculator::recalibrate() {
    state_ = State::LEVEL_REF;
}

int CantCalculator::countdownSecondsLeft() const {
    if (state_ != State::LEVEL_COUNTDOWN) return 0;
    uint32_t elapsed = millis() - countdownStartMs_;
    if (elapsed >= countdownDurationMs_) return 0;
    return static_cast<int>((countdownDurationMs_ - elapsed + 999) / 1000);
}
