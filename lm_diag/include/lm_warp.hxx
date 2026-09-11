#ifndef LM_WARP_HXX
#define LM_WARP_HXX

#include "Dolphin/types.h"

namespace LMWarp {

u32 count();
const char *name(u32 index);
u32 map(u32 index);
u32 room(u32 index);
bool hasBooSafePoint(u32 index);

// Requests and tick run on the ordinary game thread. The retail scene loader
// consumes the request and constructs Luigi from a retained ToolData row.
bool request(u32 index, bool booSafe = false);
bool requestRoomReload(bool clear, bool booSafe = false);
bool roomReloadAvailable(u32 room);
void tick();
bool active();
const char *statusText();
u32 actualRoom();

}  // namespace LMWarp

#endif
