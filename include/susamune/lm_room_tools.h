#ifndef SUSAMUNE_LM_ROOM_TOOLS_H
#define SUSAMUNE_LM_ROOM_TOOLS_H

#define LM_ROOM_TABLE_LIMIT 74u
#define LM_ROOM_RESET_FLAG_LIMIT 11u
#define LM_ROOM_RESET_EVENT_LIMIT 2u
#define LM_ROOM_NO_PARTNER 0xFFFFFFFFu

/* The high byte selects a loaded table row; the low byte is the persistent
 * room ID. They differ in most mansion rooms (Parlor is row36 / room35). */
static inline int LmRoomRecordMatches(unsigned int player,
                                     unsigned int manager,
                                     unsigned int record,
                                     unsigned int count) {
    return count > 0u && count <= LM_ROOM_TABLE_LIMIT &&
        player != 0xFFFFFFFFu && manager != 0xFFFFFFFFu &&
        record != 0xFFFFFFFFu && (manager >> 24u) < count &&
        (player >> 24u) == (manager >> 24u) &&
        (record >> 24u) == (manager >> 24u) &&
        (player & 255u) == (record & 255u) &&
        (manager & 255u) == (record & 255u) &&
        (record & 255u) < LM_ROOM_TABLE_LIMIT;
}

struct LmRoomFlagEdit { unsigned char id, value; };
struct LmRoomResetRecipe {
    unsigned int count;
    unsigned int darken;
    struct LmRoomFlagEdit edits[LM_ROOM_RESET_FLAG_LIMIT];
    unsigned int eventCount;
    unsigned char events[LM_ROOM_RESET_EVENT_LIMIT];
};

/* Retail links logical room 2 to the row held in the foyer-partner global.
 * Practice commands are symmetric: the upstairs selection must update room 2
 * too. Never interpret a row index as a logical persistence ID. */
static inline unsigned int LmRoomFoyerCompanion(
    unsigned int room, unsigned int partnerIndex, unsigned int count,
    unsigned int partnerRecord) {
    unsigned int partner = partnerRecord & 255u;
    if (room >= LM_ROOM_TABLE_LIMIT || !count || count > LM_ROOM_TABLE_LIMIT ||
        partnerIndex >= count || partnerRecord == 0xFFFFFFFFu ||
        (partnerRecord >> 24u) != partnerIndex ||
        partner >= LM_ROOM_TABLE_LIMIT || partner == 2u) return LM_ROOM_NO_PARTNER;
    if (room == 2u) return partner;
    return room == partner ? 2u : LM_ROOM_NO_PARTNER;
}

static inline int LmRoomResetRecipeFor(unsigned int room,
                                      unsigned int shownMarioItems,
                                      struct LmRoomResetRecipe *out) {
    static const unsigned char resets[][2] = {
        {3u,13u}, {14u,41u}, {14u,19u},
        {16u,74u}, {16u,59u}, {16u,67u},
        {24u,46u}, {24u,47u}, {24u,89u}, {25u,51u}, {25u,37u}, {25u,65u},
        {28u,25u}, {28u,18u}, {28u,38u}, {28u,36u},
        {35u,14u}, {35u,8u}, {40u,83u}, {40u,6u}, {40u,198u},
        {41u,49u}, {41u,50u}, {41u,52u}, {55u,40u},
        {57u,31u}, {57u,177u}, {57u,178u}, {57u,179u},
        {57u,180u}, {57u,181u}, {57u,182u}, {57u,183u}, {57u,55u},
        {59u,81u}, {61u,28u}, {66u,63u}, {66u,64u}, {70u,66u},
    };
    unsigned int i;
    if (!out || room >= LM_ROOM_TABLE_LIMIT) return 0;
    out->count = 0u;
    out->darken = room != 70u;
    out->eventCount = 0u;
    /* EventLoad=1 is a lifetime play limit. Story flags alone do not rearm it.
     * These IDs are authenticated against clean JP EventInfo and scripts. */
    if (room == 24u) {
        out->events[out->eventCount++] = 22u;
        /* The native event50 warp leads to boss map10's one-shot intro. */
        out->events[out->eventCount++] = 64u;
    }
    if (room == 35u) out->events[out->eventCount++] = 61u;
    if (room == 40u) out->events[out->eventCount++] = 76u;
    for (i = 0u; i < sizeof(resets) / sizeof(resets[0]); ++i) {
        if (resets[i][0] != room) continue;
        out->edits[out->count].id = resets[i][1];
        out->edits[out->count++].value = 0u;
    }
    /* These are already-owned items shown to Madame Clairvoya, not grants. */
    if (room == 3u) {
        for (i = 0u; i < 5u; ++i) {
            if (!(shownMarioItems & (1u << i))) continue;
            out->edits[out->count].id = (unsigned char)(21u + i * 3u);
            out->edits[out->count++].value = 1u;
            out->edits[out->count].id = (unsigned char)(20u + i * 3u);
            out->edits[out->count++].value = 0u;
        }
    }
    return 1;
}

static inline unsigned short LmRoomDarkPersistence(unsigned short before) {
    return (unsigned short)(before & ~2u);
}

#endif
