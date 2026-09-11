#ifndef SUSAMUNE_LM_SHARED_ARCHIVE_H
#define SUSAMUNE_LM_SHARED_ARCHIVE_H

#define LM_SHARED_ARCHIVE_SIZE 0x419B00u
#define LM_SHARED_ARCHIVE_DATA_OFFSET 0x6E20u
#define LM_SHARED_ARCHIVE_METADATA_SIGNATURE 0x94D08ED2u
#define LM_SHARED_ARCHIVE_MAX_BLOCKS 128u

typedef int (*LmSharedArchiveReadWord)(void* context, unsigned int address,
                                      unsigned int* value);
typedef struct {
    unsigned int owner, base, size, systemHeap, systemStart, systemEnd;
    unsigned int parentBlockTag, ownerBlockTag, usedSignature, freeSignature;
} LmSharedArchiveDescriptor;
typedef char LmSharedArchiveDescriptorSize[sizeof(LmSharedArchiveDescriptor) == 40 ? 1 : -1];

typedef struct {
    void* context;
    LmSharedArchiveReadWord read;
    unsigned int address, value;
} LmSharedArchiveReader;

static inline int LmSharedRead(LmSharedArchiveReader* reader, unsigned int address) {
    reader->address = address;
    reader->value = 0u;
    return reader->read(reader->context, address, &reader->value);
}

static inline int LmSharedExpected(LmSharedArchiveReader* reader,
    unsigned int address, unsigned int expected) {
    return LmSharedRead(reader, address) && reader->value == expected;
}

static inline int LmSharedRange(unsigned int base, unsigned int size,
    unsigned int start, unsigned int end) {
    return (base & 3u) == 0u && base >= start && base < end &&
           size != 0u && size <= end - base;
}

static inline int LmSharedOverlap(unsigned int a, unsigned int sizeA,
    unsigned int b, unsigned int sizeB) {
    return a < b + sizeB && b < a + sizeA;
}

static inline unsigned int LmSharedHash(unsigned int hash, unsigned int word) {
    return (hash ^ word) * 16777619u;
}

/* Reciprocal links plus a fixed traversal bound reject cycles without a large
 * visited array. Never infer allocation ownership from an interior HM pattern. */
static inline int LmSharedHeapList(LmSharedArchiveReader* reader,
    LmSharedArchiveDescriptor* result, int used) {
    unsigned int offset = used ? 0x7Cu : 0x74u;
    unsigned int head, tail, node, previous = 0u, count = 0u, found = 0u;
    unsigned int signature = 2166136261u;
    if (!LmSharedRead(reader, result->systemHeap + offset)) return 0;
    head = node = reader->value;
    if (!LmSharedRead(reader, result->systemHeap + offset + 4u)) return 0;
    tail = reader->value;
    while (node) {
        unsigned int tag, bytes, next;
        reader->address = previous ? previous + 12u : result->systemHeap + offset;
        reader->value = node;
        if (++count > LM_SHARED_ARCHIVE_MAX_BLOCKS ||
            !LmSharedRange(node, 16u, result->systemStart, result->systemEnd) ||
            !LmSharedRead(reader, node)) return 0;
        tag = reader->value;
        if (used ? (tag >> 16) != 0x484Du : tag != 0u) return 0;
        if (!LmSharedRead(reader, node + 4u)) return 0;
        bytes = reader->value;
        if (bytes > result->systemEnd - node - 16u ||
            !LmSharedExpected(reader, node + 8u, previous) ||
            !LmSharedRead(reader, node + 12u)) return 0;
        next = reader->value;
        if (used && node == result->base - 16u) {
            if ((tag & 0xFFu) != 0x10u || bytes != LM_SHARED_ARCHIVE_SIZE)
                return 0;
            result->parentBlockTag = tag;
            found |= 1u;
        } else if (used && node == result->owner - 16u) {
            if ((tag & 0xFFu) != 0x10u || bytes != 0x68u) return 0;
            result->ownerBlockTag = tag;
            found |= 2u;
        } else if (LmSharedOverlap(node, bytes + 16u, result->base - 16u,
                                    LM_SHARED_ARCHIVE_SIZE + 16u) ||
                   LmSharedOverlap(node, bytes + 16u, result->owner - 16u,
                                    0x78u)) return 0;
        signature = LmSharedHash(signature, node);
        signature = LmSharedHash(signature, tag);
        signature = LmSharedHash(signature, bytes);
        signature = LmSharedHash(signature, previous);
        signature = LmSharedHash(signature, next);
        previous = node;
        node = next;
    }
    reader->address = result->systemHeap + offset + 4u;
    reader->value = tail;
    if (previous != tail || (used && found != 3u)) return 0;
    if (!LmSharedExpected(reader, result->systemHeap + offset, head) ||
        !LmSharedExpected(reader, result->systemHeap + offset + 4u, tail)) return 0;
    if (used) result->usedSignature = LmSharedHash(signature, count);
    else result->freeSignature = LmSharedHash(signature, count);
    return 1;
}

/* Native fetchResource only mutates entry +10. Other directory, file-shape and
 * name metadata must still identify the clean Japanese archive. */
static inline int LmSharedMetadata(LmSharedArchiveReader* reader, unsigned int base) {
    const unsigned int header[16] = {0x52415243u, LM_SHARED_ARCHIVE_SIZE,
        0x20u, 0x6E00u, 0x412CE0u, 0x412CE0u, 0u, 0u,
        0x19u, 0x20u, 0x358u, 0x1C0u, 0x2960u, 0x44A0u, 0x03580100u, 0u};
    unsigned int i, j, signature = 2166136261u;
    for (i = 0u; i < 16u; ++i)
        if (!LmSharedExpected(reader, base + i * 4u, header[i])) return 0;
    for (i = 0u; i < 0x19u * 0x10u; i += 4u) {
        if (!LmSharedRead(reader, base + 0x40u + i)) return 0;
        signature = LmSharedHash(signature, reader->value);
    }
    for (i = 0u; i < 0x2960u; i += 4u) {
        if (!LmSharedRead(reader, base + 0x44C0u + i)) return 0;
        signature = LmSharedHash(signature, reader->value);
    }
    for (i = 0u; i < 0x358u; ++i)
        for (j = 0u; j < 0x10u; j += 4u) {
            if (!LmSharedRead(reader, base + 0x1E0u + i * 0x14u + j)) return 0;
            signature = LmSharedHash(signature, reader->value);
        }
    reader->address = base + 0x20u;
    reader->value = signature;
    return signature == LM_SHARED_ARCHIVE_METADATA_SIGNATURE;
}

static inline int LmSharedArchiveCheck(LmSharedArchiveReader* reader,
    LmSharedArchiveDescriptor* result) {
    unsigned int i;
    const unsigned int offsets[7] = {0x28u, 0x40u, 0x44u, 0x48u, 0x4Cu, 0x50u, 0x60u};
    const unsigned int relative[7] = {0x44C5u, 0u, 0x20u, 0x40u, 0x1E0u, 0x44C0u, 0x6E20u};
    if (!LmSharedRead(reader, 0x804A0B94u)) return 0;
    result->systemHeap = reader->value;
    if (!LmSharedRange(result->systemHeap, 0x88u, 0x80000000u, 0x81800000u) ||
        !LmSharedExpected(reader, result->systemHeap, 0x8038886Cu) ||
        !LmSharedRead(reader, result->systemHeap + 0x30u)) return 0;
    result->systemStart = reader->value;
    if (!LmSharedRead(reader, result->systemHeap + 0x34u)) return 0;
    result->systemEnd = reader->value;
    if (result->systemStart < result->systemHeap + 0x88u ||
        result->systemEnd > 0x81800000u ||
        !LmSharedRange(result->systemStart, 16u, result->systemHeap + 0x88u,
                        result->systemEnd) || (result->systemEnd & 3u) != 0u ||
        !LmSharedExpected(reader, result->systemHeap + 0x38u,
                           result->systemEnd - result->systemStart) ||
        !LmSharedRead(reader, 0x804A12B0u)) return 0;
    result->owner = reader->value;
    if (result->owner < result->systemStart + 16u ||
        !LmSharedRange(result->owner, 0x68u, result->systemStart, result->systemEnd) ||
        !LmSharedExpected(reader, result->owner, 0x80388D5Cu) ||
        !LmSharedExpected(reader, result->owner + 4u, result->systemHeap) ||
        !LmSharedExpected(reader, result->owner + 0x38u, result->systemHeap) ||
        !LmSharedRead(reader, result->owner + 0x5Cu)) return 0;
    result->base = reader->value;
    result->size = LM_SHARED_ARCHIVE_SIZE;
    if (result->base < result->systemStart + 16u || (result->base & 31u) != 0u ||
        !LmSharedRange(result->base, result->size, result->systemStart, result->systemEnd) ||
        LmSharedOverlap(result->base - 16u, result->size + 16u,
                        result->owner - 16u, 0x78u) ||
        !LmSharedHeapList(reader, result, 1) ||
        !LmSharedHeapList(reader, result, 0)) return 0;
    if (!LmSharedExpected(reader, result->owner + 8u, result->owner) ||
        !LmSharedExpected(reader, result->owner + 0xCu, result->systemHeap + 0x58u) ||
        !LmSharedExpected(reader, result->owner + 0x18u, result->owner) ||
        !LmSharedExpected(reader, result->owner + 0x1Cu, 0x80494754u) ||
        !LmSharedExpected(reader, result->owner + 0x2Cu, 0x52415243u) ||
        !LmSharedRead(reader, result->owner + 0x30u) || (reader->value >> 24) != 1u ||
        !LmSharedExpected(reader, result->owner + 0x34u, 1u) ||
        !LmSharedRead(reader, result->owner + 0x3Cu) || (reader->value >> 24) != 1u ||
        !LmSharedExpected(reader, result->owner + 0x54u, 1u) ||
        !LmSharedExpected(reader, result->owner + 0x58u, 0u) ||
        !LmSharedRead(reader, result->owner + 0x64u) || (reader->value >> 24) != 0u)
        return 0;
    for (i = 0u; i < 7u; ++i)
        if (!LmSharedExpected(reader, result->owner + offsets[i],
                               result->base + relative[i])) return 0;
    return LmSharedMetadata(reader, result->base) &&
        LmSharedExpected(reader, 0x804A12B0u, result->owner) &&
        LmSharedExpected(reader, 0x804A0B94u, result->systemHeap);
}

/* Read-only compiled JP resource kind. Out is always zero on refusal; an
 * archive-provided address is never a destination authorization. */
static inline int LmSharedArchiveValidate(void* context, LmSharedArchiveReadWord readWord,
    LmSharedArchiveDescriptor* out, unsigned int* fault, unsigned int* value) {
    LmSharedArchiveDescriptor result = {0u};
    LmSharedArchiveDescriptor empty = {0u};
    LmSharedArchiveReader reader;
    int valid = 0;
    reader.context = context;
    reader.read = readWord;
    reader.address = reader.value = 0u;
    if (out) *out = empty;
    if (readWord && out) valid = LmSharedArchiveCheck(&reader, &result);
    if (valid) *out = result;
    if (fault) *fault = valid ? 0u : reader.address;
    if (value) *value = valid ? 0u : reader.value;
    return valid;
}

static inline int LmSharedArchiveSameIdentity(const LmSharedArchiveDescriptor* a,
    const LmSharedArchiveDescriptor* b) {
    return a && b && a->size == LM_SHARED_ARCHIVE_SIZE && a->owner == b->owner &&
        a->base == b->base && a->size == b->size && a->systemHeap == b->systemHeap &&
        a->systemStart == b->systemStart && a->systemEnd == b->systemEnd &&
        a->parentBlockTag == b->parentBlockTag && a->ownerBlockTag == b->ownerBlockTag &&
        a->usedSignature == b->usedSignature && a->freeSignature == b->freeSignature;
}

/* Requires a descriptor validated in this admission. Nested archives must be
 * wholly inside parent file data, excluding headers and allocator/owner bytes. */
static inline int LmSharedArchiveContains(const LmSharedArchiveDescriptor* parent,
    unsigned int base, unsigned int size) {
    return parent && parent->size == LM_SHARED_ARCHIVE_SIZE &&
        LmSharedRange(parent->base, parent->size, 0x80000000u, 0x81800000u) &&
        (base & 31u) == 0u && (size & 31u) == 0u && size >= 0x20u &&
        LmSharedRange(base, size, parent->base + LM_SHARED_ARCHIVE_DATA_OFFSET,
                        parent->base + parent->size);
}

#endif
