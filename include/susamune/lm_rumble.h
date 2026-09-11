#ifndef SUSAMUNE_LM_RUMBLE_H
#define SUSAMUNE_LM_RUMBLE_H

#define LM_RUMBLE_MANAGER_GLOBAL 0x804A1758u
#define LM_RUMBLE_PAD_GLOBAL 0x804A0BF8u
#define LM_RUMBLE_STATUS_GLOBAL 0x804A2064u
#define LM_RUMBLE_PAD_VTABLE 0x8038925Cu
#define LM_RUMBLE_RESET_ADDRESS 0x800852CCu
#define LM_RUMBLE_MOTOR_ADDRESS 0x801E4CE4u

typedef unsigned int (*LmRumbleRead)(unsigned int);
typedef void (*LmRumbleWrite)(unsigned int, unsigned int);
typedef void (*LmRumbleReset)(unsigned int, unsigned int);
typedef void (*LmRumbleMotor)(unsigned int, unsigned int);

static inline int LmRumbleInside(unsigned int address, unsigned int bytes,
    unsigned int start, unsigned int end) {
    return !(address & 3u) && start >= 0x80000000u &&
        end <= 0x81800000u && start < end && address >= start &&
        address < end && bytes <= end - address;
}

static inline int LmRumbleAllocation(LmRumbleRead read, unsigned int address,
    unsigned int bytes, unsigned int start, unsigned int end) {
    unsigned int size;
    if (!LmRumbleInside(address, bytes, start, end) || address - start < 16u ||
        (read(address - 16u) >> 16u) != 0x484Du) return 0;
    size = read(address - 12u);
    return !(size & 3u) && size >= bytes && size <= end - address;
}

/* Run only after a successful, validated GAME restore. SDK connection state
 * and the user's JUT enable mask stay live; transient motor waves do not. */
static inline void LmRumbleAfterLoad(LmRumbleRead read, LmRumbleWrite write,
    LmRumbleReset reset, LmRumbleMotor motor, unsigned int gameStart,
    unsigned int gameEnd, unsigned int systemHeap, unsigned int systemStart,
    unsigned int systemEnd) {
    unsigned int pad = read(LM_RUMBLE_PAD_GLOBAL);
    unsigned int manager = read(LM_RUMBLE_MANAGER_GLOBAL);
    unsigned int controllers[4], i, j;
    int validPad = LmRumbleAllocation(read, pad, 0x98u, systemStart, systemEnd) &&
        read(pad) == LM_RUMBLE_PAD_VTABLE && read(pad + 4u) == systemHeap;
    int validControllers = validPad &&
        LmRumbleAllocation(read, manager, 0x44u, gameStart, gameEnd);
    if (validPad) {
        unsigned int port = read(pad + 0x74u) >> 16u;
        validPad = port < 4u || port == 0xFFFFu;
        validControllers = validControllers && validPad;
    }
    for (i = 0u; validControllers && i < 4u; ++i) {
        unsigned int expectedPad = i == 0u ? pad : 0u;
        controllers[i] = read(manager + 4u + i * 4u);
        if (!LmRumbleAllocation(read, controllers[i], 0xD8u, gameStart, gameEnd) ||
            read(controllers[i]) != expectedPad ||
            (controllers[i] < manager + 0x44u && manager < controllers[i] + 0xD8u))
            validControllers = 0;
        for (j = 0u; validControllers && j < i; ++j)
            if (controllers[i] < controllers[j] + 0xD8u &&
                controllers[j] < controllers[i] + 0xD8u) validControllers = 0;
    }
    if (validControllers)
        for (i = 0u; i < 4u; ++i) reset(controllers[i], i == 0u ? pad : 0u);
    if (validPad)
        for (i = 0u; i < 4u; ++i) write(pad + 0x64u + 4u * i, 0u);
    write(LM_RUMBLE_STATUS_GLOBAL, 0u);
    /* Unconditional: the restored quiet latches cannot describe live motors. */
    for (i = 0u; i < 4u; ++i) motor(i, 2u);
}

#endif
