#ifndef LM_ELEMENTS_HXX
#define LM_ELEMENTS_HXX

#include "Dolphin/types.h"

namespace LMElements {
enum class Result {
    Applied = 0, Busy = 1, Invalid = 3,
    Warping = 4, NotReady = 5, Event = 6, PlayerInactive = 7
};
constexpr u32 kChoiceCount = 4u;
const char *name(u32 choice);
const char *statusText(Result result);
Result apply(u32 choice, u32 player);
}
#endif
