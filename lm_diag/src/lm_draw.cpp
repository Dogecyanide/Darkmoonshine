#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_draw.hxx"
#include "susamune/lm_xfb_draw.h"
namespace {
LmXfbSurface surface(void *xfb, u16 width, u16 height) {
    return {static_cast<u8 *>(xfb), width, height, width * 2};
}
}
namespace LMDraw {
void fillBox(void *xfb, u16 width, u16 height, s16 x, s16 y, s16 w, s16 h,
             u32 rgb, u8 alpha) {
    LmXfbSurface s = surface(xfb, width, height);
    const int left = x * width / 320, top = y * height / 240;
    LmXfbBox(&s, left, top, (x + w) * width / 320 - left,
             (y + h) * height / 240 - top, rgb, alpha);
}
void fillPoly(void *xfb, u16 width, u16 height, const s16 *xy, u16 count,
              u32 rgb, u8 alpha) {
    LmXfbSurface s = surface(xfb, width, height);
    LmXfbFillPoly(&s, xy, count, rgb, alpha);
}
void strokePoly(void *xfb, u16 width, u16 height, const s16 *xy, u16 count,
                u32 rgb, u8 alpha, u8 thickness) {
    LmXfbSurface s = surface(xfb, width, height);
    LmXfbStrokePoly(&s, xy, count, rgb, alpha, thickness);
}
void flush(void *xfb, u16 width, u16 height, s16 y, s16 h) {
    if (!xfb || !width || width > 1024u || (width & 15u) || height > 1024u || h <= 0) return;
    const int top = LmXfbClamp(y * height / 240, 0, height);
    const int bottom = LmXfbClamp((y + h) * height / 240, 0, height);
    if (top >= bottom) return;
    using CacheFn = void (*)(void *, u32);
    reinterpret_cast<CacheFn>(0x801D5E24u)(static_cast<u8 *>(xfb) + top * width * 2u,
                                         (bottom - top) * width * 2u);
}
}
#endif
