#pragma once

// Shared interface for all modules (ShotTimer, CantLevel, StabilityTracker, Settings).
// onEnter()/onExit() are called when switching into/out of the module,
// loop() is called by main.cpp on every pass while the module is active.
class AppModule {
public:
    virtual ~AppModule() {}
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void loop() = 0;
    virtual const char* name() const = 0;

    // True while the module is actively "doing something" (running timer, live measurement) --
    // suppresses the automatic dim/sleep in main.cpp, even without a button press. Default: not
    // busy (e.g. menu, idle screens are allowed to dim normally).
    virtual bool isBusy() const { return false; }

    // Called first on a long A press (main.cpp), before the module is exited globally. Returns
    // true if the module handled the step itself (e.g. its own navigation level back) -- the
    // module then stays active. Returns false (default) if there is no own level left -- main.cpp
    // then exits the module globally back to the main menu.
    virtual bool handleBack() { return false; }
};
