#include "susamune/lm_room_tools.h"

int roomMatches(unsigned int player, unsigned int manager,
                unsigned int record, unsigned int count) {
    return LmRoomRecordMatches(player, manager, record, count);
}

int resetRecipe(unsigned int room, unsigned int items, unsigned int *result) {
    struct LmRoomResetRecipe recipe;
    unsigned int i;
    if (!LmRoomResetRecipeFor(room, items, &recipe)) return 0;
    result[0] = recipe.count;
    result[1] = recipe.darken;
    for (i = 0u; i < recipe.count; ++i) {
        result[2u + i * 2u] = recipe.edits[i].id;
        result[3u + i * 2u] = recipe.edits[i].value;
    }
    return 1;
}

unsigned int darkPersistence(unsigned int before) {
    return LmRoomDarkPersistence((unsigned short)before);
}

unsigned int foyerCompanion(unsigned int room, unsigned int index,
                            unsigned int count, unsigned int record) {
    return LmRoomFoyerCompanion(room, index, count, record);
}

unsigned int resetEvent(unsigned int room, unsigned int index) {
    struct LmRoomResetRecipe recipe;
    if (!LmRoomResetRecipeFor(room, 0u, &recipe)) return 0xFFFFFFFFu;
    return index < recipe.eventCount ? recipe.events[index] : 0xFFFFFFFFu;
}
