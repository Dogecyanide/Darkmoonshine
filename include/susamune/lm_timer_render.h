#ifndef SUSAMUNE_LM_TIMER_RENDER_H
#define SUSAMUNE_LM_TIMER_RENDER_H

#include "susamune/lm_xfb_draw.h"
#include "lm_timer_assets.hxx"

struct LmTimerStyle {
    int x, y, scale, opacity, brightness;
    unsigned int colours[9]; // MM'SS"cc followed by TIME.
    int showLabel, showStreak;
};
struct LmTimerStreakStyle {
    int x, y, scale, opacity, brightness; // Offset relative to the timer root.
    unsigned int rgb;
};

// Test instrumentation compiles away completely in the game.
#ifndef LM_TIMER_WORK
#define LM_TIMER_WORK(kind, amount) ((void)0)
#endif

static unsigned int LmTimerTexel(const LmTimerTexture *t, int x, int y) {
    LM_TIMER_WORK(0, 1);
    x = LmXfbClamp(x, 0, t->width - 1);
    y = LmXfbClamp(y, 0, t->height - 1);
    if (t->format == 0) {
        const int at = ((y / 8) * ((t->width + 7) / 8) + x / 8) * 32 +
                       (y & 7) * 4 + (x & 7) / 2;
        const unsigned int n = (t->data[at] >> ((x & 1) ? 0 : 4)) & 15;
        return 0xffffff00u | (n * 17);
    }
    const int at = (((y / 4) * ((t->width + 3) / 4) + x / 4) * 16 +
                    (y & 3) * 4 + (x & 3)) * 2;
    const unsigned int v = (unsigned int)t->data[at] * 256 + t->data[at + 1];
    unsigned int r, g, b, a;
    if (v & 0x8000) {
        r = (v >> 10) & 31; g = (v >> 5) & 31; b = v & 31;
        r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
        a = 255;
    } else {
        r = ((v >> 8) & 15) * 17; g = ((v >> 4) & 15) * 17; b = (v & 15) * 17;
        a = (v >> 12) & 7; a = (a << 5) | (a << 2) | (a >> 1);
    }
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static unsigned int LmTimerLerp(unsigned int a, unsigned int b, int fraction) {
    const unsigned int inverse = 256u - fraction;
    const unsigned int lo = ((a & 0x00FF00FFu) * inverse +
        (b & 0x00FF00FFu) * fraction + 0x00800080u) >> 8;
    const unsigned int hi = (((a >> 8) & 0x00FF00FFu) * inverse +
        ((b >> 8) & 0x00FF00FFu) * fraction + 0x00800080u);
    return (lo & 0x00FF00FFu) | (hi & 0xFF00FF00u);
}

static unsigned int LmTimerSample(const LmTimerTexture *t, int u, int v) {
    LM_TIMER_WORK(1, 1);
    if (u < -2048 || v < -2048 || u >= t->width * 4096 - 2048 ||
        v >= t->height * 4096 - 2048) return 0;
    const int x = u >> 12, y = v >> 12;
    const int fx = (u & 4095) >> 4, fy = (v & 4095) >> 4;
    return LmTimerLerp(LmTimerLerp(LmTimerTexel(t,x,y), LmTimerTexel(t,x+1,y),fx),
                       LmTimerLerp(LmTimerTexel(t,x,y+1), LmTimerTexel(t,x+1,y+1),fx),fy);
}

struct LmTimerQuad { float x, y, w, h, c, s; int left, top, right, bottom; };

static LmTimerQuad LmTimerGeometry(const LmTimerPane *p, const LmTimerStyle *style,
                                   const LmTimerStreakStyle *streak) {
    LmTimerQuad q;
    const float scale = LmXfbClamp(style->scale, 25, 200) * 0.01f;
    const float extra = streak ? LmXfbClamp(streak->scale, 25, 200) * 0.01f : 1.0f;
    q.x = style->x + (p->x + (streak ? streak->x : 0)) * scale;
    q.y = style->y + (p->y + (streak ? streak->y : 0)) * scale;
    q.w = p->width * scale * extra; q.h = p->height * scale * extra;
    q.c = p->cosine * (1.0f / 16384.0f); q.s = p->sine * (1.0f / 16384.0f);
    float xmin=q.x, xmax=q.x, ymin=q.y, ymax=q.y;
    for (int i=1; i<4; ++i) {
        const float x=q.x + ((i&1) ? q.c*q.w : 0) - ((i&2) ? q.s*q.h : 0);
        const float y=q.y + ((i&1) ? q.s*q.w : 0) + ((i&2) ? q.c*q.h : 0);
        if(x<xmin) xmin=x; if(x>xmax) xmax=x;
        if(y<ymin) ymin=y; if(y>ymax) ymax=y;
    }
    q.left=(int)xmin-1; q.top=(int)ymin-1; q.right=(int)xmax+1; q.bottom=(int)ymax+1;
    return q;
}

static int LmTimerTargetBounds(const LmTimerStyle *style, int target, int bounds[4]) {
    if (!style || !bounds || target < 0 || target >= 9 || style->x < -2048 ||
        style->x > 2048 || style->y < -2048 || style->y > 2048) return 0;
    const LmTimerQuad q = LmTimerGeometry(&kLmTimerPanes[target], style, 0);
    bounds[0]=q.left; bounds[1]=q.top; bounds[2]=q.right; bounds[3]=q.bottom;
    return 1;
}

static int LmTimerStreakBounds(const LmTimerStyle *style, const LmTimerStreakStyle *streak,
                                int bounds[4]) {
    if (!style || !streak || !bounds || style->x < -2048 || style->x > 2048 ||
        style->y < -2048 || style->y > 2048 || streak->x < -2048 || streak->x > 2048 ||
        streak->y < -2048 || streak->y > 2048) return 0;
    const LmTimerQuad q=LmTimerGeometry(&kLmTimerPanes[9],style,streak);
    bounds[0]=q.left; bounds[1]=q.top; bounds[2]=q.right; bounds[3]=q.bottom;
    return 1;
}

static int LmTimerBounds(const LmTimerStyle *style, const LmTimerStreakStyle *streak,
                          int bounds[4]) {
    if (!LmTimerTargetBounds(style,0,bounds)) return 0;
    int b[4];
    for (int i=1;i<10;++i) {
        if (i==8 && !style->showLabel) continue;
        if (i==9) {
            if (!style->showStreak || !LmTimerStreakBounds(style,streak,b)) continue;
        } else if (!LmTimerTargetBounds(style,i,b)) return 0;
        if (b[0]<bounds[0]) bounds[0]=b[0]; if (b[1]<bounds[1]) bounds[1]=b[1];
        if (b[2]>bounds[2]) bounds[2]=b[2]; if (b[3]>bounds[3]) bounds[3]=b[3];
    }
    return 1;
}

struct LmTimerDirtyRows { int top, bottom; };
static void LmTimerIncludeRows(LmTimerDirtyRows *rows, int top, int bottom) {
    if(top<rows->top) rows->top=top;
    if(bottom>rows->bottom) rows->bottom=bottom;
}
static void LmTimerFlushRows(const LmTimerDirtyRows *rows, int *top, int *height) {
    *top=LmXfbClamp(rows->top,0,480)/2;
    const int bottom=(LmXfbClamp(rows->bottom,0,480)+1)/2;
    *height=bottom>*top ? bottom-*top : 0;
}

static LmXfbColour LmTimerColour(unsigned int rgba, unsigned int tint,
                                 int alpha, int brightness) {
    if (!(rgba&255) || !alpha) { const LmXfbColour clear={0,0,0,0}; return clear; }
    LM_TIMER_WORK(2, 1);
    if ((tint & 0xFFFFFFu) == 0xFFFFFFu && brightness == 100)
        return LmXfbRgb(rgba >> 8, ((rgba & 255) * alpha + 127) / 255);
    unsigned int rgb=0;
    for(int shift=0;shift<24;shift+=8) {
        const int channel=((rgba>>(shift+8))&255)*((tint>>shift)&255);
        const int level=LmXfbClamp((channel*brightness+12750)/25500,0,255);
        rgb|=(unsigned int)level<<shift;
    }
    return LmXfbRgb(rgb, ((rgba&255)*alpha+127)/255);
}

struct LmTimerCachedPane {
    LmTimerQuad quad;
    unsigned int texture, tint;
    int alpha, brightness, width, height, valid; // 0 empty, 1 complete, 2 use direct path.
};
struct LmTimerCache {
    LmTimerCachedPane panes[10];
    unsigned char pixels[65536];
};
static_assert(sizeof(LmTimerCache) <= 65u * 1024u, "Timer cache must stay in the reserved mod blob");
static_assert(6u*4096u+2u*2048u+8192u+28672u==65536u, "Timer cache slots must exactly tile storage");

static unsigned int LmTimerCacheCapacity(int slot) {
    return slot == 9 ? 28672u : slot == 8 ? 8192u : slot == 2 || slot == 5 ? 2048u : 4096u;
}
static unsigned char *LmTimerCacheData(LmTimerCache *cache, int slot) {
    unsigned int offset = 0;
    for (int i = 0; i < slot; ++i) offset += LmTimerCacheCapacity(i);
    return cache->pixels + offset;
}
static int LmTimerCacheMatches(const LmTimerCachedPane *p, const LmTimerQuad *q,
    unsigned int texture, unsigned int tint, int alpha, int brightness, const LmXfbSurface *s) {
    return p->valid && p->texture == texture && p->tint == tint && p->alpha == alpha &&
        p->brightness == brightness && p->width == s->width && p->height == s->height &&
        p->quad.x == q->x && p->quad.y == q->y && p->quad.w == q->w && p->quad.h == q->h &&
        p->quad.c == q->c && p->quad.s == q->s && p->quad.left == q->left &&
        p->quad.right == q->right && p->quad.top == q->top && p->quad.bottom == q->bottom;
}
static void LmTimerBlendPair(unsigned char *p, const LmXfbColour &c0, const LmXfbColour &c1) {
    if (!(c0.alpha | c1.alpha)) return;
    LM_TIMER_WORK(3, 1);
    if(c0.alpha) p[0]=LmXfbBlend(p[0],c0.y,c0.alpha);
    if(c1.alpha) p[2]=LmXfbBlend(p[2],c1.y,c1.alpha);
    const int remaining=510-c0.alpha-c1.alpha;
    p[1]=(unsigned char)((p[1]*remaining+c0.cb*c0.alpha+c1.cb*c1.alpha+255)/510);
    p[3]=(unsigned char)((p[3]*remaining+c0.cr*c0.alpha+c1.cr*c1.alpha+255)/510);
}
static void LmTimerPutColour(unsigned char *p, const LmXfbColour &c) {
    p[0]=(unsigned char)c.y; p[1]=(unsigned char)c.cb;
    p[2]=(unsigned char)c.cr; p[3]=(unsigned char)c.alpha;
}
static LmXfbColour LmTimerGetColour(const unsigned char *p) {
    const LmXfbColour c={p[0],p[1],p[2],p[3]}; return c;
}
static void LmTimerPut16(unsigned char *p, unsigned int value) {
    p[0]=(unsigned char)(value >> 8); p[1]=(unsigned char)value;
}
static unsigned int LmTimerGet16(const unsigned char *p) { return (p[0] << 8) | p[1]; }

// Clip in the original integer stepping domain; recomputing u/v at a new x
// would round differently and move edge pixels in custom-size previews.
static void LmTimerClipAxis(int origin, int step, int limit, int *first, int *end) {
    if (step <= 0) return;
    if (origin < -2048) {
        const int start=(-2048-origin+step-1)/step;
        if (start>*first) *first=start;
    }
    const int last=limit-2048-1-origin;
    const int stop=last<0 ? 0 : last/step+1;
    if(stop<*end) *end=stop;
}

static void LmTimerPicture(LmXfbSurface *surface, const LmTimerQuad *q,
                            unsigned int texture, unsigned int tint,
                            int alpha, int brightness, LmTimerCache *cache = 0, int slot = 0) {
    if (texture >= 13 || !alpha) return;
    const LmTimerTexture *t=&kLmTimerTextures[texture];
    const int left=LmXfbClamp(q->left,0,surface->width)&~1;
    const int right=(LmXfbClamp(q->right,0,surface->width)+1)&~1;
    const int top=LmXfbClamp(q->top,0,surface->height);
    const int bottom=LmXfbClamp(q->bottom,0,surface->height);
    if(left>=right || top>=bottom) return;
    LmTimerCachedPane *cached=cache ? &cache->panes[slot] : 0;
    unsigned char *data=cache ? LmTimerCacheData(cache,slot) : 0;
    const unsigned int capacity=cache ? LmTimerCacheCapacity(slot) : 0u;
    const bool same=cached && LmTimerCacheMatches(cached,q,texture,tint,alpha,brightness,surface);
    if(same && cached->valid==1) {
        LM_TIMER_WORK(4, 1);
        unsigned int at=0;
        for(int y=top;y<bottom;++y) {
            const unsigned int x=LmTimerGet16(data+at), count=LmTimerGet16(data+at+2);
            at+=4;
            unsigned char *p=surface->pixels+y*surface->stride+x*2;
            for(unsigned int i=0;i<count;++i,at+=8,p+=4)
                LmTimerBlendPair(p,LmTimerGetColour(data+at),LmTimerGetColour(data+at+4));
        }
        return;
    }
    bool building=cached && !same;
    if(building) { cached->valid=0; LM_TIMER_WORK(5, 1); }
    unsigned int used=0;
    const float a=t->width*4096.0f/q->w, b=t->height*4096.0f/q->h;
    const int du=(int)(q->c*a), dv=(int)(-q->s*b);
    for(int y=top;y<bottom;++y) {
        const float dx=left+0.5f-q->x, dy=y+0.5f-q->y;
        int u=(int)((q->c*dx+q->s*dy)*a)-2048;
        int v=(int)((-q->s*dx+q->c*dy)*b)-2048;
        int first=0,end=right-left;
        LmTimerClipAxis(u,du,t->width*4096,&first,&end);
        LmTimerClipAxis(v,dv,t->height*4096,&first,&end);
        first=LmXfbClamp(first,0,right-left)&~1;
        end=(LmXfbClamp(end,0,right-left)+1)&~1;
        u+=du*first; v+=dv*first;
        const unsigned int row=used;
        unsigned int last=used+4;
        int firstOpaque=-1,lastOpaque=-1;
        if(building) {
            if(used+4>capacity) building=false;
            else { LmTimerPut16(data+used,0); LmTimerPut16(data+used+2,0); used+=4; }
        }
        for(int x=left+first;x<left+end;x+=2) {
            const LmXfbColour c0=LmTimerColour(LmTimerSample(t,u,v),tint,alpha,brightness);
            const LmXfbColour c1=LmTimerColour(LmTimerSample(t,u+du,v+dv),tint,alpha,brightness);
            unsigned char *p=surface->pixels+y*surface->stride+x*2;
            LmTimerBlendPair(p,c0,c1);
            if(building) {
                if((c0.alpha|c1.alpha) && firstOpaque<0) firstOpaque=x;
                if(firstOpaque>=0) {
                    if(used+8>capacity) building=false;
                    else {
                        LmTimerPutColour(data+used,c0); LmTimerPutColour(data+used+4,c1); used+=8;
                        if(c0.alpha|c1.alpha) { last=used; lastOpaque=x; }
                    }
                }
            }
            u+=du*2; v+=dv*2;
        }
        if(building) {
            // Trim empty row ends, but retain holes between visible pixels.
            used=last;
            LmTimerPut16(data+row,firstOpaque<0 ? 0 : firstOpaque);
            LmTimerPut16(data+row+2,firstOpaque<0 ? 0 : (lastOpaque-firstOpaque)/2+1);
        }
    }
    if(cached && !same) {
        cached->quad=*q; cached->texture=texture; cached->tint=tint;
        cached->alpha=alpha; cached->brightness=brightness;
        cached->width=surface->width; cached->height=surface->height;
        cached->valid=building ? 1 : 2; // An incomplete stream must never be replayed.
        LM_TIMER_WORK(6, building ? used : 0);
    }
}

static void LmTimerDraw(LmXfbSurface *surface, unsigned int centiseconds,
                         const LmTimerStyle *style, const LmTimerStreakStyle *streak,
                         LmTimerCache *cache = 0) {
    if(!LmXfbValid(surface) || !style || style->x < -2048 || style->x > 2048 ||
       style->y < -2048 || style->y > 2048 || (streak && (streak->x < -2048 ||
       streak->x > 2048 || streak->y < -2048 || streak->y > 2048))) return;
    const int opacity=LmXfbClamp(style->opacity,0,255);
    const int brightness=LmXfbClamp(style->brightness,0,200);
    if(!opacity) return;
    if(style->showStreak && streak) {
        const LmTimerQuad q=LmTimerGeometry(&kLmTimerPanes[9],style,streak);
        const int a=(opacity*LmXfbClamp(streak->opacity,0,255)+127)/255;
        LmTimerPicture(surface,&q,12,streak->rgb,(a*180+127)/255,
                       LmXfbClamp(streak->brightness,0,200),cache,9);
    }
    if(centiseconds>599999) centiseconds=599999;
    unsigned int digits[8];
    digits[0]=centiseconds/60000; digits[1]=(centiseconds/6000)%10;
    digits[2]=10; digits[3]=(centiseconds/1000)%6; digits[4]=(centiseconds/100)%10;
    digits[5]=10; digits[6]=(centiseconds/10)%10; digits[7]=centiseconds%10;
    for(int i=0;i<9;++i) {
        if(i==8 && !style->showLabel) continue;
        const LmTimerQuad q=LmTimerGeometry(&kLmTimerPanes[i],style,0);
        LmTimerPicture(surface,&q,i==8 ? 11 : digits[i],style->colours[i],opacity,brightness,cache,i);
    }
}
#endif
