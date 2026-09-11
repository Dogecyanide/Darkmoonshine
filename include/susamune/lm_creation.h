#ifndef SUSAMUNE_LM_CREATION_H
#define SUSAMUNE_LM_CREATION_H

// Moonshine Creation's controller editor, without Sunshine/J2D dependencies.
namespace LMCreation {
struct Style {
    unsigned short x, y;
    unsigned char scale, textA, bgR, bgG, bgB, bgA, textBrightness, padding;
};
static_assert(sizeof(Style) == 12, "Creation style must stay compact");
enum Option { Red, Green, Blue, Opacity, Brightness, BgRed, BgGreen, BgBlue,
              BgOpacity, Padding, OptionCount };
enum Confirm { None, Keep, Discard, Reset };
inline int clamp(int v, int low, int high) { return v < low ? low : v > high ? high : v; }
struct Movement {
    int direction = 0;
    unsigned updates = 0;
    void reset() { direction = 0; updates = 0; }
    int step(int next) {
        if (!next) { reset(); return 0; }
        if (next != direction) { direction = next; updates = 0; return next * 2; }
        if (updates < 30u) ++updates;
        if (updates < 6u) return 0;
        return next * (updates < 15u ? 2 : updates < 30u ? 6 : 10);
    }
};
inline unsigned char &scalar(Style &s, unsigned option) {
    const unsigned offsets[] = {5, 10, 6, 7, 8, 9, 11};
    return reinterpret_cast<unsigned char *>(&s)[offsets[option - Opacity]];
}
struct Editor {
    Style backup;
    unsigned char rgbBackup[16][3];
    unsigned target = 0, option = 0, count = 0, confirm = None;
    unsigned previous = 0, repeated = 0, repeatFrames = 0;
    Movement moveX, moveY;
    bool active = false;
    void resetMovement() { moveX.reset(); moveY.reset(); }
    void begin(const Style &style, const unsigned char (*rgb)[3], unsigned slots,
               unsigned held) {
        if (!rgb || !slots || slots > 16u || active) return;
        backup = style; count = slots;
        for (unsigned i = 0; i < count; ++i)
            for (unsigned c = 0; c < 3; ++c) rgbBackup[i][c] = rgb[i][c];
        target = option = confirm = repeated = repeatFrames = 0;
        resetMovement();
        previous = held; active = true;
    }
    unsigned repeat(unsigned mask) {
        unsigned fired = mask & ~repeated;
        if (!mask || mask != repeated) repeatFrames = 0;
        else if (++repeatFrames >= 18u) { fired |= mask; repeatFrames = 14u; }
        repeated = mask;
        return fired;
    }
    // Bits are raw SDK buttons plus four synthetic C-stick directions.
    bool update(Style &style, unsigned char (*rgb)[3], const Style &defaults,
                const unsigned char (*defaultRgb)[3], unsigned buttons,
                int cx, int cy, bool connected) {
        if (!active) return false;
        const unsigned pressed = connected ? buttons & ~previous : 0u;
        previous = buttons;
        if (!connected) { repeated = repeatFrames = 0; resetMovement(); return false; }
        if (confirm) {
            resetMovement();
            if (pressed & 0x100u) {
                if (confirm == Keep || confirm == Discard) {
                    if (confirm == Discard) {
                        style = backup;
                        for (unsigned i = 0; i < count; ++i)
                            for (unsigned c = 0; c < 3; ++c) rgb[i][c] = rgbBackup[i][c];
                    }
                    active = false; confirm = None; return true;
                }
                if (option <= Blue) {
                    for (unsigned i = target ? target - 1 : 0;
                         i < (target ? target : count); ++i)
                        rgb[i][option] = defaultRgb[i][option];
                } else scalar(style, option) = scalar(const_cast<Style &>(defaults), option);
                confirm = None;
            } else if (pressed & 0x200u) confirm = None;
            return false;
        }
        if (pressed & 0x100u) { confirm = Keep; resetMovement(); return false; }
        if (pressed & 0x200u) { confirm = Discard; resetMovement(); return false; }
        if (pressed & 0x10u) { confirm = Reset; resetMovement(); return false; }
        if (pressed & 0x1000u)
            target = (target + ((buttons & 0x400u) ? count : 1u)) % (count + 1u);
        // Movement accelerates independently; colour/size repeats stay precise.
        const int dx = moveX.step(((buttons & 2u) != 0u) - ((buttons & 1u) != 0u));
        const int dy = moveY.step(((buttons & 4u) != 0u) - ((buttons & 8u) != 0u));
        style.x = clamp(static_cast<int>(style.x) + dx, 0, 640);
        style.y = clamp(static_cast<int>(style.y) + dy, 0, 480);
        unsigned mask = buttons & 0x60u;
        if (cx <= -48) mask |= 0x10000u;
        if (cx >= 48) mask |= 0x20000u;
        if (cy <= -48) mask |= 0x40000u;
        if (cy >= 48) mask |= 0x80000u;
        const unsigned fired = repeat(mask);
        if (fired & 0x40u) style.scale = clamp(style.scale - 2, 50, 200);
        if (fired & 0x20u) style.scale = clamp(style.scale + 2, 50, 200);
        if (fired & 0x80000u) option = (option + OptionCount - 1) % OptionCount;
        else if (fired & 0x40000u) option = (option + 1) % OptionCount;
        const int delta = fired & 0x20000u ? 4 : fired & 0x10000u ? -4 : 0;
        if (!delta) return false;
        if (option <= Blue) {
            const unsigned first = target ? target - 1 : 0;
            const unsigned char value = clamp(rgb[first][option] + delta, 0, 255);
            for (unsigned i = first; i < (target ? target : count); ++i) rgb[i][option] = value;
        } else if (option == Padding) {
            if (style.padding == 255) { if (delta > 0) style.padding = 0; }
            else if (!style.padding && delta < 0) style.padding = 255;
            else style.padding = clamp(style.padding + (delta > 0 ? 1 : -1), 0, 16);
        } else {
            unsigned char &v = scalar(style, option);
            v = clamp(v + delta, option == Brightness ? 25 : 0,
                      option == Brightness ? 200 : 255);
        }
        return false;
    }
    int value(Style &style, const unsigned char (*rgb)[3], unsigned field) const {
        if (field > Blue) return scalar(style, field);
        const unsigned first = target ? target - 1 : 0;
        const int v = rgb[first][field];
        for (unsigned i = first + 1; i < (target ? target : count); ++i)
            if (v != rgb[i][field]) return -1;
        return v;
    }
};
}
#endif
