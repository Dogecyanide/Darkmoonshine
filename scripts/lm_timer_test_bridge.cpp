static unsigned int sWork[7];
#define LM_TIMER_WORK(kind, amount) (sWork[kind] += (amount))
#include "susamune/lm_timer_render.h"
static LmTimerCache sCache;
extern "C" {
int _fltused=0;
__declspec(dllexport) void draw_timer(void *pixels, unsigned int cs,
    const LmTimerStyle *style, const LmTimerStreakStyle *streak) {
    LmXfbSurface surface={static_cast<unsigned char *>(pixels),640,480,1280};
    LmTimerDraw(&surface,cs,style,streak);
}
__declspec(dllexport) void draw_timer_cached(void *pixels, unsigned int cs,
    const LmTimerStyle *style, const LmTimerStreakStyle *streak) {
    LmXfbSurface surface={static_cast<unsigned char *>(pixels),640,480,1280};
    LmTimerDraw(&surface,cs,style,streak,&sCache);
}
__declspec(dllexport) void timer_reset_cache() {
    for(unsigned i=0;i<10;++i) sCache.panes[i].valid=0;
}
__declspec(dllexport) void timer_reset_work() { for(unsigned i=0;i<7;++i) sWork[i]=0; }
__declspec(dllexport) unsigned int timer_work(unsigned int field) { return field<7 ? sWork[field] : 0; }
__declspec(dllexport) unsigned int timer_cache_size() { return sizeof(sCache); }
__declspec(dllexport) unsigned int timer_cache_status(unsigned int slot) { return slot<10 ? sCache.panes[slot].valid : 0; }
__declspec(dllexport) unsigned int timer_cache_used(unsigned int slot) {
    if(slot>=10 || sCache.panes[slot].valid!=1) return 0;
    const LmTimerCachedPane *p=&sCache.panes[slot];
    const unsigned char *data=LmTimerCacheData(&sCache,slot);
    unsigned int at=0;
    for(int y=LmXfbClamp(p->quad.top,0,p->height);y<LmXfbClamp(p->quad.bottom,0,p->height);++y)
        at+=4+LmTimerGet16(data+at+2)*8;
    return at;
}
__declspec(dllexport) unsigned int timer_lerp(unsigned int a,unsigned int b,int f) { return LmTimerLerp(a,b,f); }
__declspec(dllexport) void draw_timer_decorated(void *pixels,const LmTimerStyle *style,
    const LmTimerStreakStyle *streak,int padding,int streakPadding,int target,int *flush) {
    LmXfbSurface surface={static_cast<unsigned char *>(pixels),640,480,1280};
    int bounds[4]; LmTimerBounds(style,streak,bounds);
    LmTimerDirtyRows dirty={bounds[1],bounds[3]};
    if(padding>=0) {
        LmXfbBox(&surface,bounds[0]-padding,bounds[1]-padding,
            bounds[2]-bounds[0]+padding*2,bounds[3]-bounds[1]+padding*2,0xFF6600,133);
        LmTimerIncludeRows(&dirty,bounds[1]-padding,bounds[3]+padding);
    }
    LmTimerStreakBounds(style,streak,bounds);
    if(streakPadding>=0) {
        LmXfbBox(&surface,bounds[0]-streakPadding,bounds[1]-streakPadding,
            bounds[2]-bounds[0]+streakPadding*2,bounds[3]-bounds[1]+streakPadding*2,0x440044,128);
        LmTimerIncludeRows(&dirty,bounds[1]-streakPadding,bounds[3]+streakPadding);
    }
    LmTimerDraw(&surface,75456,style,streak,&sCache);
    if(target>=0 && target<9) {
        LmTimerTargetBounds(style,target,bounds);
        const int x=(bounds[0]+bounds[2])/2,y=bounds[3]+3;
        const short arrow[]={(short)x,(short)y,(short)(x-4),(short)(y+5),(short)(x+4),(short)(y+5)};
        LmXfbFillPoly(&surface,arrow,3,0xB0F1C8,255);
        LmTimerIncludeRows(&dirty,y,y+6);
    }
    LmTimerFlushRows(&dirty,flush,flush+1);
}
__declspec(dllexport) unsigned int timer_texel(unsigned int texture,int x,int y) {
    return texture<13 ? LmTimerTexel(&kLmTimerTextures[texture],x,y) : 0;
}
__declspec(dllexport) int timer_bounds(const LmTimerStyle *style,int target,int *bounds) {
    return LmTimerTargetBounds(style,target,bounds);
}
}
