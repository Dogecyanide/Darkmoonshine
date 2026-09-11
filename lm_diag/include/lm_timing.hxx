#ifndef LM_TIMING_HXX
#define LM_TIMING_HXX

// Portable counters: one sample per completed game update, not per VI.
namespace LMTiming {
// Horizontal displacement in the caller's fixed-point coordinate units.
// Integer square root keeps the overlay independent of libm and heap state.
inline unsigned horizontalSpeed(int dx, int dz) {
    const long long x = dx, z = dz;
    unsigned long long remainder = static_cast<unsigned long long>(x * x) +
                                   static_cast<unsigned long long>(z * z);
    unsigned long long result = 0, bit = 1ull << 62;
    while (bit > remainder) bit >>= 2;
    while (bit) {
        if (remainder >= result + bit) {
            remainder -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return static_cast<unsigned>(result);
}
inline bool rTriggerDown(unsigned buttons, unsigned rightAnalog) {
    // Raw-input threshold shared with the timing reference, not a trick test.
    return (buttons & 0x20u) != 0u || rightAnalog >= 30u;
}
template <class Pad> inline bool rTriggerDown(const Pad &pad) {
    return rTriggerDown(pad.mButton, pad.mTriggerRight);
}
struct Pump {
    static constexpr unsigned kPopupFrames = 90u;
    unsigned hold = 0, last = 0, popup = 0;
    bool ready = false, holding = false, finished = false;
    bool visible() const { return holding || popup != 0u; }
    void cancel() { hold = popup = 0; ready = holding = false; }
    void reset() { cancel(); last = 0; finished = false; }
    void sample(bool down, bool active) {
        if (!active) { cancel(); return; }
        if (popup) --popup;
        // Never time the tail of a press begun in a menu or another timeline.
        if (!ready) { if (!down) ready = true; return; }
        if (down) {
            holding = true;
            popup = 0;
            if (hold != 0xffffffffu) ++hold;
        } else if (holding) {
            last = hold; finished = true; hold = 0; holding = false;
            popup = kPopupFrames;
        }
    }
};
struct Pulse {
    unsigned elapsed = 0, delay = 0, hold = 0;
    bool armed = false, pressed = false, finished = false;
    void arm() { elapsed = delay = hold = 0; armed = true; pressed = finished = false; }
    void sample(bool down) {
        if (!armed || finished) return;
        if (!pressed && down) { pressed = true; delay = elapsed; }
        if (pressed) {
            if (down) { if (hold != 0xffffffffu) ++hold; }
            else { finished = true; armed = false; }
        }
        if (elapsed != 0xffffffffu) ++elapsed;
    }
};
inline int compare(unsigned value, unsigned target, unsigned tolerance) {
    if (value < target && target - value > tolerance) return -1;
    if (value > target && value - target > tolerance) return 1;
    return 0;
}
struct Lag {
    unsigned last = 0, extra = 0, slow = 0, samples = 0;
    bool valid = false;
    void reset() { last = extra = slow = samples = 0; valid = false; }
    void sample(unsigned vi, unsigned period, bool active) {
        const unsigned delta = vi - last;
        last = vi;
        const bool usable = active && period >= 1 && period <= 4;
        if (usable && valid) {
            ++samples;
            if (delta > period) { extra += delta - period; ++slow; }
        }
        valid = usable;
    }
};
}  // namespace LMTiming
#endif
