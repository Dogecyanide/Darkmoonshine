#include <susamune/lm_state_roots.h>

typedef struct {
    const unsigned char* bytes;
    unsigned int size, failAddress;
} Image;

static unsigned int calls, foreignReads, faultAddress, faultValue;

static int imageRead(void* context, unsigned int address, unsigned int* value) {
    Image* image = (Image*)context;
    const unsigned char* p;
    ++calls;
    if (address == image->failAddress) return 0;
    if ((address & 3u) || address < 0x80000000u || image->size < 4u ||
        address - 0x80000000u > image->size - 4u) {
        ++foreignReads;
        return 0;
    }
    p = image->bytes + address - 0x80000000u;
    *value = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
             ((unsigned int)p[2] << 8) | p[3];
    return 1;
}

int run(const unsigned char* bytes, unsigned int size,
    const LmStateGameRoots* roots, unsigned int start, unsigned int end,
    unsigned int failAddress) {
    Image image;
    image.bytes = bytes;
    image.size = size;
    image.failAddress = failAddress;
    calls = foreignReads = 0u;
    return LmStateGameRootsValidate(&image, imageRead, roots, start, end,
                                    &faultAddress, &faultValue);
}

int resourceRun(const unsigned char* bytes, unsigned int size,
    const LmStateResourceRoots* roots, unsigned int start, unsigned int end,
    unsigned int failAddress) {
    Image image;
    image.bytes = bytes;
    image.size = size;
    image.failAddress = failAddress;
    calls = foreignReads = 0u;
    return LmStateResourceRootsValidate(&image, imageRead, roots, start, end,
                                        &faultAddress, &faultValue);
}

unsigned int metric(unsigned int index) {
    switch (index) {
        case 0: return calls;
        case 1: return foreignReads;
        case 2: return faultAddress;
        case 3: return faultValue;
        default: return 0u;
    }
}

int maskAllowed(unsigned int mask) { return LmStateGameEpochMaskAllowed(mask); }
int rootOnly(unsigned int mask) { return LmStateGameRootOnlyEpoch(mask); }
int nullReader(void) {
    return LmStateGameRootsValidate(0, 0, 0, 0x80BE4560u, 0x817FB140u, 0, 0);
}
