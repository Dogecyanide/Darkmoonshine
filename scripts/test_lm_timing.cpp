#include "../lm_diag/include/lm_timing.hxx"
#include "../include/susamune/lm_state_hotkeys.h"
#include "../include/Dolphin/PAD.h"
#define CHECK(x) if (!(x)) return __LINE__
extern "C" int main() {
    CHECK(LmStateDirectionEdge(1u, 0u, 1u, 1));
    CHECK(LmStateDirectionEdge(0x2101u, 0x2100u, 1u, 1));
    CHECK(LmStateDirectionEdge(0x22u, 0x20u, 2u, 1));
    CHECK(!LmStateDirectionEdge(1u, 0x101u, 1u, 1));
    CHECK(!LmStateDirectionEdge(3u, 0u, 1u, 1));
    CHECK(!LmStateDirectionEdge(5u, 0u, 1u, 1));
    CHECK(!LmStateDirectionEdge(1u, 0u, 1u, 0));
    LmStateHotkeyLatch latch;
    latch.lastButtons = latch.lastConnected = latch.lastLatched = latch.lastDisposition = 0u;
    latch.previous = latch.pending = 0u;
    LmStateSampleHotkeys(&latch, 1u, 1, 1);
    LmStateSampleHotkeys(&latch, 0u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 1u);
    CHECK(latch.lastButtons == 1u && latch.lastConnected == 1u &&
          latch.lastLatched == 1u && latch.lastDisposition == 1u);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 0u);
    LmStateSampleHotkeys(&latch, 4u, 1, 0);
    CHECK(latch.lastButtons == 1u && latch.lastDisposition == 1u);
    LmStateSampleHotkeys(&latch, 0u, 1, 0);
    LmStateSampleHotkeys(&latch, 0x102u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 2u);
    LmStateSampleHotkeys(&latch, 2u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 0u);
    LmStateSampleHotkeys(&latch, 0u, 1, 1);
    LmStateSampleHotkeys(&latch, 1u, 1, 0);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 0u);
    LmStateSampleHotkeys(&latch, 1u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 0u);
    LmStateSampleHotkeys(&latch, 0u, 1, 1);
    LmStateSampleHotkeys(&latch, 2u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 0) == 0u);
    LmStateSampleHotkeys(&latch, 0u, 0, 1);
    CHECK(latch.previous == 0u && latch.pending == 0u);
    LmStateSampleHotkeys(&latch, 0x120u, 1, 0);
    LmStateSampleHotkeys(&latch, 0x121u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 1u);
    LmStateSampleHotkeys(&latch, 0x21u, 1, 1);
    CHECK(LmStateConsumeHotkey(&latch, 1) == 0u);
    LMTiming::Pulse p;
    p.sample(true); CHECK(!p.pressed);
    p.arm(); p.sample(false); p.sample(false); p.sample(true);
    CHECK(p.delay == 2 && p.hold == 1 && !p.finished);
    p.sample(true); p.sample(false);
    CHECK(p.hold == 2 && p.finished && !p.armed);
    p.sample(true); CHECK(p.hold == 2);
    p.arm(); p.sample(true); p.sample(false);
    CHECK(p.delay == 0 && p.hold == 1 && p.finished);
    CHECK(LMTiming::compare(10, 10, 0) == 0);
    CHECK(LMTiming::compare(9, 10, 0) == -1);
    CHECK(LMTiming::compare(11, 10, 0) == 1);
    CHECK(LMTiming::compare(9, 10, 1) == 0);
    CHECK(LMTiming::compare(11, 10, 1) == 0);
    CHECK(LMTiming::compare(0xffffffffu, 0, 1) == 1);
    LMTiming::Lag lag;
    lag.sample(100, 2, true); lag.sample(102, 2, true);
    CHECK(lag.samples == 1 && lag.extra == 0);
    lag.sample(105, 2, true);
    CHECK(lag.samples == 2 && lag.extra == 1 && lag.slow == 1);
    lag.sample(1000, 2, false); lag.sample(1002, 2, true);
    CHECK(lag.samples == 2 && lag.extra == 1);
    lag.sample(1003, 1, true); CHECK(lag.extra == 1 && lag.samples == 3);
    lag.reset(); CHECK(!lag.valid && !lag.samples);
    lag.sample(0xfffffffeu, 2, true); lag.sample(0, 2, true);
    CHECK(lag.samples == 1 && lag.extra == 0);
    lag.sample(10, 0, true); CHECK(lag.samples == 1);
    lag.sample(1000, 2, true); CHECK(lag.samples == 1 && !lag.extra);
    lag.sample(1002, 2, true); CHECK(lag.samples == 2 && !lag.extra);
    CHECK(LMTiming::horizontalSpeed(0, 0) == 0u);
    CHECK(LMTiming::horizontalSpeed(300, -400) == 500u);
    CHECK(LMTiming::horizontalSpeed(-300, 400) == 500u);
    CHECK(LMTiming::horizontalSpeed(1, 1) == 1u);
    CHECK(LMTiming::horizontalSpeed(2000000000, 0) == 2000000000u);
    CHECK(LMTiming::horizontalSpeed(-2147483647 - 1, -2147483647 - 1) == 3037000499u);
    CHECK(!LMTiming::rTriggerDown(0u, 29u));
    CHECK(LMTiming::rTriggerDown(0u, 30u));
    CHECK(LMTiming::rTriggerDown(0x20u, 0u));
    CHECK(!LMTiming::rTriggerDown(0x40u, 0u));
    CHECK(sizeof(PADStatus) == 12u);
    CHECK(__builtin_offsetof(PADStatus, mTriggerLeft) == 6u);
    CHECK(__builtin_offsetof(PADStatus, mTriggerRight) == 7u);
    PADStatus raw;
    for (unsigned i = 0u; i < sizeof(raw); ++i)
        reinterpret_cast<volatile u8 *>(&raw)[i] = 0u;
    LMTiming::Pump leftOnly;
    leftOnly.sample(false, true);
    for (unsigned pressure = 0; pressure <= 255u; ++pressure) {
        raw.mTriggerLeft = static_cast<u8>(pressure);
        raw.mButton = 0u;
        CHECK(!LMTiming::rTriggerDown(raw));
        leftOnly.sample(LMTiming::rTriggerDown(raw), true);
        CHECK(!leftOnly.visible() && !leftOnly.hold && !leftOnly.last);
        raw.mButton = 0x40u;
        CHECK(!LMTiming::rTriggerDown(raw));
        leftOnly.sample(LMTiming::rTriggerDown(raw), true);
        CHECK(!leftOnly.visible() && !leftOnly.hold && !leftOnly.last);
    }
    raw.mButton = 0u;
    raw.mTriggerLeft = 0u;
    for (unsigned pressure = 0; pressure <= 255u; ++pressure) {
        raw.mTriggerRight = static_cast<u8>(pressure);
        CHECK(LMTiming::rTriggerDown(raw) == (pressure >= 30u));
    }
    raw.mTriggerRight = 0u;
    for (unsigned bit = 1u; bit < 0x10000u; bit <<= 1u) {
        raw.mButton = static_cast<u16>(bit);
        CHECK(LMTiming::rTriggerDown(raw) == (bit == 0x20u));
    }
    raw.mButton = 0x20u;
    raw.mTriggerLeft = 255u;
    leftOnly.sample(LMTiming::rTriggerDown(raw), true);
    CHECK(leftOnly.holding && leftOnly.hold == 1u && leftOnly.visible());
    raw.mButton = 0x40u;
    raw.mTriggerLeft = 255u;
    leftOnly.sample(LMTiming::rTriggerDown(raw), true);
    CHECK(leftOnly.last == 1u && leftOnly.popup == LMTiming::Pump::kPopupFrames);
    LMTiming::Pump pump;
    pump.sample(true, true); CHECK(!pump.holding && !pump.finished);
    pump.sample(false, true); CHECK(pump.ready);
    pump.sample(true, true); CHECK(pump.hold == 1u && pump.holding);
    pump.sample(false, true);
    CHECK(pump.last == 1u && pump.finished && !pump.holding && !pump.hold);
    pump.sample(true, true); pump.sample(true, true); pump.sample(true, true);
    CHECK(pump.hold == 3u && pump.last == 1u);
    pump.sample(false, true); CHECK(pump.last == 3u && !pump.hold);
    pump.sample(false, true); CHECK(pump.last == 3u);
    CHECK(pump.popup == LMTiming::Pump::kPopupFrames - 1u && pump.visible());
    for (unsigned i = 1u; i < LMTiming::Pump::kPopupFrames; ++i) {
        CHECK(pump.visible());
        pump.sample(false, true);
    }
    CHECK(!pump.visible() && !pump.popup && pump.last == 3u);
    // L alone cannot reopen a result after it has expired.
    pump.sample(LMTiming::rTriggerDown(raw), true);
    CHECK(!pump.visible() && !pump.hold);
    // Menu, disconnect and timeline discontinuity all cancel the partial hold.
    for (unsigned interruption = 0; interruption < 3u; ++interruption) {
        pump.sample(true, true); pump.sample(true, true);
        pump.sample(true, false);
        CHECK(!pump.holding && !pump.ready && pump.last == 3u);
        CHECK(!pump.visible() && !pump.popup);
        pump.sample(true, true); CHECK(!pump.holding);
        pump.sample(false, true); CHECK(pump.ready && pump.last == 3u);
        pump.sample(true, true); pump.sample(true, true); pump.sample(true, true);
        pump.sample(false, true); CHECK(pump.last == 3u);
    }
    // A bad/disconnected released sample is not a completed pump.
    pump.sample(true, true); pump.sample(false, false);
    CHECK(!pump.holding && !pump.ready && pump.last == 3u);
    pump.sample(false, true); pump.sample(true, true);
    pump.hold = 0xffffffffu; pump.sample(true, true);
    CHECK(pump.hold == 0xffffffffu);
    pump.sample(false, true); CHECK(pump.last == 0xffffffffu);
    CHECK(pump.visible());
    pump.sample(false, false); CHECK(!pump.visible() && !pump.popup);
    pump.reset(); CHECK(!pump.hold && !pump.last && !pump.finished && !pump.ready);
    return 0;
}
