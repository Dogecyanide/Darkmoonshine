#ifndef LM_TOOLS_HXX
#define LM_TOOLS_HXX
#include "Dolphin/types.h"
struct PADStatus;
namespace LMTools {
void samplePad(const PADStatus &pad);
void tick(bool menuOpen);
void draw(void *directPrint, void *xfb);
u32 rows(bool timingPage);
bool action(bool timingPage, u32 row, s32 direction);
void drawPage(void *directPrint, bool timingPage, u32 row, u16 top);
void writePreferences(unsigned int values[48]);
void readPreferences(const unsigned int values[48], unsigned int presentLo, unsigned int presentHi);
}
#endif
