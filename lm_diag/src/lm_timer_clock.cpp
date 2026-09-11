#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_timer_clock.hxx"
#include "lm_practice.hxx"
#include "susamune/lm_timer_clock.h"

namespace {
constexpr u32 kNativeActive = 0x804993C0u;
constexpr u32 kNativeFramesHigh = 0x804A1288u;
constexpr u32 kNativeFramesLow = 0x804A128Cu;
constexpr u32 kNativeMenuOwner = 0x804A0C44u;
bool sAvailable, sRunning, sRunInNativeMenus;
u32 sCentiseconds;

u32 word(u32 address) {
    return *reinterpret_cast<volatile const u32 *>(address);
}
bool readable(u32 active) {
    return LmTimerClockReadable(word(0x804A0C20u), word(0x80398A40u),
                               word(0x80398A44u), word(0x804A0C28u), active);
}
}

namespace LMTimerClock {
void tick() {
    const u32 active = word(kNativeActive);
    sAvailable = readable(active);
    sRunning = sAvailable && LmTimerClockAdvances(active) &&
        LmTimerClockMenuRuns(word(kNativeMenuOwner),sRunInNativeMenus,LMPractice::isOpen());
    sCentiseconds = sAvailable ? LmTimerClockCentiseconds(
        word(kNativeFramesHigh), word(kNativeFramesLow)) : 0u;
}
bool available() { return sAvailable; }
bool running() { return sRunning; }
u32 centiseconds() { return sCentiseconds; }
bool runInNativeMenus() { return sRunInNativeMenus; }
void setRunInNativeMenus(bool enabled) { sRunInNativeMenus = enabled; }
}

// The entry hook replaced only mflr r0. Rejoin the untouched retail body with
// its caller's LR and ABI intact; CTR/r12 are caller-saved and there are no args.
extern "C" __attribute__((naked)) u32 diagnosticTimerRetailGameUpdate() {
    asm volatile("mflr 0\n\tlis 12, -32768\n\tori 12, 12, 0xb91c\n\tmtctr 12\n\tbctr");
}

extern "C" u32 diagnosticTimerGameUpdate() {
    const u32 beforeHigh=word(kNativeFramesHigh), beforeLow=word(kNativeFramesLow);
    const u32 result=diagnosticTimerRetailGameUpdate();
    const u32 active=word(kNativeActive);
    // Retail skips its clock in these UI branches. Never borrow TIMEACTIVE,
    // duplicate a retail tick, or infer a menu from the shared XFB mode flag.
    if(LmTimerClockSupplement(readable(active),active,word(kNativeMenuOwner),
        sRunInNativeMenus,LMPractice::isOpen(),beforeHigh,beforeLow,
        word(kNativeFramesHigh),word(kNativeFramesLow)))
        reinterpret_cast<void (*)()>(0x80061A48u)();
    return result;
}

extern "C" void diagnosticTimerEventStart(void *archive) {
    reinterpret_cast<void (*)(void *)>(0x80065158u)(archive);
    // The loader publishes the matching ID and cursor only after finding text.
    const u32 eventId = *reinterpret_cast<volatile const u16 *>(0x803C7CA0u);
    const u32 action = LmTimerClockEventAction(eventId, word(0x803C7CACu) != 0u);
    if (action) *reinterpret_cast<volatile u32 *>(kNativeActive) = action - 1u;
}
#endif
