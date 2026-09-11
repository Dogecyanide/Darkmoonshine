#include "susamune/lm_exp_heap.h"

static unsigned char* memory;
static unsigned int size, reads, foreign, fail, fault, value;

static int imageRead(void* context, unsigned int address, unsigned int* out) {
    unsigned int offset;
    (void)context;
    ++reads;
    if (address < 0x80000000u || (address & 3u) ||
        address - 0x80000000u > size - 4u) { ++foreign; return 0; }
    if (address == fail) return 0;
    offset = address - 0x80000000u;
    *out = ((unsigned int)memory[offset] << 24) |
           ((unsigned int)memory[offset + 1] << 16) |
           ((unsigned int)memory[offset + 2] << 8) | memory[offset + 3];
    return 1;
}

int run(unsigned char* image, unsigned int bytes, unsigned int heap,
        unsigned int failAddress) {
    memory = image; size = bytes; fail = failAddress;
    reads = foreign = fault = value = 0;
    return bytes >= 4u && LmExpHeapValidate(0, imageRead, heap, &fault, &value);
}
unsigned int metric(unsigned int index) {
    return index == 0 ? reads : index == 1 ? foreign : index == 2 ? fault : value;
}
int nullReader(void) { return LmExpHeapValidate(0, 0, 0, &fault, &value); }
