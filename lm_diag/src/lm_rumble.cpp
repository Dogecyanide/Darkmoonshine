#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_rumble.hxx"
#include "susamune/lm_rumble.h"

namespace {
static_assert(sizeof(unsigned int) == 4u && sizeof(u32) == 4u,
              "LM rumble native ABI requires 32-bit words");
unsigned int readWord(unsigned int address) {
    return *reinterpret_cast<volatile const unsigned int*>(address);
}
void writeWord(unsigned int address, unsigned int value) {
    *reinterpret_cast<volatile unsigned int*>(address) = value;
}
}

void LMRumble::afterLoad(u32 gameStart, u32 gameEnd, u32 systemHeap,
                         u32 systemStart, u32 systemEnd) {
    LmRumbleAfterLoad(readWord, writeWord,
        reinterpret_cast<LmRumbleReset>(LM_RUMBLE_RESET_ADDRESS),
        reinterpret_cast<LmRumbleMotor>(LM_RUMBLE_MOTOR_ADDRESS),
        gameStart, gameEnd, systemHeap, systemStart, systemEnd);
}

#endif
