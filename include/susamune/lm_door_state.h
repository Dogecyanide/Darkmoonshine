#ifndef SUSAMUNE_LM_DOOR_STATE_H
#define SUSAMUNE_LM_DOOR_STATE_H

#define LM_DOOR_CONTROLLER_OFFSET 0x7E4u
#define LM_DOOR_MODE_OFFSET 0x310u
#define LM_DOOR_STATE_OFFSET 0x314u
#define LM_DOOR_CONTROLLER_READ_SIZE 0x318u

/* Retail 800AD8B8: command IDs (DoorAutoMove=0x0C, DoorOpen=0x15)
 * are not the stored state. All door substates remain in mode2/state0x20. */
static inline int LmDoorStateBusy(unsigned int mode, unsigned int state) {
    return mode == 2u && state == 0x20u;
}

static inline int LmDoorRangeInside(unsigned int address, unsigned int size,
                                    unsigned int gameStart,
                                    unsigned int gameEnd) {
    return gameStart >= 0x80003100u && gameEnd <= 0x81800000u &&
        gameStart < gameEnd && ((gameStart | gameEnd | address) & 3u) == 0u &&
        size != 0u && address >= gameStart && address <= gameEnd &&
        size <= gameEnd - address;
}

#endif
