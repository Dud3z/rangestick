#pragma once

#include "AppModule.h"
#include <cstddef>
#include <cstdint>

// Shot-Timer: Zufalls-Delay -> Buzzer-Beep startet die Zeit -> ein Schuss wird erkannt ->
// Splits werden geloggt. Zwei wahlweise Erkennungsarten (AppSettings::shotDetectMode, im
// IDLE-Bildschirm per "B halten" umschaltbar):
//   0 = Mikrofon: Amplituden-Peak im Schallsignal.
//   1 = Rueckstoss: Peak-Ausschlag der IMU-Gesamtbeschleunigung ueber 1g (Rueckstossimpuls).
//
// Beide Erkennungsarten sind gleich aufgebaut, nur das Rohsignal unterscheidet sich:
//   1. Schwelle: Mindest-Peak, um ueberhaupt als Kandidat zu gelten.
//   2. Nachschwing-Unterdrueckung (zwei Stufen, fuer Mic: Wandecho; fuer Rueckstoss: das
//      mechanische Nachschwingen/Setzen der Waffe nach dem Schuss):
//        a) Hartes Sperrfenster (shotEchoLockoutMs / recoilLockoutMs) -- keine Erkennung direkt
//           danach.
//        b) Weiches Fenster danach (Vielfaches des harten Fensters): ein Peak zaehlt dort nur,
//           wenn er mindestens shotEchoRatio/recoilRatio so stark ist wie der vorherige Schuss.
//   3. Form-Diskriminator, um Falsch-Positive (Klick/Tastendruck bzw. Stoss/Wackeln) zu
//      verwerfen, auch wenn sie laut/stark genug waren:
//        - Mic: Frequenzanalyse (FFT) -- ein Schuss ist akustisch breitbandig, ein Klick eng.
//        - Rueckstoss: Anstiegs-Schaerfe -- ein echter Rueckstossimpuls springt innerhalb
//          weniger Samples auf den Peak, ein Stoss/Wackeln baut sich langsamer auf.
//
// Lernmodus (PWR halten in IDLE): zeichnet bei echten Schuessen das volle Nachschwing-Profil
// der GERADE GEWAEHLTEN Erkennungsart auf und schlaegt daraus passende Werte fuer
// Schwelle/Sperre/Verhaeltnis/Diskriminator vor, statt dass der Nutzer sie von Hand durchprobiert.
class ShotTimer : public AppModule {
public:
    void onEnter() override;
    void onExit() override;
    void loop() override;
    const char* name() const override { return "SHOT TIMER"; }
    // Nicht busy nur in IDLE -- waehrend Delay/Laufen/Auswertung/Lernen soll der Bildschirm
    // nicht dimmen, auch wenn dabei keine Taste gedrueckt wird.
    bool isBusy() const override { return state_ != State::IDLE; }

private:
    enum class State { IDLE, DELAY, RUNNING, STOPPED, LEARN_ARMED, LEARN_RESULT };

    static constexpr int MAX_SHOTS = 20;
    static constexpr uint32_t BEEP_BLANK_MS = 200;    // Pause direkt nach dem Beep (Umschalt-/Ausschwing-Zeit)
    static constexpr size_t MIC_SAMPLES = 128;        // ~8ms Haeppchen bei 16kHz -> Erkennungs-Aufloesung
    static constexpr uint32_t MIC_SAMPLE_RATE = 16000;
    static constexpr uint32_t ECHO_WINDOW_MULTIPLIER = 3; // weiches Fenster = Sperrfenster * Faktor
    static constexpr int LIST_VISIBLE_ROWS = 10;          // Schuesse pro Seite in der Ergebnisliste

    static constexpr int RECOIL_BUF_SIZE = 12;        // Ringpuffer fuer die Anstiegs-Schaerfe-Messung
    static constexpr int RECOIL_SHARPNESS_LOOKBACK = 4; // Samples vor dem Peak, die fuer "Anstieg" verglichen werden

    static constexpr int LEARN_MAX_SHOTS = 3;        // mehr braucht die Analyse nicht
    static constexpr int LEARN_MAX_SECONDARY = 8;    // Nachschwinger pro Schuss
    static constexpr uint32_t LEARN_WINDOW_MS = 600;  // Aufnahmefenster nach jedem Primaer-Peak
    static constexpr uint32_t LEARN_MIN_GAP_MS = 15;  // Entprellen innerhalb der Aufzeichnung

    struct LearnedShot {
        int16_t primaryPeak = 0;
        float primaryDiscriminator = 0.0f; // Spektral-Verhaeltnis (Mic) bzw. Schaerfe (Rueckstoss)
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

    // Schwelle liegt als Zahl in AppSettings::shotThreshold/recoilThresholdMilliG (im
    // Settings-Menue aenderbar); BtnB/PWR in IDLE bieten weiterhin den schnellen Direkt-
    // Umschalter im jeweils aktiven Feld.
    int listScroll_ = 0; // Seiten-Index der Schussliste in State::STOPPED

    int16_t micBuf_[MIC_SAMPLES] = {0};
    float recoilBuf_[RECOIL_BUF_SIZE] = {0.0f};
    int recoilBufIdx_ = 0;
    bool modeToggleFired_ = false; // B-lang in IDLE = Modus wechseln (einmalig pro Druck)

    uint32_t lastDrawMs_ = 0;
    uint32_t flashUntilMs_ = 0; // kurzer visueller Blitz bei Schusserkennung

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
    int16_t threshold() const;        // Mikrofon-Schwelle
    int16_t recoilThreshold() const;  // Rueckstoss-Schwelle (milli-g)
    float computeSpectralRatio() const;  // Anteil hochfrequenter Energie im aktuellen micBuf_-Haeppchen
    float computeRecoilSharpness() const; // Anstiegs-Schaerfe des aktuellen recoilBuf_-Peaks (0..1)
};
