#pragma once

#include "Dolphin/types.h"

namespace LMCrash {

void init();
void note(u32 event, u32 arg0 = 0, u32 arg1 = 0);
// SAVE/7F and WARP also enter the bounded critical queue when supported.
void phase(u32 action, u32 phase, u32 arg0 = 0, u32 arg1 = 0);

}  // namespace LMCrash
