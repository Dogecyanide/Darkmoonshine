#ifndef LM_TIMER_CLOCK_HXX
#define LM_TIMER_CLOCK_HXX

#include "Dolphin/types.h"

namespace LMTimerClock {
void tick();
bool available();
bool running();
u32 centiseconds();
bool runInNativeMenus();
void setRunInNativeMenus(bool enabled);
}

extern "C" void diagnosticTimerEventStart(void *archive);
extern "C" u32 diagnosticTimerGameUpdate();

#endif
