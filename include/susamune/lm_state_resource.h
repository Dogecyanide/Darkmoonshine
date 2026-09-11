#ifndef SUSAMUNE_LM_STATE_RESOURCE_H
#define SUSAMUNE_LM_STATE_RESOURCE_H

#define LM_STATE_RESOURCE_SLOT_COUNT 7u
#define LM_STATE_RESOURCE_RECORD_SIZE 0x40u

static inline unsigned long LmStateResourceWord(const unsigned char *record,
                                               unsigned int offset) {
    return ((unsigned long)record[offset] << 24) |
           ((unsigned long)record[offset + 1u] << 16) |
           ((unsigned long)record[offset + 2u] << 8) | record[offset + 3u];
}

static inline int LmStateSettledRoomRecord(unsigned long id,
    unsigned int slot, unsigned long backing, unsigned long heapStart,
    unsigned long heapEnd, const unsigned char *record) {
    unsigned int offset;
    if (id >= 128u || slot >= LM_STATE_RESOURCE_SLOT_COUNT ||
        record[2] != 2u ||
        LmStateResourceWord(record, 4u) != 0x80011B28u ||
        LmStateResourceWord(record, 8u) != 0u ||
        LmStateResourceWord(record, 0x18u) != backing ||
        LmStateResourceWord(record, 0x1Cu) != slot ||
        LmStateResourceWord(record, 0x20u) != id ||
        heapStart < 0x80000000u || heapEnd > 0x81800000u ||
        heapStart >= heapEnd || heapEnd - heapStart < 4u ||
        backing < heapStart || backing >= heapEnd) return 0;
    for (offset = 0xCu; offset <= 0x14u; offset += 4u) {
        const unsigned long pointer = LmStateResourceWord(record, offset);
        const unsigned int count = record[offset == 0xCu ? 0x3Cu : 0x3Du];
        const unsigned int stride = offset == 0xCu ? 0x7Cu :
                                    (offset == 0x10u ? 0x24u : 4u);
        const unsigned int bytes = count != 0u ? count * stride : 4u;
        const unsigned int prefix = offset == 0x14u ? 0u : 8u;
        if (pointer == 0u) {
            if (count != 0u) return 0;
        } else if ((pointer & 3u) != 0u || bytes > heapEnd - heapStart ||
                   pointer < heapStart + prefix || pointer > heapEnd - bytes)
            return 0;
    }
    for (offset = 0x24u; offset < 0x3Cu; offset += 4u)
        if (LmStateResourceWord(record, offset) != 0u) return 0;
    return 1;
}

static inline int LmStateIdleResourceRecordEquivalent(
    unsigned int savedId, unsigned int liveId,
    const unsigned char *saved, const unsigned char *live) {
    unsigned int i;
    if (savedId != 0xFFFFFFFFu || liveId != 0xFFFFFFFFu ||
        saved[2] != 0u || live[2] != 0u) return 0;
    for (i = 8u; i < 0x18u; ++i) {
        if (saved[i] != 0u || live[i] != 0u) return 0;
    }
    /* Constructor/cleanup leave +3/+3E/+3F padding uninitialized, including
     * across record-array relocation. Callback/backing are stale while idle
     * and fn_8001F2B4 replaces both before reuse. All semantic bytes still match. */
    for (i = 0u; i < LM_STATE_RESOURCE_RECORD_SIZE; ++i) {
        if (i == 3u || i >= 0x3Eu ||
            (i >= 4u && i < 8u) || (i >= 0x18u && i < 0x1Cu)) continue;
        if (saved[i] != live[i]) return 0;
    }
    return 1;
}

static inline int LmStateResourceChangesMatch(
    unsigned int activeMask, unsigned int recordMask,
    const unsigned long *savedIds, const unsigned long *liveIds,
    const unsigned char *savedRecords, const unsigned char *liveRecords) {
    const unsigned int slotMask = (1u << LM_STATE_RESOURCE_SLOT_COUNT) - 1u;
    unsigned int i;
    if (((activeMask | recordMask) & ~slotMask) != 0u ||
        (activeMask & ~recordMask) != 0u) return 0;
    for (i = 0u; i < LM_STATE_RESOURCE_SLOT_COUNT; ++i) {
        const unsigned int bit = 1u << i;
        if ((recordMask & ~activeMask & bit) != 0u &&
            !LmStateIdleResourceRecordEquivalent(
                savedIds[i], liveIds[i],
                savedRecords + i * LM_STATE_RESOURCE_RECORD_SIZE,
                liveRecords + i * LM_STATE_RESOURCE_RECORD_SIZE)) return 0;
    }
    return 1;
}

static inline int LmStateResourceReloadChangesMatch(
    unsigned int activeMask, unsigned int recordMask,
    const unsigned long *savedIds, const unsigned long *liveIds,
    const unsigned char *savedRecords, const unsigned char *liveRecords,
    unsigned long savedBulk, unsigned long liveBulk, unsigned long slotSize,
    unsigned long heapStart, unsigned long heapEnd) {
    const unsigned int slotMask = (1u << LM_STATE_RESOURCE_SLOT_COUNT) - 1u;
    unsigned int i;
    if (((activeMask | recordMask) & ~slotMask) != 0u ||
        (activeMask & ~recordMask) != 0u || slotSize != 0x70800u ||
        savedBulk < heapStart || liveBulk < heapStart ||
        heapEnd <= heapStart || heapEnd - heapStart < slotSize * 7u ||
        savedBulk > heapEnd - slotSize * 7u ||
        liveBulk > heapEnd - slotSize * 7u) return 0;
    for (i = 0u; i < LM_STATE_RESOURCE_SLOT_COUNT; ++i) {
        const unsigned int bit = 1u << i;
        const unsigned char *saved = savedRecords + i * LM_STATE_RESOURCE_RECORD_SIZE;
        const unsigned char *live = liveRecords + i * LM_STATE_RESOURCE_RECORD_SIZE;
        if ((recordMask & ~activeMask & bit) == 0u) continue;
        if (LmStateIdleResourceRecordEquivalent(savedIds[i], liveIds[i], saved, live))
            continue;
        /* Reloading the same room keeps the logical ID but rebuilds its
         * completed GAME-owned arrays. Validate both endpoints explicitly. */
        if (savedIds[i] != liveIds[i] ||
            !LmStateSettledRoomRecord(savedIds[i], i, savedBulk + i * slotSize,
                                      heapStart, heapEnd, saved) ||
            !LmStateSettledRoomRecord(liveIds[i], i, liveBulk + i * slotSize,
                                      heapStart, heapEnd, live)) return 0;
    }
    return 1;
}

#endif
