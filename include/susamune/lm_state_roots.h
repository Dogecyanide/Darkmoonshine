#ifndef SUSAMUNE_LM_STATE_ROOTS_H
#define SUSAMUNE_LM_STATE_ROOTS_H

#include "susamune/crash_report.h"

typedef char LmStateRootsWordMustBe32Bits[sizeof(unsigned int) == 4 ? 1 : -1];
typedef int (*LmStateRootsReadWord)(void* context, unsigned int address,
                                   unsigned int* value);

#define LM_STATE_RELOCATABLE_GAME_ROOT_MASK \
    (SUSAMUNE_LM_EPOCH_MISSION_MODE | SUSAMUNE_LM_EPOCH_GAME_MODE | \
     SUSAMUNE_LM_EPOCH_SIMPLE_MODELER | SUSAMUNE_LM_EPOCH_MAP_COL | \
     SUSAMUNE_LM_EPOCH_EN_TYPES)
#define LM_STATE_GUARDED_GAME_EPOCH_MASK \
    (LM_STATE_RELOCATABLE_GAME_ROOT_MASK | SUSAMUNE_LM_EPOCH_VOLUME_COUNT | \
     SUSAMUNE_LM_EPOCH_VOLUME_HEAD)

typedef struct {
    unsigned int missionMode, mapArchive, gameMode;
    unsigned int simpleModeler, mapCol, enTypesManager;
} LmStateGameRoots;

typedef struct {
    unsigned int recordBase, bulkBase, slotCount, slotSize;
} LmStateResourceRoots;

static inline int LmStateGameEpochMaskAllowed(unsigned int mask) {
    return mask != 0u && (mask & ~LM_STATE_GUARDED_GAME_EPOCH_MASK) == 0u;
}

static inline int LmStateGameRootOnlyEpoch(unsigned int mask) {
    return mask != 0u && (mask & ~LM_STATE_RELOCATABLE_GAME_ROOT_MASK) == 0u;
}

typedef struct {
    void* context;
    LmStateRootsReadWord read;
    unsigned int address, value;
} LmStateRootsReader;

static inline int LmStateRootsRead(LmStateRootsReader* reader,
                                   unsigned int address) {
    reader->address = address;
    reader->value = 0u;
    return reader->read(reader->context, address, &reader->value);
}

static inline int LmStateRootsExpected(LmStateRootsReader* reader,
    unsigned int address, unsigned int expected) {
    return LmStateRootsRead(reader, address) && reader->value == expected;
}

static inline int LmStateRootInside(unsigned int pointer, unsigned int size,
    unsigned int start, unsigned int end) {
    return (pointer & 3u) == 0u && pointer >= start && pointer < end &&
           size != 0u && size <= end - pointer;
}

static inline int LmStateRootsDisjoint(const unsigned int* bases,
    const unsigned int* sizes, unsigned int count) {
    unsigned int i, j;
    for (i = 0u; i < count; ++i)
        for (j = 0u; j < i; ++j)
            if (bases[i] < bases[j] + sizes[j] &&
                bases[j] < bases[i] + sizes[i]) return 0;
    return 1;
}

/* GLMJ01 scene-owned objects, not arbitrary MEM1 roots. All ranges are checked
 * before dereferencing them; GameMode aliases MissionMode, never a sixth owner.
 * This proves only the relocated root graph, not overall restore eligibility. */
static inline int LmStateGameRootsCheck(LmStateRootsReader* reader,
    const LmStateGameRoots* roots, unsigned int start, unsigned int end) {
    const unsigned int sizes[7] = {0x24u, 0x68u, 0xC10u, 0x14u, 8u,
                                  0xE48u, 0x31DC0u};
    const unsigned int globals[5] = {0x804A17C8u, 0u, 0x804A17D0u,
                                     0x804A17D8u, 0x804A17E8u};
    const unsigned int vtables[6] = {0x8034F080u, 0x80388D5Cu,
        0x8034F0CCu, 0x8034F180u, 0x803560A8u, 0x80358D68u};
    unsigned int bases[7], i;
    bases[0] = roots->missionMode;
    bases[1] = roots->mapArchive;
    bases[2] = roots->simpleModeler;
    bases[3] = roots->mapCol;
    bases[4] = roots->enTypesManager;
    for (i = 0u; i < 5u; ++i) {
        reader->address = globals[i];
        reader->value = bases[i];
        if (!LmStateRootInside(bases[i], sizes[i], start, end)) return 0;
    }
    if (!LmStateRootsDisjoint(bases, sizes, 5u)) return 0;
    reader->address = 0x804A17B0u;
    reader->value = roots->gameMode;
    if (roots->gameMode != roots->missionMode) return 0;
    if (!LmStateRootsExpected(reader, 0x804A17B0u, roots->gameMode)) return 0;
    for (i = 0u; i < 5u; ++i) {
        if (globals[i] && !LmStateRootsExpected(reader, globals[i], bases[i]))
            return 0;
        if (!LmStateRootsExpected(reader, bases[i], vtables[i])) return 0;
    }
    if (!LmStateRootsExpected(reader, bases[0] + 4u, bases[4]) ||
        !LmStateRootsExpected(reader, bases[0] + 0x18u, bases[1]) ||
        !LmStateRootsRead(reader, bases[0] + 8u)) return 0;
    bases[5] = reader->value;
    if (!LmStateRootInside(bases[5], sizes[5], start, end) ||
        !LmStateRootsDisjoint(bases, sizes, 6u) ||
        !LmStateRootsExpected(reader, bases[5], vtables[5]) ||
        !LmStateRootsRead(reader, bases[4] + 4u)) return 0;
    if (reader->value < 8u) return 0;
    bases[6] = reader->value - 8u;
    if (!LmStateRootInside(bases[6], sizes[6], start, end) ||
        !LmStateRootsDisjoint(bases, sizes, 7u)) return 0;
    return LmStateRootsExpected(reader, bases[6], 0x218u) &&
           LmStateRootsExpected(reader, bases[6] + 4u, 0x17Du);
}

/* Call independently for the authenticated saved bytes and live bytes.
 * A saved reader must never fall back to live memory for an absent range.
 * The caller still requires unchanged map/archive/scene/audio and SYS owners,
 * quiescence, complete volume/resource/model proofs, and matching heap extents. */
static inline int LmStateGameRootsValidate(void* context,
    LmStateRootsReadWord read, const LmStateGameRoots* roots,
    unsigned int heapStart, unsigned int heapEnd,
    unsigned int* faultAddress, unsigned int* faultValue) {
    LmStateRootsReader reader;
    int valid = 0;
    reader.context = context;
    reader.read = read;
    reader.address = reader.value = 0u;
    if (read && roots && heapStart >= 0x80000000u &&
        heapEnd <= 0x81800000u && heapStart < heapEnd &&
        ((heapStart | heapEnd) & 3u) == 0u)
        valid = LmStateGameRootsCheck(&reader, roots, heapStart, heapEnd);
    if (faultAddress) *faultAddress = valid ? 0u : reader.address;
    if (faultValue) *faultValue = valid ? 0u : reader.value;
    return valid;
}

/* The retail scene reload recreates both allocations, but never changes the
 * seven-slot schema. These checks do not substitute for settled record states,
 * an unchanged room map, or the volume/model ownership proof. */
static inline int LmStateResourceRootsCheck(LmStateRootsReader* reader,
    const LmStateResourceRoots* roots, unsigned int start, unsigned int end) {
    const unsigned int sizes[2] = {7u * 0x40u + 8u, 7u * 0x70800u};
    unsigned int bases[2], i;
    reader->address = 0x804A0D10u;
    reader->value = roots->slotCount;
    if (roots->slotCount != 7u) return 0;
    reader->address = 0x804A0D14u;
    reader->value = roots->slotSize;
    if (roots->slotSize != 0x70800u) return 0;
    reader->address = 0x804A0D08u;
    reader->value = roots->recordBase;
    if (roots->recordBase < 8u) return 0;
    bases[0] = roots->recordBase - 8u;
    if (!LmStateRootInside(bases[0], sizes[0], start, end)) return 0;
    reader->address = 0x804A0D0Cu;
    reader->value = roots->bulkBase;
    bases[1] = roots->bulkBase;
    if ((bases[1] & 31u) != 0u ||
        !LmStateRootInside(bases[1], sizes[1], start, end) ||
        !LmStateRootsDisjoint(bases, sizes, 2u)) return 0;
    if (!LmStateRootsExpected(reader, 0x804A0D08u, roots->recordBase) ||
        !LmStateRootsExpected(reader, 0x804A0D0Cu, roots->bulkBase) ||
        !LmStateRootsRead(reader, 0x804A0D10u) ||
        (reader->value >> 24) != 7u ||
        !LmStateRootsExpected(reader, 0x804A0D14u, 0x70800u) ||
        !LmStateRootsExpected(reader, bases[0], 0x40u) ||
        !LmStateRootsExpected(reader, bases[0] + 4u, 7u)) return 0;
    for (i = 0u; i < 7u; ++i)
        if (!LmStateRootsExpected(reader, 0x80398ECCu + i * 4u,
                                  roots->bulkBase + i * 0x70800u)) return 0;
    return 1;
}

static inline int LmStateResourceRootsValidate(void* context,
    LmStateRootsReadWord read, const LmStateResourceRoots* roots,
    unsigned int heapStart, unsigned int heapEnd,
    unsigned int* faultAddress, unsigned int* faultValue) {
    LmStateRootsReader reader;
    int valid = 0;
    reader.context = context;
    reader.read = read;
    reader.address = reader.value = 0u;
    if (read && roots && heapStart >= 0x80000000u &&
        heapEnd <= 0x81800000u && heapStart < heapEnd &&
        ((heapStart | heapEnd) & 3u) == 0u)
        valid = LmStateResourceRootsCheck(&reader, roots, heapStart, heapEnd);
    if (faultAddress) *faultAddress = valid ? 0u : reader.address;
    if (faultValue) *faultValue = valid ? 0u : reader.value;
    return valid;
}

#endif
