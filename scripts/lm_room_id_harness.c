#include "susamune/lm_room_id.h"

int player_room(unsigned int packedRoom) {
    return LmPlayerRoomId(packedRoom);
}
