#ifndef SUSAMUNE_LM_AUDIO_IDLE_H
#define SUSAMUNE_LM_AUDIO_IDLE_H

typedef int (*LmAudioIdleReadWord)(void* context, unsigned int address,
                                  unsigned int* value);

typedef struct {
    void* context;
    LmAudioIdleReadWord read;
    unsigned int address, value, start, end;
} LmAudioIdleReader;

static inline int LmAudioIdleRange(unsigned int address, unsigned int size,
                                  unsigned int start, unsigned int end) {
    return !(address & 3u) && address >= start && address < end &&
           size && size <= end - address;
}

static inline int LmAudioIdleWord(LmAudioIdleReader* r, unsigned int address) {
    r->address = address;
    r->value = 0u;
    return LmAudioIdleRange(address, 4u, 0x80000000u, 0x81800000u) &&
           r->read(r->context, address, &r->value);
}

static inline int LmAudioIdleExpected(LmAudioIdleReader* r,
    unsigned int address, unsigned int expected) {
    return LmAudioIdleWord(r, address) && r->value == expected;
}

static inline int LmAudioIdlePointer(LmAudioIdleReader* r,
    unsigned int address, unsigned int size, unsigned int* pointer) {
    if (!LmAudioIdleWord(r, address) ||
        !LmAudioIdleRange(r->value, size, r->start, r->end)) return 0;
    *pointer = r->value;
    return 1;
}

/* JP 8018D4E4 drains the active SE/sequence/stream owners. Its one retained
 * sequence is rebound to basic+64. Do not rewind the audio arena or compare
 * free-list links: they are live allocator state, not gameplay state. */
static inline int LmAudioIdleCheck(LmAudioIdleReader* r) {
    const unsigned int basic = 0x803E3CF8u;
    unsigned int heap, data, bootstrap, se, seq, stream;
    unsigned int seCount, seqCount, streamCount, index, i;
    r->start = 0x80000000u;
    r->end = 0x81800000u;
    if (!LmAudioIdlePointer(r, 0x804A0B94u, 0x88u, &heap) ||
        !LmAudioIdleExpected(r, heap, 0x8038886Cu) ||
        !LmAudioIdleWord(r, heap + 0x30u)) return 0;
    r->start = r->value;
    if (!LmAudioIdleWord(r, heap + 0x34u)) return 0;
    r->end = r->value;
    if (r->end > 0x81800000u || (r->end & 3u) ||
        !LmAudioIdleRange(r->start, 4u, heap + 0x88u, r->end) ||
        !LmAudioIdleExpected(r, 0x804A03A8u, basic) ||
        !LmAudioIdleExpected(r, 0x804A1DD0u, basic) ||
        !LmAudioIdleExpected(r, basic + 8u, 0x80383FB0u) ||
        !LmAudioIdlePointer(r, basic, 0x224u, &data) ||
        !LmAudioIdlePointer(r, basic + 0x64u, 0x34u, &bootstrap) ||
        !LmAudioIdleExpected(r, bootstrap + 8u, 0x80000800u) ||
        !LmAudioIdleExpected(r, bootstrap + 0x30u, basic + 0x64u) ||
        !LmAudioIdleWord(r, bootstrap)) return 0;
    index = r->value >> 24u;
    if (!LmAudioIdleWord(r, 0x804A042Cu)) return 0;
    seCount = r->value;
    if (!seCount || seCount > 16u) return 0;
    if (!LmAudioIdleWord(r, 0x804A0444u)) return 0;
    seqCount = r->value;
    if (!seqCount || seqCount > 16u || index >= seqCount) return 0;
    if (!LmAudioIdleWord(r, 0x804A045Cu)) return 0;
    streamCount = r->value;
    if (!streamCount || streamCount > 16u) return 0;
    if (!LmAudioIdlePointer(r, data + 0x1E8u, seCount * 0xCu, &se) ||
        !LmAudioIdlePointer(r, data + 0x180u, seqCount * 0x4Cu, &seq) ||
        !LmAudioIdlePointer(r, data + 0x184u, streamCount * 0x14u, &stream)) return 0;
    for (i = 0u; i < seCount; ++i)
        if (!LmAudioIdleExpected(r, se + i * 0xCu + 4u, 0u)) return 0;
    for (i = 0u; i < seqCount; ++i)
        if (!LmAudioIdleExpected(r, seq + i * 0x4Cu + 0x44u,
                                 i == index ? bootstrap : 0u)) return 0;
    for (i = 0u; i < streamCount; ++i)
        if (!LmAudioIdleExpected(r, stream + i * 0x14u + 0x10u, 0u)) return 0;
    return LmAudioIdleExpected(r, data + 0x214u, bootstrap) &&
           LmAudioIdleExpected(r, bootstrap + 0x28u, 0u) &&
           LmAudioIdleExpected(r, bootstrap + 0x2Cu, 0u) &&
           LmAudioIdleExpected(r, data + 0x220u, 0u);
}

/* Read-only postcondition, not a replacement for the native drain, a hardware
 * barrier or the retained-allocation profile. No linked list is followed. */
static inline int LmAudioIdleValidate(void* context, LmAudioIdleReadWord readWord,
    unsigned int* faultAddress, unsigned int* faultValue) {
    LmAudioIdleReader r = {context, readWord, 0u, 0u, 0u, 0u};
    const int valid = readWord && LmAudioIdleCheck(&r);
    if (faultAddress) *faultAddress = valid ? 0u : r.address;
    if (faultValue) *faultValue = valid ? 0u : r.value;
    return valid;
}

#endif
