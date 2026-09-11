#ifndef LM_INPUT_DISPLAY_HXX
#define LM_INPUT_DISPLAY_HXX
#include "Dolphin/types.h"
struct PADStatus;
namespace LMInputDisplay {
void draw(void *xfb, u16 width, u16 height, const PADStatus &pad);
}
#endif
