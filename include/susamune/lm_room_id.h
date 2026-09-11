#ifndef SUSAMUNE_LM_ROOM_ID_H
#define SUSAMUNE_LM_ROOM_ID_H

/* GLMJ01 800DB98C/90 extracts this byte before the room-table lookup.
 * 80092318..24 treats the complete FFFFFFFF word as the unset sentinel. */
static inline int LmPlayerRoomId(unsigned int packedRoom) {
    return packedRoom == 0xFFFFFFFFu ? -1 : (int)(packedRoom & 0xFFu);
}

#endif
