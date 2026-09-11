#ifndef LM_COLOUR_HXX
#define LM_COLOUR_HXX

#include "Dolphin/types.h"

namespace LMColour {

// Tick only after the completed frame's GXDrawDone barrier.
void tick();
bool enabled();
void setEnabled(bool enabled);
u32 rgb();
void setRgb(u32 rgb);
u32 presetCount();
const char *presetName(u32 index);
void applyPreset(u32 index);
const char *statusText();

}  // namespace LMColour

#endif
