#pragma once

// Gemeinsame Schnittstelle aller Module (ShotTimer, CantLevel, StabilityTracker, Settings).
// onEnter()/onExit() werden beim Wechsel ins bzw. aus dem Modul aufgerufen,
// loop() wird vom main.cpp in jedem Durchlauf aufgerufen solange das Modul aktiv ist.
class AppModule {
public:
    virtual ~AppModule() {}
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void loop() = 0;
    virtual const char* name() const = 0;

    // true, solange das Modul aktiv "etwas tut" (laufender Timer, Live-Messung) -- unterdrueckt
    // das automatische Dimmen/Abdunkeln in main.cpp, auch ohne Tastendruck. Default: nicht busy
    // (z.B. Menue, Idle-Bildschirme duerfen normal dimmen).
    virtual bool isBusy() const { return false; }

    // Bei langem A-Druck zuerst aufgerufen (main.cpp), noch bevor das Modul global verlassen
    // wird. Gibt true zurueck, wenn das Modul den Schritt selbst behandelt hat (z.B. eine
    // eigene Navigationsebene zurueck) -- das Modul bleibt dann aktiv. Gibt false zurueck
    // (Default), wenn es keine eigene Ebene mehr gibt -- dann verlaesst main.cpp das Modul
    // global zurueck ins Hauptmenue.
    virtual bool handleBack() { return false; }
};
