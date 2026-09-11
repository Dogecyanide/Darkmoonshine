#ifndef LM_DRAW_HXX
#define LM_DRAW_HXX
#include "Dolphin/types.h"
namespace LMDraw {
// Boxes/flush use JUT's 320x240 logical coordinates. Polygons use XFB pixels.
void fillBox(void *xfb, u16 width, u16 height, s16 x, s16 y, s16 w, s16 h,
             u32 rgb, u8 alpha = 255u);
void fillPoly(void *xfb, u16 width, u16 height, const s16 *xy, u16 count,
              u32 rgb, u8 alpha = 255u);
void strokePoly(void *xfb, u16 width, u16 height, const s16 *xy, u16 count,
                u32 rgb, u8 alpha = 255u, u8 thickness = 3u);
void flush(void *xfb, u16 width, u16 height, s16 y, s16 h);
}
#endif
