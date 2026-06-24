#pragma once

#include <cstdint>

// Reine Berechnungslogik fuer das "Wackeln" der Waffe, ohne UI. Wird sowohl vom eigenstaendigen
// Stability-Modul als auch vom Kombi-Modus (ComboView) verwendet -- ueber eine gemeinsame globale
// Instanz (siehe main.cpp), damit Session/Peak in beiden Ansichten konsistent sind.
//
// Statt fester Roll/Pitch-Achsen (die von der Montage-Orientierung des Sticks abhaengen wuerden)
// wird die normalisierte (und tiefpassgefilterte) Schwerkraft-Richtung selbst getrackt: fuer jedes
// Sample wird der Winkel zur gemittelten Richtung des Rolling-Windows bestimmt (asin des
// Kreuzprodukt-Betrags, numerisch stabil fuer kleine Winkel), daraus RMS gebildet und in MOA
// umgerechnet (1 Grad = 60 MOA).
class StabilityCalculator {
public:
    static constexpr int WINDOW = 50;        // ~1s bei 50Hz Sample-Rate
    static constexpr int HISTORY = 60;       // Verlaufsgrafik, ein Eintrag alle ~70ms (~4s Fenster)
    static constexpr int SCATTER_COUNT = 24; // Schussgruppen-Streudiagramm, ein Punkt pro Sample

    // Mit einer bereits geholten IMU-Messung aufrufen (genau einmal pro Frame von der jeweiligen
    // AppModule::loop() gelesen) -- die Klasse liest den Sensor NICHT selbst (intern weiterhin
    // auf SAMPLE_INTERVAL_MS gedrosselt). Dadurch lesen mehrere Verbraucher (Anti-Cant + Stability
    // im Kombi-Modus) niemals zweimal pro Frame vom selben Sensor, was sich sonst gegenseitig die
    // "neue Daten"-Markierung wegschnappen kann -- mit dem Effekt, dass Stability nur noch
    // sporadisch echte Samples bekommt und nie zur Ruhe (nahe 0) kommt.
    void update(float ax, float ay, float az);
    void resetSession();  // Fenster/Peak/Verlauf zuruecksetzen (z.B. neuer Anschlag)

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
    static constexpr float ACCEL_FILTER_ALPHA = 0.15f; // Tiefpass gegen Sensorrauschen

    void sample(float ax, float ay, float az);

    bool filterInit_ = false;
    float fax_ = 0.0f, fay_ = 0.0f, faz_ = 0.0f; // tiefpassgefilterte Accel-Werte

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
