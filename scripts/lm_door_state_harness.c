#include <susamune/lm_door_state.h>

int busy(unsigned int mode, unsigned int state) {
    return LmDoorStateBusy(mode, state);
}

int validRange(unsigned int address, unsigned int size,
               unsigned int start, unsigned int end) {
    return LmDoorRangeInside(address, size, start, end);
}
