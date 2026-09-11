#ifndef LM_PRACTICE_HXX
#define LM_PRACTICE_HXX

#include "Dolphin/types.h"

struct PADStatus;

namespace LMPractice {

// Called immediately after the one retail PADRead. The menu keeps the physical
// port-1 sample and gives JUTGamePad a neutral sample while it owns input.
void filterPadRead(PADStatus *statuses);
void tick();
void draw(void *directPrint, void *xfb);
bool isOpen();
void writePreferences(unsigned int values[48]);
void readPreferences(const unsigned int values[48], unsigned int presentLo, unsigned int presentHi);

// Called by the queued warp path before publication, then after acceptance.
bool prepareRoomReload(u32 room, bool clear);
void commitRoomReload(u32 room, bool clear);

}  // namespace LMPractice

#endif  // LM_PRACTICE_HXX
