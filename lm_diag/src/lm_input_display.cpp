#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_input_display.hxx"
#include "lm_draw.hxx"
#include "Dolphin/PAD.h"
#include "susamune/lm_xfb_draw.h"

// Moonshine's input_display.cpp geometry/palette, adapted to the completed XFB.
// Original visual proportions follow sup39/BitPatty's controller display.
namespace {
constexpr u32 kColours[] = {
    0xEEEEEEu, 0xFFD300u, 0x2EE5B8u, 0xFF1A1Au,
    0xEEEEEEu, 0xEEEEEEu, 0xDFDFDFu, 0xDFDFDFu,
    0xEEEEEEu, 0x9494FFu, 0xFFFFFFu, 0xEEEEEEu,
};
enum Colour { MainStick, CStick, A, B, X, Y, L, R, Start, Z, Values, TriggerOutline };

struct Painter {
    LmXfbSurface surface;
    int x(int v) const { return (16 + v) * surface.width / 640; }
    int y(int v) const { return (314 + v) * surface.height / 480; }
    void box(int lx, int ly, int w, int h, u32 rgb, u8 alpha) {
        LmXfbBox(&surface, x(lx), y(ly), x(lx + w) - x(lx),
                 y(ly + h) - y(ly), rgb, alpha);
    }
    void regularVertices(int lx, int ly, int radius, s16 *xy, int count, int step) const {
        static const s16 ux[32] = {
            1000, 981, 924, 831, 707, 556, 383, 195,
            0, -195, -383, -556, -707, -831, -924, -981,
            -1000, -981, -924, -831, -707, -556, -383, -195,
            0, 195, 383, 556, 707, 831, 924, 981,
        };
        for (int i = 0; i < count; ++i) {
            const int j = i * step;
            xy[i * 2] = static_cast<s16>(x(lx) + radius * surface.width * ux[j] / 640000);
            xy[i * 2 + 1] = static_cast<s16>(y(ly) + radius * surface.height * ux[(j + 24) & 31] / 480000);
        }
    }
    void fillCircle(int lx, int ly, int radius, Colour slot, u8 alpha) {
        s16 xy[64];
        regularVertices(lx, ly, radius, xy, 32, 1);
        LmXfbFillPoly(&surface, xy, 32, kColours[slot], alpha);
    }
    void strokeCircle(int lx, int ly, int radius, Colour slot, u8 alpha) {
        s16 xy[64];
        regularVertices(lx, ly, radius, xy, 32, 1);
        LmXfbStrokePoly(&surface, xy, 32, kColours[slot], alpha, 3);
    }
    void strokeGate(int lx, int ly, int radius, Colour slot, u8 alpha) {
        s16 xy[16];
        regularVertices(lx, ly, radius, xy, 8, 4);
        LmXfbStrokePoly(&surface, xy, 8, kColours[slot], alpha, 3);
    }
    void button(int lx, int ly, int radius, bool down, Colour slot) {
        if (down) fillCircle(lx, ly, radius, slot, 0xBFu);
        strokeCircle(lx, ly, radius, slot, 0xBFu);
    }
    void dpad(int lx, int ly, bool down) {
        box(lx, ly, 8, 8, kColours[Values], 0xBFu);
        box(lx + 1, ly + 1, 6, 6, down ? 0xFFD300u : 0x182028u, 0xFFu);
    }
};
}

namespace LMInputDisplay {
void draw(void *xfb, u16 width, u16 height, const PADStatus &pad) {
    Painter p = {{static_cast<u8 *>(xfb), width, height, width * 2}};
    if (!LmXfbValid(&p.surface)) return;
    const PADStatus empty = {};
    const PADStatus &raw = pad.mCurError ? empty : pad;
    const u16 buttons = raw.mButton;
    p.box(0, 0, 182, 120, 0u, 0x7Fu);
    const int lFill = buttons & 0x40u ? 64 : LmXfbClamp(raw.mTriggerLeft * 52 / 170, 0, 52);
    const int rFill = buttons & 0x20u ? 64 : LmXfbClamp(raw.mTriggerRight * 52 / 170, 0, 52);
    p.box(12, 10, lFill, 8, kColours[L], 0xBFu);
    p.box(170 - rFill, 10, rFill, 8, kColours[R], 0xBFu);
    s16 trigger[] = {
        static_cast<s16>(p.x(12)), static_cast<s16>(p.y(10)),
        static_cast<s16>(p.x(76)), static_cast<s16>(p.y(10)),
        static_cast<s16>(p.x(76)), static_cast<s16>(p.y(18)),
        static_cast<s16>(p.x(12)), static_cast<s16>(p.y(18)),
    };
    LmXfbStrokePoly(&p.surface, trigger, 4, kColours[TriggerOutline], 0xBFu, 3);
    trigger[0] = trigger[6] = static_cast<s16>(p.x(106));
    trigger[2] = trigger[4] = static_cast<s16>(p.x(170));
    LmXfbStrokePoly(&p.surface, trigger, 4, kColours[TriggerOutline], 0xBFu, 3);

    const int mx = LmXfbClamp(static_cast<s8>(raw.mStickX), -100, 100) * 14 / 100;
    const int my = LmXfbClamp(static_cast<s8>(raw.mStickY), -100, 100) * 14 / 100;
    const int cx = LmXfbClamp(static_cast<s8>(raw.mSubStickX), -100, 100) * 14 / 100;
    const int cy = LmXfbClamp(static_cast<s8>(raw.mSubStickY), -100, 100) * 14 / 100;
    p.fillCircle(32 + mx, 52 - my, 12, MainStick, 0xEFu);
    p.strokeGate(32, 52, 19, MainStick, 0xEFu);
    p.fillCircle(64 + cx, 92 - cy, 12, CStick, 0xEFu);
    p.strokeGate(64, 92, 19, CStick, 0xEFu);
    p.button(138, 66, 18, buttons & 0x100u, A);
    p.button(113, 89, 9, buttons & 0x200u, B);
    p.button(164, 50, 8, buttons & 0x400u, X);
    p.button(119, 41, 8, buttons & 0x800u, Y);
    p.button(144, 34, 6, buttons & 0x10u, Z);
    p.button(91, 64, 5, buttons & 0x1000u, Start);
    p.dpad(12, 91, buttons & 1u);
    p.dpad(28, 91, buttons & 2u);
    p.dpad(20, 99, buttons & 4u);
    p.dpad(20, 83, buttons & 8u);
    p.box(20, 91, 8, 8, kColours[Values], 0xBFu);
    LMDraw::flush(xfb, width, height, 157, 60);
}
}
#endif
