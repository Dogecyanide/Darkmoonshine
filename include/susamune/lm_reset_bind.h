#ifndef SUSAMUNE_LM_RESET_BIND_H
#define SUSAMUNE_LM_RESET_BIND_H

namespace LMResetBind {
// Leave D-left/right/down and Start to state/menu/console-reset shortcuts.
constexpr unsigned Allowed = 0x0F78u;
inline unsigned count(unsigned mask) {
    unsigned n = 0;
    while (mask) { mask &= mask - 1u; ++n; }
    return n;
}
inline bool valid(unsigned mask) {
    return !(mask & ~Allowed) && count(mask) >= 2u && count(mask) <= 4u;
}
struct Trigger {
    bool armed;
    bool sample(unsigned bind, unsigned held, bool enabled, bool connected) {
        if (!enabled || !connected) { armed = false; return false; }
        if (!held) { armed = true; return false; }
        if (!valid(bind) || !armed) return false;
        if (held & ~bind) { armed = false; return false; }
        if (held != bind) return false;
        armed = false;
        return true;
    }
};
enum Result { Waiting, Accepted, Cancelled, Invalid };
struct Recorder {
    bool releaseFirst, invalid;
    unsigned chord;
    void begin() { releaseFirst = true; invalid = false; chord = 0; }
    Result sample(unsigned held, bool connected) {
        if (!connected) return Cancelled;
        if (releaseFirst) {
            if (!held) releaseFirst = false;
            return Waiting;
        }
        if (held) {
            if ((held & ~Allowed) || count(held) > 4u) invalid = true;
            if ((held & chord) == chord) chord = held;
            else if ((held & chord) != held) invalid = true;
            return Waiting;
        }
        if (!chord) return Waiting;
        if (!invalid && chord == 0x0200u) return Cancelled;
        return !invalid && valid(chord) ? Accepted : Invalid;
    }
};
inline void text(unsigned mask, char out[32]) {
    const unsigned bits[] = {0x40u, 0x20u, 0x10u, 0x100u, 0x200u, 0x400u, 0x800u, 8u};
    const char *const names[] = {"L", "R", "Z", "A", "B", "X", "Y", "D-Up"};
    unsigned at = 0;
    if (!valid(mask)) { out[0]='O'; out[1]='F'; out[2]='F'; out[3]=0; return; }
    for (unsigned i=0; i<8u; ++i) {
        if (!(mask & bits[i])) continue;
        if (at) out[at++]='+';
        for (const char *p=names[i]; *p; ++p) out[at++]=*p;
    }
    out[at]=0;
}
}
#endif
