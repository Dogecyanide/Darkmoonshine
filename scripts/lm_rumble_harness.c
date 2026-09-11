#include "susamune/lm_rumble.h"

static unsigned int game[0x1000], system[0x400];
static unsigned int manager, pad, status, enable, motors[4], calls, resets, writes, bad;
static unsigned int* location(unsigned int a) {
    if (a >= 0x81000000u && a < 0x81004000u && !(a & 3u))
        return &game[(a - 0x81000000u) / 4u];
    if (a >= 0x80500000u && a < 0x80501000u && !(a & 3u))
        return &system[(a - 0x80500000u) / 4u];
    if (a == LM_RUMBLE_MANAGER_GLOBAL) return &manager;
    if (a == LM_RUMBLE_PAD_GLOBAL) return &pad;
    if (a == LM_RUMBLE_STATUS_GLOBAL) return &status;
    if (a == LM_RUMBLE_STATUS_GLOBAL + 4u) return &enable;
    ++bad;
    return &bad;
}
static unsigned int read(unsigned int a) { return *location(a); }
static void write(unsigned int a, unsigned int v) { ++writes; *location(a) = v; }
static void motor(unsigned int channel, unsigned int command) {
    ++calls;
    if (channel > 3u || command > 2u) { ++bad; return; }
    motors[channel] = command;
}
static void nativeReset(unsigned int controller, unsigned int gamepad) {
    unsigned int i;
    ++resets;
    *location(controller) = gamepad;
    *location(controller + 4u) &= 0xFFFFu;
    for (i = 0u; i < 4u; ++i) {
        unsigned int slot = controller + 8u + 0x18u * i, j;
        *location(slot) &= 0xFFFFFFu;
        for (j = 4u; j < 0x18u; j += 4u) *location(slot + j) = 0u;
        slot = controller + 0x68u + 0x1Cu * i;
        *location(slot) &= 0xFFFFFFu;
        *location(slot + 4u) = *location(slot + 8u) = 0u;
    }
}
static void allocation(unsigned int a, unsigned int size) {
    *location(a - 16u) = 0x484D0000u;
    *location(a - 12u) = size;
}
__declspec(dllexport) void setup(void) {
    unsigned int i, j;
    for (i = 0; i < 0x1000u; ++i) game[i] = 0x11111111u;
    for (i = 0; i < 0x400u; ++i) system[i] = 0x22222222u;
    manager = 0x81000100u; pad = 0x80500100u;
    allocation(manager, 0x44u); allocation(pad, 0x98u);
    *location(pad) = LM_RUMBLE_PAD_VTABLE;
    *location(pad + 4u) = 0x804F0000u;
    *location(pad + 0x74u) = 0x0000FECAu;
    for (i = 0; i < 4u; ++i) {
        unsigned int controller = 0x81000200u + 0x100u * i;
        allocation(controller, 0xD8u);
        *location(manager + 4u + i * 4u) = controller;
        *location(controller) = i == 0u ? pad : 0u;
        for (j = 0; j < 4u; ++j) *location(pad + 0x64u + 4u*j) = 0xFFFFFFFFu;
        motors[i] = 1u;
    }
    status = 0x01010101u; enable = 0xA0000000u;
    calls = resets = writes = bad = 0u;
}
__declspec(dllexport) void run(unsigned int successful) {
    if (successful) LmRumbleAfterLoad(read, write, nativeReset, motor,
        0x81000000u, 0x81004000u, 0x804F0000u, 0x80500000u, 0x80501000u);
}
__declspec(dllexport) unsigned int word(unsigned int a) { return read(a); }
__declspec(dllexport) void poke(unsigned int a, unsigned int v) { *location(a) = v; }
__declspec(dllexport) unsigned int metric(unsigned int index) {
    if (index < 4u) return motors[index];
    if (index == 4u) return calls;
    if (index == 5u) return resets;
    if (index == 6u) return writes;
    return bad;
}
/* Authenticated LM update's on/off edge logic, with a fresh game event. */
__declspec(dllexport) void future_event(unsigned int active) {
    unsigned int controller = read(manager + 4u);
    unsigned int flags = read(controller + 4u);
    if (active && !(flags >> 24u) && (enable & 0x80000000u)) {
        motor(0u, 1u); status |= 0x01000000u;
        *location(controller + 4u) = flags | 0x01010000u;
    } else if (!active && ((flags >> 16u) & 0xFFu)) {
        motor(0u, 2u); status &= 0x00FFFFFFu;
        *location(controller + 4u) = flags & 0xFFFFu;
    }
}
