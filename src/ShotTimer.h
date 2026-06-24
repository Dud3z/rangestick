#pragma once

#include "AppModule.h"
#include <cstddef>
#include <cstdint>

// Shot timer: random delay -> buzzer beep starts the clock -> a shot is detected -> splits are
// logged. Two selectable detection modes (AppSettings::shotDetectMode, switchable on the IDLE
// screen via "hold B"):
//   0 = microphone: amplitude peak in the audio signal.
//   1 = recoil: peak deviation of total IMU acceleration above 1g (recoil impulse).
//
// Both detection modes are structured the same way, only the raw signal differs:
//   1. Threshold: minimum peak to even count as a candidate.
//   2. Settling suppression (two stages, for mic: room echo; for recoil: the weapon's mechanical
//      settling/recoil after the shot):
//        a) Hard lockout window (shotEchoLockoutMs / recoilLockoutMs) -- no detection directly
//           after.
//        b) Soft window after that (a multiple of the hard window): a peak only counts there if
//           it's at least shotEchoRatio/recoilRatio as strong as the previous shot.
//   3. Shape discriminator to reject false positives (click/button press, or bump/shake) even if
//      they were loud/strong enough:
//        - Mic: frequency analysis (FFT) -- a shot is acoustically broadband, a click is narrow.
//        - Recoil: rise sharpness -- a real recoil impulse jumps to the peak within a few
//          samples, a bump/shake builds up more slowly.
//
// Learn mode (hold PWR in IDLE): records the full settling profile of the CURRENTLY SELECTED
// detection mode on real shots and suggests matching values for threshold/lockout/ratio/
// discriminator from that, instead of the user having to try values by hand.
class ShotTimer : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "SHOT TIMER"; }
    // Not busy only in IDLE -- during delay/running/result/learning the screen should not dim,
    // even if no button is pressed in the meantime.
    bool isBusy() const override { return state_ != State::IDLE; }

private:
    enum class State { IDLE, DELAY, RUNNING, STOPPED, LEARN_ARMED, LEARN_RESULT };

    static constexpr int MAX_SHOTS = 20;
    static constexpr uint32_t BEEP_BLANK_MS = 200;    // pause right after the beep (switchover/ringing time)
    static constexpr size_t MIC_SAMPLES = 128;        // ~8ms chunk at 16kHz -> detection resolution
    static constexpr uint32_t MIC_SAMPLE_RATE = 16000;
    static constexpr uint32_t ECHO_WINDOW_MULTIPLIER = 3; // soft window = lockout window * factor
    static constexpr int LIST_VISIBLE_ROWS = 10;          // shots per page in the results list

    static constexpr int RECOIL_BUF_SIZE = 12;        // ring buffer for the rise-sharpness measurement
    static constexpr int RECOIL_SHARPNESS_LOOKBACK = 4; // samples before the peak compared for "rise"

    static constexpr int LEARN_MAX_SHOTS = 50;       // generous cap, not a hard practical limit
    static constexpr int LEARN_MAX_SECONDARY = 8;    // settling events per shot
    static constexpr uint32_t LEARN_WINDOW_MS = 600;  // capture window after each primary peak
    static constexpr uint32_t LEARN_MIN_GAP_MS = 15;  // debouncing within the recording

    struct LearnedShot {
        int16_t primaryPeak = 0;
        float primaryDiscriminator = 0.0f; // spectral ratio (mic) or sharpness (recoil)
        int secondaryCount = 0;
        uint32_t secondaryTimeMs[LEARN_MAX_SECONDARY] = {0};
        int16_t secondaryPeak[LEARN_MAX_SECONDARY] = {0};
    };

    State state_ = State::IDLE;
    uint32_t stateStartMs_ = 0;
    uint32_t delayMs_ = 0;
    uint32_t beepMs_ = 0;
    uint32_t lastShotElapsed_ = 0;
    int16_t lastShotPeak_ = 0;
    uint32_t stopElapsed_ = 0;

    uint32_t shotTimes_[MAX_SHOTS] = {0};
    int shotCount_ = 0;

    // Threshold lives as a number in AppSettings::shotThreshold/recoilThresholdMilliG (editable
    // in the settings menu); BtnB/PWR in IDLE still offer the quick direct toggle on the
    // respective active field.
    int listScroll_ = 0; // page index of the shot list in State::STOPPED

    int16_t micBuf_[MIC_SAMPLES] = {0};
    float recoilBuf_[RECOIL_BUF_SIZE] = {0.0f};
    int recoilBufIdx_ = 0;
    bool modeToggleFired_ = false; // long-B in IDLE = switch mode (once per press)

    uint32_t lastDrawMs_ = 0;
    uint32_t flashUntilMs_ = 0; // brief visual flash on shot detection

    LearnedShot learnedShots_[LEARN_MAX_SHOTS];
    int learnedShotCount_ = 0;
    bool learnCapturing_ = false;
    uint32_t learnPrimaryMs_ = 0;
    uint32_t learnLastPeakMs_ = 0;
    uint32_t learnSuggestLockoutMs_ = 0;
    float learnSuggestRatio_ = 0.0f;
    int16_t learnSuggestThreshold_ = 0;
    float learnSuggestDiscriminator_ = 0.0f;

    void startDelay();
    void playBeepAndArm();
    void armForLearning();
    void disarmMic();
    void pollMic();
    void pollRecoil();
    void pollLearn();
    void registerLearnSample(int16_t peak, float discriminator);
    void finishLearning();
    void draw();
    int16_t threshold() const;        // microphone threshold
    int16_t recoilThreshold() const;  // recoil threshold (milli-g)
    float computeSpectralRatio() const;  // fraction of high-frequency energy in the current micBuf_ chunk
    float computeRecoilSharpness() const; // rise sharpness of the current recoilBuf_ peak (0..1)
};
