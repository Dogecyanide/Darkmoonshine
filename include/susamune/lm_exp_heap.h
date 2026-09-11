#ifndef SUSAMUNE_LM_EXP_HEAP_H
#define SUSAMUNE_LM_EXP_HEAP_H

#define LM_EXP_HEAP_MAX_BLOCKS 8192u

typedef int (*LmExpHeapReadWord)(void* context, unsigned int address,
                                unsigned int* value);
typedef struct {
    void* context;
    LmExpHeapReadWord read;
    unsigned int address, value;
} LmExpHeapReader;

static inline int LmExpHeapRange(unsigned int address, unsigned int bytes,
                               unsigned int start, unsigned int end) {
    return !(address & 3u) && address >= start && address < end &&
           bytes && bytes <= end - address;
}

static inline int LmExpHeapWord(LmExpHeapReader* r, unsigned int address) {
    r->address = address;
    r->value = 0u;
    return r->read(r->context, address, &r->value);
}

static inline int LmExpHeapExpected(LmExpHeapReader* r, unsigned int address,
                                  unsigned int value) {
    return LmExpHeapWord(r, address) && r->value == value;
}

/* Match retail accounting, but validate links before following them. Used
 * blocks include the low seven alignment-padding bits of header byte +2. */
static inline int LmExpHeapList(LmExpHeapReader* r, unsigned int heap,
    unsigned int start, unsigned int end, int used, unsigned int* total) {
    unsigned int offset = used ? 0x7Cu : 0x74u;
    unsigned int node, head, tail, previous = 0u, count = 0u;
    unsigned int previousEnd = start;
    if (!LmExpHeapWord(r, heap + offset)) return 0;
    head = node = r->value;
    if (!LmExpHeapWord(r, heap + offset + 4u)) return 0;
    tail = r->value;
    while (node) {
        unsigned int tag, padding, bytes, next, span;
        r->address = previous ? previous + 12u : heap + offset;
        r->value = node;
        if (++count > LM_EXP_HEAP_MAX_BLOCKS ||
            !LmExpHeapRange(node, 16u, start, end) ||
            (!used && node < previousEnd) || !LmExpHeapWord(r, node)) return 0;
        tag = r->value;
        if ((tag >> 16) != (used ? 0x484Du : 0u)) return 0;
        padding = used ? (tag >> 8) & 0x7Fu : 0u;
        if ((padding & 3u) || padding > node - start) return 0;
        if (!LmExpHeapWord(r, node + 4u)) return 0;
        bytes = r->value;
        if ((bytes & 3u) || bytes > end - node - 16u) return 0;
        span = bytes + 16u + padding;
        if (span > end - start - *total) return 0;
        *total += span;
        if (!LmExpHeapExpected(r, node + 8u, previous) ||
            !LmExpHeapWord(r, node + 12u)) return 0;
        next = r->value;
        if (next && !LmExpHeapRange(next, 16u, start, end)) return 0;
        previousEnd = node + 16u + bytes;
        if (!used && next && next < previousEnd) return 0;
        previous = node;
        node = next;
    }
    r->address = heap + offset + 4u;
    r->value = tail;
    return previous == tail &&
        LmExpHeapExpected(r, heap + offset, head) &&
        LmExpHeapExpected(r, heap + offset + 4u, tail);
}

static inline int LmExpHeapCheck(LmExpHeapReader* r, unsigned int heap) {
    unsigned int start, end, total = 0u;
    r->address = 0u;
    r->value = heap;
    if (!LmExpHeapRange(heap, 0x88u, 0x80000000u, 0x81800000u) ||
        !LmExpHeapExpected(r, heap, 0x8038886Cu) ||
        !LmExpHeapWord(r, heap + 0x30u)) return 0;
    start = r->value;
    if (!LmExpHeapWord(r, heap + 0x34u)) return 0;
    end = r->value;
    if (end > 0x81800000u || (end & 3u) ||
        !LmExpHeapRange(start, 16u, heap + 0x88u, end) ||
        !LmExpHeapExpected(r, heap + 0x38u, end - start) ||
        !LmExpHeapList(r, heap, start, end, 1, &total) ||
        !LmExpHeapList(r, heap, start, end, 0, &total)) return 0;
    r->address = heap + 0x38u;
    r->value = total;
    return total == end - start;
}

/* Diagnostic structural check, not an object-ownership or synchronization
 * primitive. Use a quiescent boundary; never repair a failed list and resume. */
static inline int LmExpHeapValidate(void* context, LmExpHeapReadWord readWord,
    unsigned int heap, unsigned int* faultAddress, unsigned int* faultValue) {
    LmExpHeapReader reader;
    int valid;
    reader.context = context;
    reader.read = readWord;
    reader.address = reader.value = 0u;
    valid = readWord && LmExpHeapCheck(&reader, heap);
    if (faultAddress) *faultAddress = valid ? 0u : reader.address;
    if (faultValue) *faultValue = valid ? 0u : reader.value;
    return valid;
}

#endif
