#ifndef SUSAMUNE_LM_XFB_DRAW_H
#define SUSAMUNE_LM_XFB_DRAW_H

/* CPU drawing into a completed GX Y0-Cb-Y1-Cr framebuffer. */
struct LmXfbSurface {
    unsigned char *pixels;
    int width, height, stride;
};
struct LmXfbColour { int y, cb, cr, alpha; };

static int LmXfbClamp(int x, int lo, int hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
static struct LmXfbColour LmXfbRgb(unsigned int rgb, unsigned int alpha) {
    const int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    struct LmXfbColour c;
    c.y = 16 + ((66 * r + 129 * g + 25 * b + 128) >> 8);
    c.cb = 128 + ((-38 * r - 74 * g + 112 * b + 128) >> 8);
    c.cr = 128 + ((112 * r - 94 * g - 18 * b + 128) >> 8);
    c.alpha = LmXfbClamp((int)alpha, 0, 255);
    return c;
}
static unsigned char LmXfbBlend(int old, int value, int alpha) {
    return (unsigned char)((old * (255 - alpha) + value * alpha + 127) / 255);
}
static int LmXfbValid(const struct LmXfbSurface *s) {
    return s && s->pixels && s->width > 0 && s->width <= 1024 &&
        !(s->width & 1) && s->height > 0 && s->height <= 1024 &&
        s->stride >= s->width * 2 && s->stride <= 4096;
}
static void LmXfbSpan(struct LmXfbSurface *s, int x0, int x1, int y,
                      struct LmXfbColour c) {
    int pair;
    if (y < 0 || y >= s->height || !c.alpha) return;
    x0 = LmXfbClamp(x0, 0, s->width);
    x1 = LmXfbClamp(x1, 0, s->width);
    if (x0 >= x1) return;
    if (c.alpha == 255) {
        unsigned char *row = s->pixels + y * s->stride;
        /* Only edge pixels share chroma with an uncovered neighbour. */
        if (x0 & 1) {
            unsigned char *p = row + (x0 - 1) * 2;
            p[2] = (unsigned char)c.y;
            p[1] = LmXfbBlend(p[1], c.cb, 128);
            p[3] = LmXfbBlend(p[3], c.cr, 128);
        }
        for (pair = (x0 + 1) & ~1; pair < (x1 & ~1); pair += 2) {
            unsigned char *p = row + pair * 2;
            p[0] = p[2] = (unsigned char)c.y;
            p[1] = (unsigned char)c.cb;
            p[3] = (unsigned char)c.cr;
        }
        if (x1 & 1) {
            unsigned char *p = row + (x1 - 1) * 2;
            p[0] = (unsigned char)c.y;
            p[1] = LmXfbBlend(p[1], c.cb, 128);
            p[3] = LmXfbBlend(p[3], c.cr, 128);
        }
        return;
    }
    for (pair = x0 & ~1; pair < x1; pair += 2) {
        unsigned char *p = s->pixels + y * s->stride + pair * 2;
        const int a = pair >= x0, b = pair + 1 < x1;
        const int chromaAlpha = (c.alpha * (a + b) + 1) / 2;
        if (a) p[0] = LmXfbBlend(p[0], c.y, c.alpha);
        if (b) p[2] = LmXfbBlend(p[2], c.y, c.alpha);
        p[1] = LmXfbBlend(p[1], c.cb, chromaAlpha);
        p[3] = LmXfbBlend(p[3], c.cr, chromaAlpha);
    }
}
static void LmXfbBox(struct LmXfbSurface *s, int x, int y, int w, int h,
                     unsigned int rgb, unsigned int alpha) {
    int row, bottom;
    struct LmXfbColour c;
    if (!LmXfbValid(s) || w <= 0 || h <= 0 ||
        x < -4096 || x > 4096 || y < -4096 || y > 4096 || w > 8192 || h > 8192) return;
    c = LmXfbRgb(rgb, alpha);
    bottom = LmXfbClamp(y + h, 0, s->height);
    for (row = LmXfbClamp(y, 0, s->height); row < bottom; ++row)
        LmXfbSpan(s, x, x + w, row, c);
}
static int LmXfbVerticesValid(const short *xy, int count) {
    int i;
    if (!xy || count < 3 || count > 32) return 0;
    for (i = 0; i < count * 2; ++i) if (xy[i] < -1024 || xy[i] > 1024) return 0;
    return 1;
}
static void LmXfbFillPoly(struct LmXfbSurface *s, const short *xy, int count,
                          unsigned int rgb, unsigned int alpha) {
    int i, y, top = 2048, bottom = -2048;
    struct LmXfbColour c;
    if (!LmXfbValid(s) || !LmXfbVerticesValid(xy, count)) return;
    c = LmXfbRgb(rgb, alpha);
    for (i = 0; i < count; ++i) {
        if (xy[i * 2 + 1] < top) top = xy[i * 2 + 1];
        if (xy[i * 2 + 1] > bottom) bottom = xy[i * 2 + 1];
    }
    top = LmXfbClamp(top, 0, s->height);
    bottom = LmXfbClamp(bottom, 0, s->height);
    /* All clients draw convex shapes. Intersections are fixed point 1/256. */
    for (y = top; y < bottom; ++y) {
        int left = 0x7fffffff, right = (-0x7fffffff - 1);
        for (i = 0; i < count; ++i) {
            const int j = (i + 1) % count;
            int x0 = xy[2 * i], y0 = xy[2 * i + 1];
            int x1 = xy[2 * j], y1 = xy[2 * j + 1], t, at;
            if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
            if (y < y0 || y >= y1) continue;
            at = x0 * 256 + ((2 * (y - y0) + 1) * (x1 - x0) * 128) / (y1 - y0);
            if (at < left) left = at;
            if (at > right) right = at;
        }
        if (left <= right) LmXfbSpan(s, (left + 127) >> 8, (right + 127) >> 8, y, c);
    }
}
static void LmXfbStrokePoly(struct LmXfbSurface *s, const short *xy, int count,
                            unsigned int rgb, unsigned int alpha, int thickness) {
    int i;
    struct LmXfbColour c;
    if (!LmXfbValid(s) || !LmXfbVerticesValid(xy, count)) return;
    c = LmXfbRgb(rgb, alpha);
    thickness = LmXfbClamp(thickness, 1, 4);
    for (i = 0; i < count; ++i) {
        const int j = (i + 1) % count;
        int x = xy[i * 2], y = xy[i * 2 + 1];
        const int endX = xy[j * 2], endY = xy[j * 2 + 1];
        int dx = endX - x, dy = endY - y, err, twice, row;
        const int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
        if (dx < 0) dx = -dx;
        if (dy > 0) dy = -dy;
        err = dx + dy;
        for (;;) {
            for (row = 0; row < thickness; ++row)
                LmXfbSpan(s, x - thickness / 2, x - thickness / 2 + thickness,
                          y - thickness / 2 + row, c);
            if (x == endX && y == endY) break;
            twice = 2 * err;
            if (twice >= dy) { err += dy; x += sx; }
            if (twice <= dx) { err += dx; y += sy; }
        }
    }
}
#endif
