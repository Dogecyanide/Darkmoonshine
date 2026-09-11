#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_tools.hxx"
#include "lm_timing.hxx"
#include "lm_state.hxx"
#include "lm_input_display.hxx"
#include "lm_draw.hxx"
#include "lm_timer.hxx"
#include "lm_colour.hxx"
#include "lm_preferences.hxx"
#include "Dolphin/PAD.h"
#include "susamune/lm_room_id.h"

namespace {
static_assert(sizeof(PADStatus) == 12u &&
              __builtin_offsetof(PADStatus, mTriggerLeft) == 6u &&
              __builtin_offsetof(PADStatus, mTriggerRight) == 7u,
              "Raw PAD trigger channels must match the retail decoder");
using TextFn = void (*)(void *, u16, u16, const char *, ...);
using EraseFn = void (*)(void *, u16, u16, u16, u16);
u32 word(u32 a) { return *reinterpret_cast<volatile u32 *>(a); }
bool mem(u32 a, u32 n) { return a >= 0x80003100u && n <= 0x017FCF00u && a <= 0x81800000u - n; }
u32 player() {
    const u32 owner = word(0x804A17C8u);
    if (word(0x804A0C20u) != 2u || word(0x80398A40u) != 2u ||
        word(0x80398A44u) != 2u || word(0x804A0C28u) || !mem(owner, 0x24u) ||
        !mem(word(owner + 8u), 0xE48u)) return 0u;
    const u32 p = reinterpret_cast<u32>((reinterpret_cast<void *(*)(u32)>(0x800E7E7Cu))(0u));
    return mem(p, 0x1000u) ? p : 0u;
}
u32 sMetadata = 0u;
bool sInputs, sLagVisible, sRPumpVisible;
PADStatus sPad;
LMTiming::Lag sLag;
LMTiming::Pulse sPulse;
LMTiming::Pump sRPump;
u32 sTimeline, sLoad, sPlayer;
s32 sPosition[3], sDelta[3];
u32 sHorizontalSpeed;
u16 sAngle;
s32 sRoom, sHp;
bool sPositionValid;
struct Reference { u32 delay, hold, tolerance, button; bool valid; };
Reference sReferences[2] = {{0u, 1u, 0u, 3u, false}, {0u, 1u, 0u, 1u, false}};
u32 sProfile;
bool sRecord, sArmAfterMenu, sWaitRelease, sWasMenu;
const char *sButtonNames[] = {"R (ANALOG 30+)", "A", "B", "L (ANALOG 30+)", "Z", "X", "Y"};
bool monitoredDown() {
    if (!sProfile) return false;
    const u32 b = sReferences[sProfile - 1u].button;
    const u16 masks[] = {0x20u, 0x100u, 0x200u, 0x40u, 0x10u, 0x400u, 0x800u};
    return (sPad.mButton & masks[b]) ||
           (b == 0u && LMTiming::rTriggerDown(sPad)) ||
           (b == 3u && sPad.mTriggerLeft >= 30u);
}
u32 adjusted(u32 v, s32 d, u32 count) {
    return d < 0 ? (v + count - 1u) % count : (v + 1u) % count;
}
void arm() { sPulse.arm(); sWaitRelease = monitoredDown(); }
const char *mark(u32 row, u32 selected) { return row == selected ? ">" : " "; }
const char *onOff(bool v) { return v ? "ON" : "OFF"; }
}

namespace LMTools {
void writePreferences(unsigned int v[48]) {
    v[0]=sMetadata; v[1]=sInputs; v[2]=sLagVisible; v[3]=sRPumpVisible;
    v[4]=LMColour::enabled(); v[5]=LMColour::rgb(); v[34]=sProfile;
    for(unsigned i=0;i<2;++i) {
        const Reference &r=sReferences[i]; const unsigned at=35+i*5;
        v[at]=r.delay; v[at+1]=r.hold; v[at+2]=r.tolerance; v[at+3]=r.button; v[at+4]=r.valid;
    }
}
void readPreferences(const unsigned int v[48], unsigned int lo, unsigned int hi) {
    if((lo&1u) && v[0]<=2u) sMetadata=v[0];
    if((lo&2u) && v[1]<=1u) sInputs=v[1];
    if((lo&4u) && v[2]<=1u) sLagVisible=v[2];
    if((lo&8u) && v[3]<=1u) sRPumpVisible=v[3];
    const bool enabled=(lo&16u) && v[4]<=1u ? v[4]!=0u : LMColour::enabled();
    if((lo&32u) && v[5]<=0xFFFFFFu) LMColour::setRgb(v[5]);
    LMColour::setEnabled(enabled);
    if((hi&4u) && v[34]<=2u) sProfile=v[34];
    for(unsigned i=0;i<2;++i) {
        Reference &r=sReferences[i]; const unsigned at=35+i*5;
        if(hi&(1u<<(at-32))) r.delay=v[at];
        if((hi&(1u<<(at-31))) && v[at+1]>=1u) r.hold=v[at+1];
        if((hi&(1u<<(at-30))) && v[at+2]<=10u) r.tolerance=v[at+2];
        if((hi&(1u<<(at-29))) && v[at+3]<7u) r.button=v[at+3];
        if((hi&(1u<<(at-28))) && v[at+4]<=1u) r.valid=v[at+4];
        if(((hi&(1u<<(at-31))) && !v[at+1]) ||
           ((hi&(1u<<(at-30))) && v[at+2]>10u) ||
           ((hi&(1u<<(at-29))) && v[at+3]>=7u) ||
           ((hi&(1u<<(at-28))) && v[at+4]>1u)) r.valid=false;
    }
}
void samplePad(const PADStatus &pad) { sPad = pad; }
void tick(bool menuOpen) {
    LMTimer::tick(menuOpen);
    const u32 p = player();
    const u32 revision = LMState::timelineRevision();
    const bool changed = revision != sTimeline;
    sTimeline = revision;
    const bool loaded = sLoad != LMState::loadRevision();
    sLoad = LMState::loadRevision();
    const bool active = p && !menuOpen && sPad.mCurError == 0u;
    sRPump.sample(LMTiming::rTriggerDown(sPad),
                  sRPumpVisible && active && !sWasMenu && !changed &&
                  !loaded && p == sPlayer);
    // Retail display queue's +4 is the requested retrace interval. Count
    // missed VIs separately from slow updates; exclude menu and state I/O.
    // 8000747C/84 uses lis 803A + signed addi 8560 = 80398560.
    sLag.sample(word(0x804A21D8u), word(0x80398564u),
                active && !sWasMenu && !changed && !loaded);
    bool finite = p != 0u;
    for (u32 i = 0; i < 3u; ++i) {
        const u32 address = p + 0x44u + i * 4u;
        if (p && (word(address) & 0x7F800000u) == 0x7F800000u) { finite = false; break; }
        const f32 f = p ? *reinterpret_cast<volatile f32 *>(address) : 0.0f;
        if (!(f >= -10000000.0f && f <= 10000000.0f)) { finite = false; break; }
        const s32 v = static_cast<s32>(f * 100.0f);
        sDelta[i] = sPositionValid && p == sPlayer && active && !sWasMenu &&
                    !changed && !loaded ? v - sPosition[i] : 0;
        sPosition[i] = v;
    }
    sPositionValid = finite;
    if (!finite) sDelta[0] = sDelta[1] = sDelta[2] = 0;
    sHorizontalSpeed = LMTiming::horizontalSpeed(sDelta[0], sDelta[2]);
    sPlayer = p;
    if (finite) {
        sAngle = *reinterpret_cast<volatile u16 *>(p + 0x88u);
        sRoom = static_cast<s32>(LmPlayerRoomId(word(p + 0xB4u)));
        sHp = *reinterpret_cast<volatile s16 *>(p + 0xFCu);
    }
    if (menuOpen) {
        sPulse.armed = false;
    } else if (sProfile && (loaded || (sWasMenu && sArmAfterMenu))) {
        arm();
        sArmAfterMenu = false;
    } else if (active && sProfile) {
        if (sWaitRelease) { if (!monitoredDown()) sWaitRelease = false; }
        else {
            const bool wasFinished = sPulse.finished;
            sPulse.sample(monitoredDown());
            if (!wasFinished && sPulse.finished) {
                Reference &ref = sReferences[sProfile - 1u];
                if (sRecord) {
                    ref.delay = sPulse.delay; ref.hold = sPulse.hold; ref.valid = true;
                    sRecord = false;
                    LMPreferences::requestSave();
                }
            }
        }
    }
    if (!p || sPad.mCurError) { sPulse.armed = false; sPositionValid = false; }
    sWasMenu = menuOpen;
}
void draw(void *dp, void *xfb) {
    LMTimer::draw(dp, xfb);
    if (sInputs) LMInputDisplay::draw(xfb, 640u, 480u, sPad);
    auto text = reinterpret_cast<TextFn>(0x801D49F8u);
    if (sInputs) {
        // Raw SDK sample, before PADClamp/menu filtering. Keep the two
        // trigger channels visible for controller/protocol verification.
        LMDraw::fillBox(xfb, 640u, 480u, 8, 219, 92, 9, 0x111D24u, 0xD0u);
        if (!sPad.mCurError)
            text(dp, 10u, 220u, "L%03u R%03u %04X", sPad.mTriggerLeft,
                 sPad.mTriggerRight, sPad.mButton);
        else text(dp, 10u, 220u, "PAD disconnected");
        LMDraw::flush(xfb, 640u, 480u, 219, 9);
    }
    if (sRPumpVisible && sRPump.visible()) {
        LMDraw::fillBox(xfb, 640u, 480u, 220, 109, 94, 21, 0x111D24u, 0xD0u);
        LMDraw::fillBox(xfb, 640u, 480u, 220, 109, 2, 21, 0x57DCA7u);
        const unsigned frames = sRPump.holding ? sRPump.hold : sRPump.last;
        const bool capped = frames > 99999u;
        text(dp, 226u, 112u, "R-pump");
        text(dp, 226u, 120u, sRPump.holding ? "HOLD %u%s f" : "%u%s frames",
             capped ? 99999u : frames, capped ? "+" : "");
        LMDraw::flush(xfb, 640u, 480u, 109, 21);
    }
    u32 lines = (sMetadata ? 6u : 0u) + (sLagVisible ? 2u : 0u) +
                (sProfile ? 4u : 0u);
    if (!lines) return;
    const u16 top = static_cast<u16>(228u - lines * 7u);
    const u16 left = sInputs ? 105u : 10u;
    reinterpret_cast<EraseFn>(0x801D4294u)(dp, left - 2u, top,
                                        314u - left, static_cast<u16>(lines * 7u));
    u16 y = top;
    if (sMetadata) {
        if (sPositionValid) {
            for (u32 i = 0u; i < 3u; ++i) {
                if (sMetadata == 2u)
                    text(dp, left, y + i * 7u, "%c %ld d%ld x100", 'X' + i, sPosition[i], sDelta[i]);
                else text(dp, left, y + i * 7u, "%c %ld x100", 'X' + i, sPosition[i]);
            }
            text(dp, left, y + 21u, "Angle %04X  Map %lu", sAngle, word(0x804A0C48u));
            text(dp, left, y + 28u, "Room %ld  HP %ld", sRoom, sHp);
            text(dp, left, y + 35u, "Speed %lu.%02lu u/update (XZ)",
                 sHorizontalSpeed / 100u, sHorizontalSpeed % 100u);
        } else text(dp, left, y, "Metadata: no active player");
        y += 42u;
    }
    if (sLagVisible) {
        text(dp, left, y, "Lag VI %lu  Slow %lu", sLag.extra, sLag.slow);
        text(dp, left, y + 7u, "Updates %lu", sLag.samples);
        y += 14u;
    }
    if (sProfile) {
        const Reference &ref = sReferences[sProfile - 1u];
        text(dp, left, y, "%s / timing reference", sProfile == 1u ? "Pearl" : "Chauncey");
        text(dp, left, y + 7u, "Press %lu / Hold %lu", sPulse.delay, sPulse.hold);
        if (sPulse.finished && ref.valid && !sRecord) {
            const s32 d = LMTiming::compare(sPulse.delay, ref.delay, ref.tolerance);
            const s32 h = LMTiming::compare(sPulse.hold, ref.hold, ref.tolerance);
            text(dp, left, y + 14u, "%s / %s",
                 d < 0 ? "EARLY" : d > 0 ? "LATE" : "ON TIME",
                 h < 0 ? "SHORT" : h > 0 ? "LONG" : "HOLD OK");
        } else text(dp, left, y + 14u, "%s", sPulse.armed ? "Waiting for press / release" :
                   sRecord ? "Recording reference" : ref.valid ? "Reference ready" : "No reference recorded");
        text(dp, left, y + 21u, "REFERENCE, NOT TRICK RESULT");
    }
}
u32 rows(bool timing) { return timing ? 7u : 6u; }
bool action(bool timing, u32 row, s32 direction) {
    if (!timing) {
        if (row == 0u) sMetadata = adjusted(sMetadata, direction, 3u);
        if (row == 1u) sInputs = !sInputs;
        if (row == 2u) sLagVisible = !sLagVisible;
        if (row == 3u && !direction) sLag.reset();
        if (row == 4u) { sRPumpVisible = !sRPumpVisible; sRPump.reset(); }
        if (row == 5u && !direction) LMTimer::openMenu(sPad);
        return false;
    }
    if (!row) {
        sProfile = adjusted(sProfile, direction, 3u);
        sRecord = sArmAfterMenu = false; sPulse = {};
    } else if (sProfile) {
        Reference &ref = sReferences[sProfile - 1u];
        if (row == 1u) ref.button = adjusted(ref.button, direction, 7u);
        if ((row == 2u || row == 3u) && !direction) {
            sRecord = row == 2u; sArmAfterMenu = true; return true;
        }
        if (row == 4u) { ref.delay = adjusted(ref.delay, direction, 601u); ref.valid = true; }
        if (row == 5u) { ref.hold = 1u + adjusted(ref.hold - 1u, direction, 120u); ref.valid = true; }
        if (row == 6u) ref.tolerance = adjusted(ref.tolerance, direction, 11u);
        if (row == 1u) { ref.valid = false; sPulse = {}; }
    }
    return false;
}
void drawPage(void *dp, bool timing, u32 selected, u16 top) {
    auto text = reinterpret_cast<TextFn>(0x801D49F8u);
    if (!timing) {
        text(dp, 10u, top, "%s Metadata        %s", mark(0u, selected), sMetadata == 0u ? "OFF" : sMetadata == 1u ? "POSITION" : "POSITION + DELTA");
        text(dp, 10u, top + 11u, "%s Input display   %s", mark(1u, selected), onOff(sInputs));
        text(dp, 10u, top + 22u, "%s Lag counter     %s", mark(2u, selected), onOff(sLagVisible));
        text(dp, 10u, top + 33u, "%s Reset lag counters", mark(3u, selected));
        text(dp, 10u, top + 44u, "%s R-pump display  %s", mark(4u, selected), onOff(sRPumpVisible));
        text(dp, 10u, top + 55u, "%s Sunshine timer...", mark(5u, selected));
        if (selected == 4u) {
            text(dp, 10u, top + 73u, "R = digital click OR raw analog 30+");
            text(dp, 10u, top + 84u, "Held game frames; release not counted");
            text(dp, 10u, top + 95u, "Right popup; result lasts 90 updates");
        } else {
            text(dp, 10u, top + 84u, "Lag = extra VI retraces / slow updates");
            text(dp, 10u, top + 95u, "Menus and state I/O are excluded");
        }
    } else {
        const Reference &ref = sReferences[sProfile == 2u ? 1u : 0u];
        text(dp, 10u, top, "%s Profile    %s", mark(0u, selected), !sProfile ? "OFF" : sProfile == 1u ? "PEARL REFERENCE" : "CHAUNCEY REFERENCE");
        text(dp, 10u, top + 11u, "%s Button     %s", mark(1u, selected), sButtonNames[ref.button]);
        text(dp, 10u, top + 22u, "%s Record next press/release", mark(2u, selected));
        text(dp, 10u, top + 33u, "%s Arm timing comparison", mark(3u, selected));
        text(dp, 10u, top + 44u, "%s Target delay  %lu frames", mark(4u, selected), ref.delay);
        text(dp, 10u, top + 55u, "%s Target hold   %lu frames", mark(5u, selected), ref.hold);
        text(dp, 10u, top + 66u, "%s Tolerance     %lu frames", mark(6u, selected), ref.tolerance);
        text(dp, 10u, top + 84u, "Loading a state re-arms the comparison.");
        text(dp, 10u, top + 95u, "Timing reference, not a trick-hit detector.");
    }
}
}
#endif
