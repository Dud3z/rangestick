#pragma once

#include <cstdint>

// Reine Berechnungs-/Kalibrierlogik fuer den Cant-(Roll-)Winkel, ohne UI. Wird sowohl vom
// eigenstaendigen Anti-Cant-Modul als auch vom Kombi-Modus (ComboView) verwendet -- ueber eine
// gemeinsame globale Instanz (siehe main.cpp), damit eine einmal durchgefuehrte Kalibrierung in
// beiden Ansichten gilt, statt sie doppelt verlangen zu muessen.
//
// Die Montage-Orientierung des Sticks am Gewehr ist beliebig -- welche IMU-Achse dabei der
// Rohrachse entspricht, ist also nicht im Voraus bekannt. Eine einzelne statische Kalibrier-
// Messung reicht dafuer NICHT zuverlaessig (siehe CantCalculator.cpp fuer Details). Stattdessen
// macht der Nutzer eine kurze Kipp-Bewegung (Cant simulieren); die Achse mit dem groessten
// Gyroskop-Ausschlag dabei IST die Rohrachse, unabhaengig von der Montage-Lage.
//
// Schritt 1 (LEVEL_REF) verlangt "Waffe waagerecht halten UND Taste druecken" gleichzeitig --
// das ist mit einer Hand schwer zuverlaessig hinzubekommen. Nach dem Tastendruck laeuft deshalb
// erst ein konfigurierbarer Countdown (AppSettings::cantCalibCountdownS, 0 = aus) und die
// eigentliche Referenz-Messung wird erst an dessen Ende genommen -- der Nutzer kann also druecken
// und danach in Ruhe noch still halten, statt exakt im selben Moment beides tun zu muessen.
class CantCalculator {
public:
    enum class State { LEVEL_REF, LEVEL_COUNTDOWN, ROCK_GESTURE, READY };

    // Mit einer bereits geholten IMU-Messung aufrufen (genau einmal pro Frame von der jeweiligen
    // AppModule::loop() gelesen) -- die Klasse liest den Sensor NICHT selbst. Dadurch lesen
    // mehrere Verbraucher (Anti-Cant + Stability im Kombi-Modus) niemals zweimal pro Frame vom
    // selben Sensor, was sich sonst gegenseitig die "neue Daten"-Markierung wegschnappen kann.
    void update(float ax, float ay, float az, float gx, float gy, float gz);
    void confirm();       // Taste "Bestaetigen": naechsten Kalibrier-Schritt bzw. neu kalibrieren
    void recalibrate();   // direkt zurueck auf Schritt 1

    State state() const { return state_; }
    float angleDeg() const { return angleDeg_; }
    // Sekunden bis zum Ende des Countdowns, aufgerundet (3,2,1); 0 wenn nicht im Countdown.
    int countdownSecondsLeft() const;

private:
    static constexpr float ALPHA = 0.2f; // Glaettung (Exponential Moving Average) auf die rohen Accel-Werte

    State state_ = State::LEVEL_REF;
    bool initialized_ = false;
    float fx_ = 0.0f, fy_ = 0.0f, fz_ = 0.0f; // gefilterte Accel-Werte

    float refGravity_[3] = {0.0f, 0.0f, 0.0f}; // Schwerkraft bei "Waffe waagerecht" (Schritt 1)
    float gyroAccum_[3] = {0.0f, 0.0f, 0.0f};  // aufsummierter |Gyro| pro Achse waehrend der Kipp-Bewegung

    int boreAxis_ = 0;         // 0=x,1=y,2=z -- per Kipp-Bewegung erkannte Rohrachse
    float refAngleDeg_ = 0.0f; // Rollwinkel bei Kalibrierung (= neuer Nullpunkt)
    float angleDeg_ = 0.0f;

    uint32_t countdownStartMs_ = 0;
    uint32_t countdownDurationMs_ = 0;

    float rollDegFor(const float v[3]) const;
    void captureLevelRef(); // Schwerkraft jetzt als Referenz uebernehmen, weiter zu ROCK_GESTURE
};
