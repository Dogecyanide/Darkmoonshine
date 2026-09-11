#ifndef SUSAMUNE_LM_STATE_HOTKEYS_H
#define SUSAMUNE_LM_STATE_HOTKEYS_H
/* Face/shoulder buttons may be held while practising. Only another D-pad
 * direction conflicts; a held direction must not retrigger on button release. */
static inline int LmStateDirectionEdge(unsigned int buttons,
    unsigned int previous, unsigned int direction, int connected) {
    return connected && (buttons & 15u) == direction &&
        (previous & direction) == 0u;
}

struct LmStateHotkeyLatch {
    unsigned int previous, pending;
    // Sticky diagnostics for one tap; menu navigation must not erase them.
    unsigned int lastButtons, lastConnected, lastLatched, lastDisposition;
};
static inline void LmStateSampleHotkeys(struct LmStateHotkeyLatch *latch,
    unsigned int buttons, int connected, int available) {
    const int newDirection = available && (buttons & 3u) &&
        ((buttons ^ latch->previous) & 15u);
    if (newDirection) {
        latch->lastButtons = buttons;
        latch->lastConnected = connected != 0;
        latch->lastDisposition = 0u;
    }
    if ((!connected || !available) && latch->pending)
        latch->lastDisposition = 3u;  // cancelled before presenter
    if (!connected || !available) latch->pending = 0u;
    else if (LmStateDirectionEdge(buttons, latch->previous, 1u, connected)) latch->pending = 1u;
    else if (LmStateDirectionEdge(buttons, latch->previous, 2u, connected)) latch->pending = 2u;
    if (newDirection) latch->lastLatched = latch->pending;
    latch->previous = connected ? buttons : 0u;
}
static inline unsigned int LmStateConsumeHotkey(struct LmStateHotkeyLatch *latch,
                                               int available) {
    const unsigned int result = available ? latch->pending : 0u;
    if (latch->pending) latch->lastDisposition = available ? 1u : 2u;
    latch->pending = 0u;
    return result;
}
#endif
