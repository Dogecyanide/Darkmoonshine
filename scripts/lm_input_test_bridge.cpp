#define SUSAMUNE_VERSION_LMJ 1
#include "lm_draw.hxx"
#include "Dolphin/PAD.h"
namespace LMDraw { void flush(void *, u16, u16, s16, s16) {} }
#include "../lm_diag/src/lm_input_display.cpp"

extern "C" {
__declspec(dllexport) void draw_input(void *pixels, unsigned int buttons,
    int mx, int my, int cx, int cy, unsigned int left, unsigned int right,
    unsigned int error) {
    PADStatus pad = {};
    pad.mButton = static_cast<u16>(buttons);
    pad.mStickX = static_cast<u8>(mx); pad.mStickY = static_cast<u8>(my);
    pad.mSubStickX = static_cast<u8>(cx); pad.mSubStickY = static_cast<u8>(cy);
    pad.mTriggerLeft = static_cast<u8>(left); pad.mTriggerRight = static_cast<u8>(right);
    pad.mCurError = static_cast<u8>(error);
    LMInputDisplay::draw(pixels, 640, 480, pad);
}
__declspec(dllexport) void box(void *pixels, int x, int y, int w, int h,
    unsigned int rgb, unsigned int alpha) {
    LmXfbSurface surface = {static_cast<unsigned char *>(pixels), 640, 480, 1280};
    LmXfbBox(&surface, x, y, w, h, rgb, alpha);
}
__declspec(dllexport) void polygon(void *pixels, const short *points, int count, int stroke) {
    LmXfbSurface surface = {static_cast<unsigned char *>(pixels), 640, 480, 1280};
    if (stroke) LmXfbStrokePoly(&surface, points, count, 0xFF00FF, 191, 3);
    else LmXfbFillPoly(&surface, points, count, 0xFF00FF, 191);
}
}
