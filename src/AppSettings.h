#pragma once

#include <cstdint>

// Zentrale, in NVS persistierte Konfiguration. Werte liegen hier bereits aufgeloest
// (z.B. Millisekunden, Helligkeit 0-255), nicht als Indizes -- das Settings-Modul kennt
// die diskreten Options-Listen und schreibt direkt den gewaehlten Wert hierher.
namespace AppSettings {

constexpr uint32_t kTimeoutNever = 0xFFFFFFFFu;

extern uint8_t brightnessActive;   // 0-255
extern uint8_t brightnessDim;      // 0-255
extern uint32_t dimTimeoutMs;      // kTimeoutNever = nie dimmen
extern uint32_t sleepTimeoutMs;    // kTimeoutNever = nie ausschalten
extern bool screenFlipped;         // false = Rotation 0, true = Rotation 2 (180 Grad)
extern int accentColorIndex;       // Index in kAccentPalette (siehe AppSettings.cpp)

extern int16_t shotThreshold;      // Mikrofon-Peak-Schwelle (0..32767) -- direkt als Zahl editierbar
extern float shotDelayMinS;
extern float shotDelayMaxS;
extern uint8_t buzzerVolume;       // 0-255, 0 = stumm
extern uint32_t shotEchoLockoutMs; // Mindestabstand zwischen zwei erkannten Schuessen
extern float shotEchoRatio;        // Mindest-Lautstaerke (relativ zum letzten Schuss) im weichen Echo-Fenster
extern bool shotSpectralEnabled;   // Frequenzanalyse nutzen, um Klicks/Tastendruecke von Schuessen zu unterscheiden
extern float shotSpectralRatio;    // Mindest-Anteil hochfrequenter Energie, um als "schussartig" zu gelten

extern int shotDetectMode;            // 0 = Mikrofon, 1 = Rueckstoss (IMU-Beschleunigungsspitze)
extern int16_t recoilThresholdMilliG; // Peak-Ausschlag der Gesamtbeschleunigung ueber 1g, in milli-g
extern uint32_t recoilLockoutMs;      // Mindestabstand zwischen zwei erkannten Schuessen (Nachschwingen unterdruecken)
extern float recoilRatio;             // Mindest-Ausschlag (relativ zum letzten Schuss) im weichen Nachschwing-Fenster
extern bool recoilSharpnessEnabled;   // Anstiegsgeschwindigkeit pruefen, um Stoesse/Wackeln von echtem Rueckstoss zu unterscheiden
extern float recoilSharpness;         // Mindest-Schaerfe (0..1) des Anstiegs, um als Rueckstoss zu gelten

extern float cantGreenDeg;
extern float cantYellowDeg;
extern float cantCalibCountdownS; // Sekunden Countdown nach "Bestaetigen" vor der Referenz-Messung (0 = aus)

extern float stabilityGreenMoa;
extern float stabilityYellowMoa;
extern float stabilityGraphMaxMoa;
extern float stabilityDeadzoneMoa; // unter dieser Schwelle gilt die Anzeige als "0" (Ruhezustand)

extern uint16_t cpuFreqMhz;        // CPU-Taktrate (80/160/240) -- niedriger = weniger Verbrauch bei gleicher APB-Taktung
extern uint16_t imuPollDelayMs;    // Pause je Durchlauf in den IMU-Live-Loops, damit die CPU zwischendurch idlen kann
extern uint16_t battReadIntervalMs; // Mindestabstand zwischen zwei Akkustand-Abfragen am PMIC (0 = bei jedem Redraw)

constexpr int kAccentPaletteCount = 8;
uint16_t accentColor(int index);

void load();           // aus NVS laden (Default falls nicht vorhanden)
void save();           // aktuelle Werte in NVS schreiben
void resetDefaults();  // RAM-Werte auf Standard zuruecksetzen (noch nicht gespeichert)
void applyDisplay();   // Helligkeit/Rotation/Akzentfarbe sofort wirksam machen
void applyCpuFreq();   // CPU-Taktrate sofort wirksam machen

} // namespace AppSettings
