#ifndef SUSAMUNE_LM_ARCHIVE_GUARD_H
#define SUSAMUNE_LM_ARCHIVE_GUARD_H

#include "lm_shared_archive.h"

#define LM_ARCHIVE_GUARD_VTABLE 0x01u
#define LM_ARCHIVE_GUARD_NODE 0x02u
#define LM_ARCHIVE_GUARD_OBJECT_LOCATION 0x04u
#define LM_ARCHIVE_GUARD_BACKING_LOCATION 0x08u
#define LM_ARCHIVE_GUARD_FLAGS 0x10u
#define LM_ARCHIVE_GUARD_FILE_LENGTH 0x20u
#define LM_ARCHIVE_GUARD_BACKING_RANGE 0x40u
#define LM_ARCHIVE_GUARD_REQUIRED_FLAGS 0x106u
#define LM_ARCHIVE_GUARD_GAME_OWNER 1u

struct LmArchiveGuardEntry {
    unsigned int node, object, vtable, stateFlags;
    unsigned int archiveHeader, fileLength, ownerFlags;
};
typedef char lm_archive_guard_word_size_check[sizeof(unsigned int) == 4 ? 1 : -1];

/* Census construction already validates each object/link. Preserve that proof:
 * allocator-pointer owner nibbles are not the captured byte-range owners. */
static inline unsigned int LmArchiveGuardReasons(const struct LmArchiveGuardEntry *entry,
    unsigned int gameStart, unsigned int gameEnd, unsigned int expectedVtable) {
    unsigned int reasons = 0u;
    if (entry->vtable != expectedVtable) reasons |= LM_ARCHIVE_GUARD_VTABLE;
    if (entry->node != entry->object + 0x18u) reasons |= LM_ARCHIVE_GUARD_NODE;
    if (((entry->ownerFlags >> 8u) & 15u) != LM_ARCHIVE_GUARD_GAME_OWNER)
        reasons |= LM_ARCHIVE_GUARD_OBJECT_LOCATION;
    if (((entry->ownerFlags >> 12u) & 15u) != LM_ARCHIVE_GUARD_GAME_OWNER)
        reasons |= LM_ARCHIVE_GUARD_BACKING_LOCATION;
    if ((entry->stateFlags & LM_ARCHIVE_GUARD_REQUIRED_FLAGS) != LM_ARCHIVE_GUARD_REQUIRED_FLAGS)
        reasons |= LM_ARCHIVE_GUARD_FLAGS;
    if (entry->fileLength < 0x20u) reasons |= LM_ARCHIVE_GUARD_FILE_LENGTH;
    if (entry->fileLength == 0u || entry->fileLength > 0x01800000u ||
        entry->archiveHeader < 0x80000000u ||
        entry->archiveHeader > 0x81800000u - entry->fileLength ||
        entry->archiveHeader < gameStart ||
        entry->archiveHeader + entry->fileLength > gameEnd)
        reasons |= LM_ARCHIVE_GUARD_BACKING_RANGE;
    return reasons;
}

/* Parent must be freshly validated and equal to the captured companion owner.
 * Only the two backing reasons may be removed; keep every other guard reason. */
static inline int LmArchiveGuardSharedBackingAllowed(const struct LmArchiveGuardEntry *entry,
    unsigned int objectOwnerHeap, unsigned int archiveHeap, unsigned int type,
    unsigned int mountSource, unsigned int gameHeap,
    const LmSharedArchiveDescriptor *parent) {
    return entry && parent && gameHeap != parent->systemHeap &&
        (entry->ownerFlags & 0xFFFFu) == 0x2121u &&
        objectOwnerHeap == gameHeap && archiveHeap == parent->systemHeap &&
        type == 0x52415243u && mountSource == entry->archiveHeader &&
        ((entry->stateFlags >> 16u) & 15u) == 1u &&
        (entry->stateFlags & 0x01000000u) != 0u &&
        LmSharedArchiveContains(parent, entry->archiveHeader, entry->fileLength);
}

/* The mask retains all failures; this value follows the old predicate's first
 * failing condition. Location/range failures carry the exact endpoint address. */
static inline unsigned int LmArchiveGuardFailureValue(const struct LmArchiveGuardEntry *entry,
    unsigned int reasons) {
    if (reasons & LM_ARCHIVE_GUARD_VTABLE) return entry->vtable;
    if (reasons & LM_ARCHIVE_GUARD_NODE) return entry->node;
    if (reasons & LM_ARCHIVE_GUARD_OBJECT_LOCATION) return entry->object;
    if (reasons & LM_ARCHIVE_GUARD_BACKING_LOCATION) return entry->archiveHeader;
    if (reasons & LM_ARCHIVE_GUARD_FLAGS) return entry->stateFlags;
    if (reasons & LM_ARCHIVE_GUARD_FILE_LENGTH) return entry->fileLength;
    if (reasons & LM_ARCHIVE_GUARD_BACKING_RANGE) return entry->archiveHeader;
    return 0u;
}

#endif
