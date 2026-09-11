#ifndef SUSAMUNE_LM_TIMER_CLOCK_H
#define SUSAMUNE_LM_TIMER_CLOCK_H

/* The native count is game updates at 30 Hz, not its accelerated wall clock. */
#define LM_TIMER_CLOCK_MAX_CENTISECONDS 599999u

static inline unsigned int LmTimerClockCentiseconds(unsigned int high,
                                                    unsigned int low) {
    if (high || low >= 180000u) return LM_TIMER_CLOCK_MAX_CENTISECONDS;
    return (low * 10u + 1u) / 3u;
}

static inline int LmTimerClockReadable(unsigned int scene,
                                      unsigned int loopScene,
                                      unsigned int ownerScene,
                                      unsigned int transition,
                                      unsigned int active) {
    return scene == 2u && loopScene == 2u && ownerScene == 2u &&
           transition == 0u && active <= 1u;
}

static inline int LmTimerClockAdvances(unsigned int active) {
    return active == 1u;
}

/* GLMJ's control owner: 1 START pause, 2 Y map, 3 Z Game Boy Horror. */
static inline int LmTimerClockNativeMenu(unsigned int owner) {
    return owner >= 1u && owner <= 3u;
}

static inline int LmTimerClockMenuRuns(unsigned int owner, int runInNativeMenus,
                                      int modMenuOpen) {
    return !LmTimerClockNativeMenu(owner) || runInNativeMenus || modMenuOpen;
}

static inline int LmTimerClockSupplement(int readable, unsigned int active,
    unsigned int owner, int runInNativeMenus, int modMenuOpen,
    unsigned int beforeHigh, unsigned int beforeLow,
    unsigned int afterHigh, unsigned int afterLow) {
    return readable && LmTimerClockAdvances(active) && LmTimerClockNativeMenu(owner) &&
        LmTimerClockMenuRuns(owner,runInNativeMenus,modMenuOpen) &&
        beforeHigh == afterHigh && beforeLow == afterLow;
}

/* 0 leaves retail alone; 1 mirrors TIMESTOP; 2 mirrors TIMEACTIVE. */
static inline unsigned int LmTimerClockEventAction(unsigned int eventId,
                                                  int scriptReady) {
    if (!scriptReady) return 0u;
    return eventId == 29u ? 1u : eventId == 53u ? 2u : 0u;
}

#endif
