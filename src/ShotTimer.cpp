#include "ShotTimer.h"
#include <M5Unified.h>
#include <arduinoFFT.h>
#include <cmath>
#include "Canvas.h"
#include "Theme.h"
#include "AppSettings.h"

namespace {
constexpr int16_t kThresholdMin = 2000;
constexpr int16_t kThresholdMax = 30000;
constexpr int16_t kThresholdStep = 500;
constexpr int16_t kRecoilMin = 200;   // 0.2g
constexpr int16_t kRecoilMax = 8000;  // 8g
constexpr int16_t kRecoilStep = 100;  // 0.1g
} // namespace

int16_t ShotTimer::threshold() const { return AppSettings::shotThreshold; }
int16_t ShotTimer::recoilThreshold() const { return AppSettings::recoilThresholdMilliG; }

float ShotTimer::computeSpectralRatio() const {
    float vReal[MIC_SAMPLES];
    float vImag[MIC_SAMPLES];
    for (size_t i = 0; i < MIC_SAMPLES; ++i) {
        vReal[i] = static_cast<float>(micBuf_[i]);
        vImag[i] = 0.0f;
    }

    ArduinoFFT<float> fft(vReal, vImag, static_cast<uint16_t>(MIC_SAMPLES), static_cast<float>(MIC_SAMPLE_RATE));
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    // 16kHz / 128 Samples = 125 Hz pro Bin. Bin 0 (Gleichanteil) wird ausgelassen, Bin 8 (~1kHz)
    // trennt "tief" von "hoch". Ein Schuss ist breitbandig -- viel Energie auch oberhalb 1kHz;
    // ein kurzer Klick/Tastendruck ist meist eng/tieffrequent.
    constexpr int kSplitBin = 8;
    constexpr int kHighBandEnd = MIC_SAMPLES / 2;
    float lowEnergy = 0.0f, highEnergy = 0.0f;
    for (int i = 1; i < kSplitBin; ++i) lowEnergy += vReal[i];
    for (int i = kSplitBin; i < kHighBandEnd; ++i) highEnergy += vReal[i];

    float total = lowEnergy + highEnergy;
    if (total < 1.0f) return 0.0f;
    return highEnergy / total;
}

float ShotTimer::computeRecoilSharpness() const {
    // Wie schnell ist das Signal auf den (gerade erst geschriebenen) Peak angestiegen? Ein
    // echter Rueckstossimpuls braucht dafuer nur 1-2 Samples, ein Stoss/Wackeln baut sich ueber
    // mehr Samples gleichmaessig auf. 1.0 = Sprung von ~0 auf den Peak, 0.0 = kein Unterschied.
    int peakIdx = (recoilBufIdx_ - 1 + RECOIL_BUF_SIZE) % RECOIL_BUF_SIZE;
    float peak = recoilBuf_[peakIdx];
    if (peak < 0.05f) return 0.0f;

    int priorIdx = (peakIdx - RECOIL_SHARPNESS_LOOKBACK + RECOIL_BUF_SIZE) % RECOIL_BUF_SIZE;
    float priorValue = recoilBuf_[priorIdx];

    float ratio = priorValue / peak;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    return 1.0f - ratio;
}

void ShotTimer::onEnter() {
    state_ = State::IDLE;
    shotCount_ = 0;
    listScroll_ = 0;
    lastShotPeak_ = 0;
    lastDrawMs_ = 0;
    flashUntilMs_ = 0;
    modeToggleFired_ = false;
    recoilBufIdx_ = 0;
    for (auto& v : recoilBuf_) v = 0.0f;
}

void ShotTimer::onExit() {
    disarmMic();
}

void ShotTimer::disarmMic() {
    if (M5.Mic.isEnabled()) {
        M5.Mic.end();
    }
    M5.Speaker.begin();
}

void ShotTimer::startDelay() {
    state_ = State::DELAY;
    stateStartMs_ = millis();
    long minMs = static_cast<long>(AppSettings::shotDelayMinS * 1000.0f);
    long maxMs = static_cast<long>(AppSettings::shotDelayMaxS * 1000.0f);
    if (maxMs <= minMs) maxMs = minMs + 1;
    delayMs_ = random(minMs, maxMs);
}

void ShotTimer::playBeepAndArm() {
    // Lautsprecher und Mikrofon teilen sich den I2S-Bus -> nacheinander umschalten.
    if (M5.Mic.isEnabled()) {
        M5.Mic.end();
    }
    M5.Speaker.begin();
    M5.Speaker.setVolume(AppSettings::buzzerVolume);
    M5.Speaker.tone(1800, 120);
    delay(130);
    M5.Speaker.end();

    if (AppSettings::shotDetectMode == 0) {
        auto cfg = M5.Mic.config();
        cfg.sample_rate = MIC_SAMPLE_RATE;
        M5.Mic.config(cfg);
        M5.Mic.begin();
    } else {
        recoilBufIdx_ = 0;
        for (auto& v : recoilBuf_) v = 0.0f; // alte Auschlaege duerfen nicht als Anstieg gelten
    }

    beepMs_ = millis();
    shotCount_ = 0;
    lastShotElapsed_ = 0;
    lastShotPeak_ = 0;
    state_ = State::RUNNING;
}

void ShotTimer::armForLearning() {
    // Wie playBeepAndArm(), nur ohne Beep -- fuer den Lernmodus, der keine Zeit stoppt.
    if (AppSettings::shotDetectMode == 0) {
        if (M5.Mic.isEnabled()) {
            M5.Mic.end();
        }
        M5.Speaker.end();

        auto cfg = M5.Mic.config();
        cfg.sample_rate = MIC_SAMPLE_RATE;
        M5.Mic.config(cfg);
        M5.Mic.begin();
    } else {
        recoilBufIdx_ = 0;
        for (auto& v : recoilBuf_) v = 0.0f;
    }
}

void ShotTimer::pollMic() {
    if (!M5.Mic.isEnabled()) return;
    M5.Mic.record(micBuf_, MIC_SAMPLES, MIC_SAMPLE_RATE);

    int16_t peak = 0;
    for (size_t i = 0; i < MIC_SAMPLES; ++i) {
        int16_t v = micBuf_[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    if (peak <= threshold()) return;

    if (AppSettings::shotSpectralEnabled && computeSpectralRatio() < AppSettings::shotSpectralRatio) {
        return; // laut genug, aber klingt nicht breitbandig genug fuer einen Schuss (z.B. Klick)
    }

    uint32_t elapsed = millis() - beepMs_;
    if (elapsed <= BEEP_BLANK_MS || shotCount_ >= MAX_SHOTS) return;

    if (shotCount_ > 0) {
        uint32_t sinceLast = elapsed - lastShotElapsed_;
        uint32_t lockoutMs = AppSettings::shotEchoLockoutMs;
        if (sinceLast <= lockoutMs) return; // hartes Sperrfenster, garantiert dieselbe Schallwelle
        if (sinceLast <= lockoutMs * ECHO_WINDOW_MULTIPLIER) {
            // Weiches Fenster: Wandecho ist durch die Reflexion leiser als der Originalknall --
            // nur akzeptieren, wenn fast so laut wie der vorherige (echte) Schuss.
            if (peak < static_cast<int16_t>(lastShotPeak_ * AppSettings::shotEchoRatio)) return;
        }
    }

    shotTimes_[shotCount_++] = elapsed;
    lastShotElapsed_ = elapsed;
    lastShotPeak_ = peak;
    flashUntilMs_ = millis() + 150;
}

void ShotTimer::pollRecoil() {
    // Pause bleibt klein genug, um die deutlich langsamere IMU-Abtastrate nicht zu unterschreiten --
    // spart Busy-Spin-CPU-Zyklen ohne die Schusserkennung zu verzoegern.
    delay(AppSettings::imuPollDelayMs);
    if (!M5.Imu.update()) return;
    auto d = M5.Imu.getImuData();
    float mag = sqrtf(d.accel.x * d.accel.x + d.accel.y * d.accel.y + d.accel.z * d.accel.z);
    float deviationG = fabsf(mag - 1.0f);

    recoilBuf_[recoilBufIdx_] = deviationG;
    recoilBufIdx_ = (recoilBufIdx_ + 1) % RECOIL_BUF_SIZE;

    int16_t peak = static_cast<int16_t>(deviationG * 1000.0f);
    if (peak <= recoilThreshold()) return;

    if (AppSettings::recoilSharpnessEnabled && computeRecoilSharpness() < AppSettings::recoilSharpness) {
        return; // starker Ausschlag, aber zu langsamer Anstieg fuer einen echten Rueckstossimpuls
    }

    uint32_t elapsed = millis() - beepMs_;
    if (elapsed <= BEEP_BLANK_MS || shotCount_ >= MAX_SHOTS) return;

    if (shotCount_ > 0) {
        uint32_t sinceLast = elapsed - lastShotElapsed_;
        uint32_t lockoutMs = AppSettings::recoilLockoutMs;
        if (sinceLast <= lockoutMs) return; // hartes Sperrfenster, garantiert dasselbe Nachschwingen
        if (sinceLast <= lockoutMs * ECHO_WINDOW_MULTIPLIER) {
            // Weiches Fenster: das mechanische Nachschwingen/Setzen ist schwaecher als der
            // urspruengliche Rueckstoss -- nur akzeptieren, wenn fast so stark wie der vorherige Schuss.
            if (peak < static_cast<int16_t>(lastShotPeak_ * AppSettings::recoilRatio)) return;
        }
    }

    shotTimes_[shotCount_++] = elapsed;
    lastShotElapsed_ = elapsed;
    lastShotPeak_ = peak;
    flashUntilMs_ = millis() + 150;
}

void ShotTimer::registerLearnSample(int16_t peak, float discriminator) {
    uint32_t now = millis();
    if (now - learnLastPeakMs_ < LEARN_MIN_GAP_MS) return; // Entprellen
    learnLastPeakMs_ = now;

    bool startNew = !learnCapturing_;
    if (learnCapturing_ && (now - learnPrimaryMs_) > LEARN_WINDOW_MS) {
        startNew = true; // Aufnahmefenster des vorherigen Schusses ist vorbei -> das ist ein neuer
    }

    if (startNew) {
        if (learnedShotCount_ >= LEARN_MAX_SHOTS) return; // genug Daten, weitere Schuesse ignorieren
        LearnedShot& shot = learnedShots_[learnedShotCount_++];
        shot.primaryPeak = peak;
        shot.primaryDiscriminator = discriminator;
        shot.secondaryCount = 0;
        learnPrimaryMs_ = now;
        learnCapturing_ = true;
        flashUntilMs_ = now + 150;
    } else {
        LearnedShot& shot = learnedShots_[learnedShotCount_ - 1];
        if (shot.secondaryCount < LEARN_MAX_SECONDARY) {
            shot.secondaryTimeMs[shot.secondaryCount] = now - learnPrimaryMs_;
            shot.secondaryPeak[shot.secondaryCount] = peak;
            shot.secondaryCount++;
        }
    }
}

void ShotTimer::pollLearn() {
    if (AppSettings::shotDetectMode == 0) {
        if (!M5.Mic.isEnabled()) return;
        M5.Mic.record(micBuf_, MIC_SAMPLES, MIC_SAMPLE_RATE);

        int16_t peak = 0;
        for (size_t i = 0; i < MIC_SAMPLES; ++i) {
            int16_t v = micBuf_[i];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
        }
        if (peak <= threshold()) return;
        registerLearnSample(peak, computeSpectralRatio());
    } else {
        delay(AppSettings::imuPollDelayMs);
        if (!M5.Imu.update()) return;
        auto d = M5.Imu.getImuData();
        float mag = sqrtf(d.accel.x * d.accel.x + d.accel.y * d.accel.y + d.accel.z * d.accel.z);
        float deviationG = fabsf(mag - 1.0f);

        recoilBuf_[recoilBufIdx_] = deviationG;
        recoilBufIdx_ = (recoilBufIdx_ + 1) % RECOIL_BUF_SIZE;

        int16_t peak = static_cast<int16_t>(deviationG * 1000.0f);
        if (peak <= recoilThreshold()) return;
        registerLearnSample(peak, computeRecoilSharpness());
    }
}

void ShotTimer::finishLearning() {
    disarmMic();

    // Ueber alle aufgezeichneten Schuesse: wie spaet und wie stark ist der schlimmste
    // Nachschwinger? Sperrfenster und Verhaeltnis werden so vorgeschlagen, dass genau dieser
    // schlimmste Fall noch unterdrueckt wuerde, mit etwas Sicherheitsabstand.
    uint32_t maxEchoTimeMs = 0;
    float maxEchoRatio = 0.0f;
    int16_t minPrimaryPeak = 32767;
    float minDiscriminator = 1.0f;
    for (int i = 0; i < learnedShotCount_; ++i) {
        const LearnedShot& shot = learnedShots_[i];
        if (shot.primaryPeak <= 0) continue;
        if (shot.primaryPeak < minPrimaryPeak) minPrimaryPeak = shot.primaryPeak;
        if (shot.primaryDiscriminator < minDiscriminator) minDiscriminator = shot.primaryDiscriminator;
        for (int j = 0; j < shot.secondaryCount; ++j) {
            float ratio = static_cast<float>(shot.secondaryPeak[j]) / static_cast<float>(shot.primaryPeak);
            if (ratio > 0.15f) { // sehr schwache Ausschlaege fuer die Analyse ignorieren
                if (shot.secondaryTimeMs[j] > maxEchoTimeMs) maxEchoTimeMs = shot.secondaryTimeMs[j];
                if (ratio > maxEchoRatio) maxEchoRatio = ratio;
            }
        }
    }

    uint32_t lockout = maxEchoTimeMs + 40;
    if (lockout < 80) lockout = 80;
    if (lockout > 400) lockout = 400;

    float ratio = maxEchoRatio + 0.15f;
    if (ratio < 0.3f) ratio = 0.3f;
    if (ratio > 0.9f) ratio = 0.9f;

    int16_t suggestThreshold;
    float suggestDiscriminator;
    if (AppSettings::shotDetectMode == 0) {
        // Schwelle: deutlich unter dem leisesten aufgezeichneten Schuss ansetzen, damit auch
        // etwas leisere Folgeschuesse noch sicher erkannt werden.
        suggestThreshold = AppSettings::shotThreshold;
        if (minPrimaryPeak < 32767) {
            int v = static_cast<int>(minPrimaryPeak * 0.6f);
            if (v < kThresholdMin) v = kThresholdMin;
            if (v > kThresholdMax) v = kThresholdMax;
            suggestThreshold = static_cast<int16_t>(v);
        }
        suggestDiscriminator = AppSettings::shotSpectralRatio;
        if (minDiscriminator < 1.0f) {
            suggestDiscriminator = minDiscriminator - 0.1f;
            if (suggestDiscriminator < 0.05f) suggestDiscriminator = 0.05f;
        }
    } else {
        suggestThreshold = AppSettings::recoilThresholdMilliG;
        if (minPrimaryPeak < 32767) {
            int v = static_cast<int>(minPrimaryPeak * 0.6f);
            if (v < kRecoilMin) v = kRecoilMin;
            if (v > kRecoilMax) v = kRecoilMax;
            suggestThreshold = static_cast<int16_t>(v);
        }
        suggestDiscriminator = AppSettings::recoilSharpness;
        if (minDiscriminator < 1.0f) {
            suggestDiscriminator = minDiscriminator - 0.1f;
            if (suggestDiscriminator < 0.05f) suggestDiscriminator = 0.05f;
        }
    }

    learnSuggestLockoutMs_ = lockout;
    learnSuggestRatio_ = ratio;
    learnSuggestThreshold_ = suggestThreshold;
    learnSuggestDiscriminator_ = suggestDiscriminator;
    state_ = State::LEARN_RESULT;
}

void ShotTimer::loop() {
    switch (state_) {
        case State::IDLE:
            // B lang = Erkennungsmodus wechseln (einmalig pro Druck); nur wenn das nicht
            // ausgeloest wurde, zaehlt die Tasten-Loslass-Aktion als "Schwelle hoch".
            if (M5.BtnB.pressedFor(600) && !modeToggleFired_) {
                modeToggleFired_ = true;
                AppSettings::shotDetectMode = 1 - AppSettings::shotDetectMode;
                AppSettings::save();
            }
            if (M5.BtnA.wasReleased()) {
                startDelay();
            } else if (M5.BtnB.wasReleased()) {
                if (!modeToggleFired_) {
                    if (AppSettings::shotDetectMode == 0) {
                        int v = AppSettings::shotThreshold + kThresholdStep; // Hoch
                        if (v > kThresholdMax) v = kThresholdMax;
                        AppSettings::shotThreshold = static_cast<int16_t>(v);
                    } else {
                        int v = AppSettings::recoilThresholdMilliG + kRecoilStep; // Hoch
                        if (v > kRecoilMax) v = kRecoilMax;
                        AppSettings::recoilThresholdMilliG = static_cast<int16_t>(v);
                    }
                    AppSettings::save();
                }
                modeToggleFired_ = false;
            } else if (M5.BtnPWR.wasClicked()) {
                if (AppSettings::shotDetectMode == 0) {
                    int v = AppSettings::shotThreshold - kThresholdStep; // Runter
                    if (v < kThresholdMin) v = kThresholdMin;
                    AppSettings::shotThreshold = static_cast<int16_t>(v);
                } else {
                    int v = AppSettings::recoilThresholdMilliG - kRecoilStep; // Runter
                    if (v < kRecoilMin) v = kRecoilMin;
                    AppSettings::recoilThresholdMilliG = static_cast<int16_t>(v);
                }
                AppSettings::save();
            } else if (M5.BtnPWR.wasHold()) {
                learnedShotCount_ = 0;
                learnCapturing_ = false;
                learnLastPeakMs_ = 0;
                armForLearning();
                state_ = State::LEARN_ARMED;
            }
            break;

        case State::DELAY:
            if (millis() - stateStartMs_ >= delayMs_) {
                playBeepAndArm();
            }
            break;

        case State::RUNNING:
            if (AppSettings::shotDetectMode == 0) pollMic(); else pollRecoil();
            if (M5.BtnA.wasReleased()) {
                // Interessiert die Zeit des letzten Schusses, nicht den Moment des Knopfdrucks
                // (der durch Reaktionszeit immer etwas spaeter liegt).
                stopElapsed_ = (shotCount_ > 0) ? shotTimes_[shotCount_ - 1] : (millis() - beepMs_);
                state_ = State::STOPPED;
                listScroll_ = 0;
            }
            break;

        case State::STOPPED: {
            int totalPages = (shotCount_ + LIST_VISIBLE_ROWS - 1) / LIST_VISIBLE_ROWS;
            if (totalPages < 1) totalPages = 1;
            if (M5.BtnA.wasReleased()) {
                startDelay();
            } else if (M5.BtnB.wasPressed()) {
                listScroll_ = (listScroll_ - 1 + totalPages) % totalPages; // Hoch
            } else if (M5.BtnPWR.wasClicked()) {
                listScroll_ = (listScroll_ + 1) % totalPages; // Runter
            }
            break;
        }

        case State::LEARN_ARMED:
            pollLearn();
            if (M5.BtnA.wasReleased() && learnedShotCount_ > 0) {
                finishLearning();
            } else if (M5.BtnPWR.wasHold()) {
                disarmMic();
                state_ = State::IDLE; // Lernmodus abbrechen
            }
            break;

        case State::LEARN_RESULT:
            if (M5.BtnA.wasReleased()) {
                if (AppSettings::shotDetectMode == 0) {
                    AppSettings::shotEchoLockoutMs = learnSuggestLockoutMs_;
                    AppSettings::shotEchoRatio = learnSuggestRatio_;
                    AppSettings::shotThreshold = learnSuggestThreshold_;
                    AppSettings::shotSpectralRatio = learnSuggestDiscriminator_;
                } else {
                    AppSettings::recoilLockoutMs = learnSuggestLockoutMs_;
                    AppSettings::recoilRatio = learnSuggestRatio_;
                    AppSettings::recoilThresholdMilliG = learnSuggestThreshold_;
                    AppSettings::recoilSharpness = learnSuggestDiscriminator_;
                }
                AppSettings::save();
                state_ = State::IDLE;
            } else if (M5.BtnB.wasPressed() || M5.BtnPWR.wasClicked()) {
                state_ = State::IDLE; // verwerfen
            }
            break;
    }

    // Anzeige bewusst auf ~14Hz gedrosselt, damit das Erkennungs-Polling in State::RUNNING
    // nicht durch teure Display-Redraws ausgebremst wird.
    uint32_t now = millis();
    if (now - lastDrawMs_ >= 70) {
        lastDrawMs_ = now;
        draw();
    }
}

void ShotTimer::draw() {
    canvas.fillScreen(Theme::BG);

    bool flashing = millis() < flashUntilMs_;
    if (flashing) {
        canvas.drawRect(0, 0, canvas.width(), canvas.height(), Theme::ACCENT2);
        canvas.drawRect(1, 1, canvas.width() - 2, canvas.height() - 2, Theme::ACCENT2);
    }

    canvas.setCursor(4, 2);
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::ACCENT, Theme::BG);
    canvas.print("SHOT TIMER");

    switch (state_) {
        case State::IDLE:
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, 40);
            canvas.setTextSize(2);
            canvas.print("Press A");
            canvas.setCursor(4, 65);
            canvas.print("to start");
            canvas.setTextSize(1);

            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setCursor(4, 130);
            canvas.print(AppSettings::shotDetectMode == 0 ? "Modus: Mikrofon" : "Modus: Rueckstoss");
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 142);
            canvas.print("B halten = wechseln");

            canvas.setCursor(4, 205);
            canvas.print("PWR halten = Lernmodus");
            canvas.setCursor(4, 220);
            if (AppSettings::shotDetectMode == 0) {
                canvas.printf("Schw [B/PWR]: %d", AppSettings::shotThreshold);
            } else {
                canvas.printf("Schw [B/PWR]: %.1fg", static_cast<double>(AppSettings::recoilThresholdMilliG) / 1000.0);
            }
            break;

        case State::DELAY: {
            int dots = 1 + (millis() / 300) % 3;
            char buf[4] = {0, 0, 0, 0};
            for (int i = 0; i < dots; ++i) buf[i] = '.';
            drawBigNumber(buf, canvas.width() / 2, 90, Theme::WARN, 1);
            break;
        }

        case State::RUNNING: {
            uint32_t elapsed = millis() - beepMs_;
            char buf[8];
            snprintf(buf, sizeof(buf), "%2lu.%02lu", elapsed / 1000, (elapsed % 1000) / 10);
            int numH = drawBigNumber(buf, canvas.width() / 2, 28, flashing ? Theme::ACCENT2 : Theme::ACCENT, 1);
            int y = 28 + numH + 14;

            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, y);
            canvas.printf("Shots: %d", shotCount_);
            if (shotCount_ > 0) {
                uint32_t last = shotTimes_[shotCount_ - 1];
                uint32_t split = (shotCount_ > 1) ? (last - shotTimes_[shotCount_ - 2]) : last;
                canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
                canvas.setCursor(4, y + 20);
                canvas.printf("Last:  %lu.%02lus", last / 1000, (last % 1000) / 10);
                canvas.setCursor(4, y + 40);
                canvas.printf("Split: %lu.%02lus", split / 1000, (split % 1000) / 10);
            }
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 220);
            canvas.print("A = stop");
            break;
        }

        case State::STOPPED: {
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 16);
            canvas.printf("LAST SHOT  (Shots: %d)", shotCount_);

            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(4, 28);
            canvas.printf("%lu.%02lus", stopElapsed_ / 1000, (stopElapsed_ % 1000) / 10);
            canvas.setTextSize(1);

            constexpr int kListTop = 56;
            constexpr int kRowPitch = 14;
            int totalPages = (shotCount_ + LIST_VISIBLE_ROWS - 1) / LIST_VISIBLE_ROWS;
            if (totalPages < 1) totalPages = 1;
            int pageStart = listScroll_ * LIST_VISIBLE_ROWS;

            for (int row = 0; row < LIST_VISIBLE_ROWS; ++row) {
                int idx = pageStart + row;
                if (idx >= shotCount_) break;
                uint32_t t = shotTimes_[idx];
                uint32_t split = (idx > 0) ? (t - shotTimes_[idx - 1]) : t;
                int y = kListTop + row * kRowPitch;
                canvas.setTextColor(Theme::TEXT, Theme::BG);
                canvas.setCursor(4, y);
                canvas.printf("#%-2d %5.2fs", idx + 1, static_cast<double>(t) / 1000.0);
                canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
                canvas.setCursor(82, y);
                canvas.printf("+%.2f", static_cast<double>(split) / 1000.0);
            }

            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 204);
            if (totalPages > 1) {
                canvas.printf("B/PWR=Seite (%d/%d)", listScroll_ + 1, totalPages);
            } else if (shotCount_ == 0) {
                canvas.print("Keine Schuesse erkannt");
            }
            canvas.setCursor(4, 220);
            canvas.print("A = run again");
            break;
        }

        case State::LEARN_ARMED: {
            canvas.setTextColor(Theme::ACCENT2, Theme::BG);
            canvas.setTextSize(2);
            canvas.setCursor(4, 28);
            canvas.print("LERNMODUS");
            canvas.setTextSize(1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 50);
            canvas.print(AppSettings::shotDetectMode == 0 ? "Modus: Mikrofon" : "Modus: Rueckstoss");
            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, 64);
            canvas.print("1-2 Schuss abgeben.");
            canvas.setCursor(4, 76);
            canvas.print("Echo wird mitgelernt.");

            char buf[8];
            snprintf(buf, sizeof(buf), "%d", learnedShotCount_);
            drawBigNumber(buf, canvas.width() / 2, 100, Theme::ACCENT, 1);
            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 160);
            canvas.print("Schuss(e) erkannt");

            canvas.setCursor(4, 204);
            canvas.print("A = fertig");
            canvas.setCursor(4, 220);
            canvas.print("PWR halten = Abbruch");
            break;
        }

        case State::LEARN_RESULT: {
            canvas.setTextColor(Theme::ACCENT, Theme::BG);
            canvas.setTextSize(1);
            canvas.setCursor(4, 16);
            canvas.print("Vorschlag:");

            constexpr int kValueX = 80;
            constexpr int kRowPitch = 18;
            int y = 36;

            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, y);
            canvas.print("Schwelle:");
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setCursor(kValueX, y);
            if (AppSettings::shotDetectMode == 0) {
                canvas.printf("%d", learnSuggestThreshold_);
            } else {
                canvas.printf("%.1fg", static_cast<double>(learnSuggestThreshold_) / 1000.0);
            }
            y += kRowPitch;

            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, y);
            canvas.print("Sperre:");
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setCursor(kValueX, y);
            canvas.printf("%ums", static_cast<unsigned>(learnSuggestLockoutMs_));
            y += kRowPitch;

            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, y);
            canvas.print("Verh.:");
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setCursor(kValueX, y);
            canvas.printf("%.0f%%", static_cast<double>(learnSuggestRatio_ * 100.0f));
            y += kRowPitch;

            canvas.setTextColor(Theme::TEXT, Theme::BG);
            canvas.setCursor(4, y);
            canvas.print(AppSettings::shotDetectMode == 0 ? "Spektral:" : "Schaerfe:");
            canvas.setTextColor(Theme::GOOD, Theme::BG);
            canvas.setCursor(kValueX, y);
            canvas.printf("%.0f%%", static_cast<double>(learnSuggestDiscriminator_ * 100.0f));

            canvas.setTextColor(Theme::SUBTEXT, Theme::BG);
            canvas.setCursor(4, 204);
            canvas.print("A = uebernehmen");
            canvas.setCursor(4, 220);
            canvas.print("B/PWR = verwerfen");
            break;
        }
    }
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}
