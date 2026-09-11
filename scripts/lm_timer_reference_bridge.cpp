// Frozen pre-optimization implementation: independent byte-parity oracle.
#include "lm_timer_reference.h"
extern "C" {
int _fltused=0;
__declspec(dllexport) void draw_timer(void *pixels, unsigned int cs,
    const LmTimerStyle *style, const LmTimerStreakStyle *streak) {
    LmXfbSurface surface={static_cast<unsigned char *>(pixels),640,480,1280};
    LmTimerDraw(&surface,cs,style,streak);
}
}
