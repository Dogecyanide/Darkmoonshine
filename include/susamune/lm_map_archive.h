#ifndef SUSAMUNE_LM_MAP_ARCHIVE_H
#define SUSAMUNE_LM_MAP_ARCHIVE_H

#include "lm_state_roots.h"

#define LM_MAP_ARCHIVE_MAX_BLOCKS 8192u

typedef struct {
    unsigned int mission, archive, backing, bytes, heap;
    unsigned int start, end, usedHead, usedTail;
} LmMapArchiveRoots;

/* MissionMode owns this mount directly; it is not a model-table row. The
 * complete owner, wrapper and RARC are ordinary group-B GAME allocations. */
static inline int LmMapArchiveCheck(LmStateRootsReader* r,
    const LmMapArchiveRoots* roots) {
    unsigned int bases[3], sizes[3], starts[3];
    unsigned int i, j, node, previous = 0u, count = 0u, found = 0u;
    if (!roots || roots->start < 0x80000000u || roots->end > 0x81800000u ||
        roots->start >= roots->end || ((roots->start | roots->end) & 3u)) return 0;
    bases[0] = roots->mission;
    bases[1] = roots->archive;
    bases[2] = roots->backing;
    sizes[0] = 0x24u;
    sizes[1] = 0x68u;
    sizes[2] = roots->bytes;
    if (sizes[2] < 0x40u || (sizes[2] & 31u) || (bases[2] & 31u)) return 0;
    for (i = 0u; i < 3u; ++i) {
        unsigned int padding;
        r->address = bases[i];
        r->value = sizes[i];
        if (!LmStateRootInside(bases[i], sizes[i], roots->start + 16u, roots->end) ||
            !LmStateRootsRead(r, bases[i] - 16u)) return 0;
        padding = (r->value >> 8u) & 0x7Fu;
        if ((padding & 3u) || padding > bases[i] - 16u - roots->start) return 0;
        starts[i] = bases[i] - 16u - padding;
        for (j = 0u; j < i; ++j)
            if (starts[i] < bases[j] + sizes[j] && starts[j] < bases[i] + sizes[i]) return 0;
    }
    if (!LmStateRootsExpected(r, 0x804A17C8u, bases[0]) ||
        !LmStateRootsExpected(r, 0x804A17B0u, bases[0]) ||
        !LmStateRootsExpected(r, bases[0], 0x8034F080u) ||
        !LmStateRootsExpected(r, bases[0] + 0x18u, bases[1]) ||
        !LmStateRootsExpected(r, bases[1], 0x80388D5Cu) ||
        !LmStateRootsExpected(r, bases[1] + 4u, roots->heap) ||
        !LmStateRootsExpected(r, bases[1] + 0x2Cu, 0x52415243u) ||
        !LmStateRootsExpected(r, bases[1] + 0x38u, roots->heap) ||
        !LmStateRootsExpected(r, bases[1] + 0x40u, bases[2]) ||
        !LmStateRootsExpected(r, bases[1] + 0x44u, bases[2] + 0x20u) ||
        !LmStateRootsExpected(r, bases[1] + 0x5Cu, bases[2]) ||
        !LmStateRootsExpected(r, bases[2], 0x52415243u) ||
        !LmStateRootsExpected(r, bases[2] + 4u, sizes[2]) ||
        !LmStateRootsExpected(r, bases[2] + 8u, 0x20u) ||
        !LmStateRootsRead(r, bases[2] + 0xCu) || r->value > sizes[2] - 0x20u ||
        !LmStateRootsExpected(r, bases[1] + 0x60u, bases[2] + 0x20u + r->value)) return 0;
    node = roots->usedHead;
    while (node) {
        unsigned int tag, padding, bytes, next;
        r->address = previous ? previous + 12u : roots->start;
        r->value = node;
        if (++count > LM_MAP_ARCHIVE_MAX_BLOCKS ||
            !LmStateRootInside(node, 16u, roots->start, roots->end) ||
            !LmStateRootsRead(r, node)) return 0;
        tag = r->value;
        padding = (tag >> 8u) & 0x7Fu;
        if ((tag >> 16u) != 0x484Du || (padding & 3u) || padding > node - roots->start ||
            !LmStateRootsRead(r, node + 4u)) return 0;
        bytes = r->value;
        if ((bytes & 3u) || bytes > roots->end - node - 16u ||
            !LmStateRootsExpected(r, node + 8u, previous) ||
            !LmStateRootsRead(r, node + 12u)) return 0;
        next = r->value;
        for (i = 0u; i < 3u; ++i) {
            if (node == bases[i] - 16u) {
                r->address = node;
                r->value = tag;
                if (bytes != sizes[i] || (tag & 0xFFu) != 0x0Bu ||
                    node - padding != starts[i]) return 0;
                found |= 1u << i;
            } else if (node - padding < bases[i] + sizes[i] &&
                       starts[i] < node + 16u + bytes) return 0;
        }
        previous = node;
        node = next;
    }
    r->address = roots->usedTail;
    r->value = previous;
    return previous == roots->usedTail && found == 7u;
}

/* Reader, heap list and owner endpoints must all refer to the same image. This
 * only accounts for the map mount; every other restore guard remains required. */
static inline int LmMapArchiveValidate(void* context, LmStateRootsReadWord read,
    const LmMapArchiveRoots* roots, unsigned int* fault, unsigned int* value) {
    LmStateRootsReader reader;
    int valid;
    reader.context = context;
    reader.read = read;
    reader.address = reader.value = 0u;
    valid = read && LmMapArchiveCheck(&reader, roots);
    if (fault) *fault = valid ? 0u : reader.address;
    if (value) *value = valid ? 0u : reader.value;
    return valid;
}
#endif
