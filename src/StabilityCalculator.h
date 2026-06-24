#pragma once

#include <cstdint>

// Pure calculation logic for the weapon's "wobble", without UI. Used both by the standalone
// stability module and by the combo mode (ComboView) -- via a shared global instance (see
// main.cpp), so session/peak stay consistent across both views.
//
// Instead of fixed roll/pitch axes (which would depend on the stick's mounting orientation), the
// normalized (and low-pass filtered) gravity direction itself is tracked: for each sample, the
// angle to the averaged direction of the rolling window is computed (asin of the cross-product
// magnitude, numerically stable for small angles), an RMS is formed from that and converted to
// MOA (1 degree = 60 MOA).
class StabilityCalculator {
public:
    static constexpr int WINDOW = 50;        // ~1s at 50Hz sample rate
    static constexpr int HISTORY = 60;       // history graph, one entry every ~70ms (~4s window)
    static constexpr int SCATTER_COUNT = 24; // shot-group scatter plot, one point per sample

    // Call with an already-fetched IMU reading (read exactly once per frame by the respective
    // AppModule::loop()) -- this class does NOT read the sensor itself (still internally
    // throttled to SAMPLE_INTERVAL_MS). This way, multiple consumers (anti-cant + stability in
    // combo mode) never read from the same sensor twice per frame, which would otherwise let
    // them snatch the "new data" flag away from each other -- with the effect that stability
    // would only get real samples sporadically and never settle down (near 0).
    void update(float ax, float ay, float az);
    void resetSession();  // reset window/peak/history (e.g. new string)

    float wobbleMoa() const { return wobbleMoa_; }
    float peakMoa() const { return peakMoa_; }

    const float* history() const { return history_; }
    int historyIndex() const { return historyIndex_; }
    int historyCount() const { return historyCount_; }

    const float* scatterX() const { return scatterX_; }
    const float* scatterY() const { return scatterY_; }
    int scatterIndex() const { return scatterIndex_; }
    int scatterCount() const { return scatterCount_; }

private:
    static constexpr uint32_t SAMPLE_INTERVAL_MS = 20;
    static constexpr float ACCEL_FILTER_ALPHA = 0.15f; // low-pass against sensor noise

    void sample(float ax, float ay, float az);

    bool filterInit_ = false;
    float fax_ = 0.0f, fay_ = 0.0f, faz_ = 0.0f; // low-pass filtered accel values

    float axBuf_[WINDOW] = {0};
    float ayBuf_[WINDOW] = {0};
    float azBuf_[WINDOW] = {0};
    int bufIndex_ = 0;
    int bufCount_ = 0;

    float wobbleMoa_ = 0.0f;
    float peakMoa_ = 0.0f;

    float history_[HISTORY] = {0};
    int historyIndex_ = 0;
    int historyCount_ = 0;

    float scatterX_[SCATTER_COUNT] = {0};
    float scatterY_[SCATTER_COUNT] = {0};
    int scatterIndex_ = 0;
    int scatterCount_ = 0;

    uint32_t lastSampleMs_ = 0;
    uint32_t lastHistoryMs_ = 0;
};
