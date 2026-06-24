#include "StabilityCalculator.h"
#include <Arduino.h>
#include "AppSettings.h"
#include <cmath>

void StabilityCalculator::resetSession() {
    bufIndex_ = 0;
    bufCount_ = 0;
    peakMoa_ = 0.0f;
    historyIndex_ = 0;
    historyCount_ = 0;
    scatterIndex_ = 0;
    scatterCount_ = 0;
    // Also re-seed the low-pass filter: otherwise, when re-entering a stability module, the OLD
    // filtered orientation (from before leaving) would mix with the NEW pose in a single EMA
    // trace -- if the device was moved in the meantime, this produces a brief but massive
    // phantom spike even though it is now actually lying still.
    filterInit_ = false;
}

void StabilityCalculator::sample(float ax, float ay, float az) {
    // Low-pass on the raw values: the BMI270 noise alone already corresponds to about 0.1-0.15
    // degrees of angular jitter, which through the x60 MOA conversion looks like several MOA of
    // "wobble" -- even at complete rest. Without filtering, we'd be measuring pure sensor noise,
    // not the weapon's actual movement.
    if (!filterInit_) {
        fax_ = ax;
        fay_ = ay;
        faz_ = az;
        filterInit_ = true;
    } else {
        fax_ += ACCEL_FILTER_ALPHA * (ax - fax_);
        fay_ += ACCEL_FILTER_ALPHA * (ay - fay_);
        faz_ += ACCEL_FILTER_ALPHA * (az - faz_);
    }

    float norm = sqrtf(fax_ * fax_ + fay_ * fay_ + faz_ * faz_);
    if (norm < 0.01f) return; // invalid reading (free fall or similar), discard

    int latest = bufIndex_;
    axBuf_[bufIndex_] = fax_ / norm;
    ayBuf_[bufIndex_] = fay_ / norm;
    azBuf_[bufIndex_] = faz_ / norm;
    bufIndex_ = (bufIndex_ + 1) % WINDOW;
    if (bufCount_ < WINDOW) bufCount_++;

    if (bufCount_ < 4) return; // needs a few samples before scatter is meaningful

    float mx = 0, my = 0, mz = 0;
    for (int i = 0; i < bufCount_; ++i) { mx += axBuf_[i]; my += ayBuf_[i]; mz += azBuf_[i]; }
    float mlen = sqrtf(mx * mx + my * my + mz * mz);
    if (mlen < 0.01f) return;
    mx /= mlen; my /= mlen; mz /= mlen; // averaged orientation of the window, normalized

    // Deviation via the cross product (asin) instead of the dot product (acos): at the small
    // angles expected here, the dot-product value sits very close to 1.0, where acos() is
    // numerically unstable (its derivative diverges) -- tiny rounding noise would otherwise get
    // blown up into an artificial baseline of several degrees of "wobble", even at complete
    // rest. asin(|cross product|) is well-behaved for small angles.
    float sumSq = 0;
    for (int i = 0; i < bufCount_; ++i) {
        float cxp = ayBuf_[i] * mz - azBuf_[i] * my;
        float cyp = azBuf_[i] * mx - axBuf_[i] * mz;
        float czp = axBuf_[i] * my - ayBuf_[i] * mx;
        float crossMag = sqrtf(cxp * cxp + cyp * cyp + czp * czp);
        crossMag = constrain(crossMag, 0.0f, 1.0f);
        float devDeg = asinf(crossMag) * 180.0f / static_cast<float>(M_PI);
        sumSq += devDeg * devDeg;
    }
    float wobbleDeg = sqrtf(sumSq / bufCount_); // RMS deviation from the mean orientation

    wobbleMoa_ = wobbleDeg * 60.0f; // 1 degree = 60 MOA (angular unit, distance-independent)
    if (wobbleMoa_ < AppSettings::stabilityDeadzoneMoa) wobbleMoa_ = 0.0f; // hide residual noise at rest
    if (wobbleMoa_ > peakMoa_) peakMoa_ = wobbleMoa_;

    // Shot-group scatter plot: project the tangential deviation of the latest sample onto a
    // local 2D plane (u/v) perpendicular to the averaged direction -- yields a "shot group"
    // independent of the stick's mounting orientation.
    float hx = 0, hy = 0, hz = 1;
    if (fabsf(mz) > 0.9f) { hx = 1; hy = 0; hz = 0; }
    float ux = my * hz - mz * hy, uy = mz * hx - mx * hz, uz = mx * hy - my * hx;
    float ulen = sqrtf(ux * ux + uy * uy + uz * uz);
    if (ulen > 1e-6f) {
        ux /= ulen; uy /= ulen; uz /= ulen;
        float vx = my * uz - mz * uy, vy = mz * ux - mx * uz, vz = mx * uy - my * ux;

        float sx = axBuf_[latest], sy = ayBuf_[latest], sz = azBuf_[latest];
        float dot = sx * mx + sy * my + sz * mz;
        float devx = sx - dot * mx, devy = sy - dot * my, devz = sz - dot * mz;
        float px = constrain(devx * ux + devy * uy + devz * uz, -1.0f, 1.0f);
        float py = constrain(devx * vx + devy * vy + devz * vz, -1.0f, 1.0f);

        scatterX_[scatterIndex_] = asinf(px) * 180.0f / static_cast<float>(M_PI) * 60.0f; // MOA
        scatterY_[scatterIndex_] = asinf(py) * 180.0f / static_cast<float>(M_PI) * 60.0f;
        scatterIndex_ = (scatterIndex_ + 1) % SCATTER_COUNT;
        if (scatterCount_ < SCATTER_COUNT) scatterCount_++;
    }
}

void StabilityCalculator::update(float ax, float ay, float az) {
    uint32_t now = millis();
    if (now - lastSampleMs_ >= SAMPLE_INTERVAL_MS) {
        lastSampleMs_ = now;
        sample(ax, ay, az);
    }
    if (now - lastHistoryMs_ >= 70) {
        lastHistoryMs_ = now;
        history_[historyIndex_] = wobbleMoa_;
        historyIndex_ = (historyIndex_ + 1) % HISTORY;
        if (historyCount_ < HISTORY) historyCount_++;
    }
}
