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

static unsigned int LmTimerTexel(const LmTimerTexture *t, int x, int y) {
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
    unsigned int value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const int lo = (a >> shift) & 255, hi = (b >> shift) & 255;
        value |= (unsigned int)((lo * (256 - fraction) + hi * fraction + 128) >> 8) << shift;
    }
    return value;
}

static unsigned int LmTimerSample(const LmTimerTexture *t, int u, int v) {
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

static LmXfbColour LmTimerColour(unsigned int rgba, unsigned int tint,
                                 int alpha, int brightness) {
    if (!(rgba&255) || !alpha) { const LmXfbColour clear={0,0,0,0}; return clear; }
    unsigned int rgb=0;
    for(int shift=0;shift<24;shift+=8) {
        const int channel=((rgba>>(shift+8))&255)*((tint>>shift)&255);
        const int level=LmXfbClamp((channel*brightness+12750)/25500,0,255);
        rgb|=(unsigned int)level<<shift;
    }
    return LmXfbRgb(rgb, ((rgba&255)*alpha+127)/255);
}

static void LmTimerPicture(LmXfbSurface *surface, const LmTimerQuad *q,
                            unsigned int texture, unsigned int tint,
                            int alpha, int brightness) {
    if (texture >= 13 || !alpha) return;
    const LmTimerTexture *t=&kLmTimerTextures[texture];
    const int left=LmXfbClamp(q->left,0,surface->width)&~1;
    const int right=(LmXfbClamp(q->right,0,surface->width)+1)&~1;
    const int top=LmXfbClamp(q->top,0,surface->height);
    const int bottom=LmXfbClamp(q->bottom,0,surface->height);
    const float a=t->width*4096.0f/q->w, b=t->height*4096.0f/q->h;
    const int du=(int)(q->c*a), dv=(int)(-q->s*b);
    for(int y=top;y<bottom;++y) {
        const float dx=left+0.5f-q->x, dy=y+0.5f-q->y;
        int u=(int)((q->c*dx+q->s*dy)*a)-2048;
        int v=(int)((-q->s*dx+q->c*dy)*b)-2048;
        for(int x=left;x<right;x+=2) {
            const LmXfbColour c0=LmTimerColour(LmTimerSample(t,u,v),tint,alpha,brightness);
            const LmXfbColour c1=LmTimerColour(LmTimerSample(t,u+du,v+dv),tint,alpha,brightness);
            unsigned char *p=surface->pixels+y*surface->stride+x*2;
            if(c0.alpha) p[0]=LmXfbBlend(p[0],c0.y,c0.alpha);
            if(c1.alpha) p[2]=LmXfbBlend(p[2],c1.y,c1.alpha);
            const int remaining=510-c0.alpha-c1.alpha;
            p[1]=(unsigned char)((p[1]*remaining+c0.cb*c0.alpha+c1.cb*c1.alpha+255)/510);
            p[3]=(unsigned char)((p[3]*remaining+c0.cr*c0.alpha+c1.cr*c1.alpha+255)/510);
            u+=du*2; v+=dv*2;
        }
    }
}

static void LmTimerDraw(LmXfbSurface *surface, unsigned int centiseconds,
                         const LmTimerStyle *style, const LmTimerStreakStyle *streak) {
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
                       LmXfbClamp(streak->brightness,0,200));
    }
    if(centiseconds>599999) centiseconds=599999;
    unsigned int digits[8];
    digits[0]=centiseconds/60000; digits[1]=(centiseconds/6000)%10;
    digits[2]=10; digits[3]=(centiseconds/1000)%6; digits[4]=(centiseconds/100)%10;
    digits[5]=10; digits[6]=(centiseconds/10)%10; digits[7]=centiseconds%10;
    for(int i=0;i<9;++i) {
        if(i==8 && !style->showLabel) continue;
        const LmTimerQuad q=LmTimerGeometry(&kLmTimerPanes[i],style,0);
        LmTimerPicture(surface,&q,i==8 ? 11 : digits[i],style->colours[i],opacity,brightness);
    }
}
#endif
