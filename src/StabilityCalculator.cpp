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
    // Tiefpassfilter ebenfalls neu seeden: sonst mischt sich beim Wiedereinstieg in ein
    // Stability-Modul die ALTE gefilterte Ausrichtung (von vor dem Verlassen) mit der NEUEN
    // Lage in einer einzigen EMA-Spur -- wenn das Geraet inzwischen bewegt wurde, erzeugt das
    // einen kurzen aber massiven Schein-Ausschlag, obwohl es jetzt eigentlich still liegt.
    filterInit_ = false;
}

void StabilityCalculator::sample(float ax, float ay, float az) {
    // Tiefpass auf die Rohwerte: das BMI270-Rauschen allein entspricht schon ca. 0.1-0.15 Grad
    // Winkel-Jitter, was durch die x60-MOA-Umrechnung wie mehrere MOA "Wackeln" aussieht --
    // auch im absoluten Stillstand. Ohne Filterung wuerde reines Sensorrauschen gemessen,
    // nicht die tatsaechliche Bewegung der Waffe.
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
    if (norm < 0.01f) return; // ungueltige Messung (freier Fall o.ae.), verwerfen

    int latest = bufIndex_;
    axBuf_[bufIndex_] = fax_ / norm;
    ayBuf_[bufIndex_] = fay_ / norm;
    azBuf_[bufIndex_] = faz_ / norm;
    bufIndex_ = (bufIndex_ + 1) % WINDOW;
    if (bufCount_ < WINDOW) bufCount_++;

    if (bufCount_ < 4) return; // braucht ein paar Samples bevor Streuung sinnvoll ist

    float mx = 0, my = 0, mz = 0;
    for (int i = 0; i < bufCount_; ++i) { mx += axBuf_[i]; my += ayBuf_[i]; mz += azBuf_[i]; }
    float mlen = sqrtf(mx * mx + my * my + mz * mz);
    if (mlen < 0.01f) return;
    mx /= mlen; my /= mlen; mz /= mlen; // gemittelte Ausrichtung des Fensters, normalisiert

    // Abweichung ueber das Kreuzprodukt (asin) statt Skalarprodukt (acos): bei den hier
    // erwarteten kleinen Winkeln liegt der Skalarprodukt-Wert sehr nahe an 1.0, wo acos()
    // numerisch instabil ist (Ableitung divergiert) -- winziges Rundungsrauschen wurde so
    // zu einem kuenstlichen Sockel von mehreren Grad "Wackeln" aufgeblasen, selbst im
    // absoluten Stillstand. asin(|Kreuzprodukt|) ist fuer kleine Winkel gutmuetig.
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
    float wobbleDeg = sqrtf(sumSq / bufCount_); // RMS-Abweichung von der mittleren Ausrichtung

    wobbleMoa_ = wobbleDeg * 60.0f; // 1 Grad = 60 MOA (Winkel-Einheit, distanzunabhaengig)
    if (wobbleMoa_ < AppSettings::stabilityDeadzoneMoa) wobbleMoa_ = 0.0f; // Restrauschen im Ruhezustand ausblenden
    if (wobbleMoa_ > peakMoa_) peakMoa_ = wobbleMoa_;

    // Schussgruppen-Streudiagramm: tangentiale Abweichung des neuesten Samples auf eine lokale
    // 2D-Ebene (u/v) senkrecht zur gemittelten Richtung projizieren -- ergibt eine "Schussgruppe"
    // unabhaengig von der Montage-Orientierung des Sticks.
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
