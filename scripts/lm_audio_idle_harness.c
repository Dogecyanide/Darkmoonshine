#include <susamune/lm_audio_idle.h>

typedef struct {
    const unsigned char* bytes;
    unsigned int size, failAddress;
} Image;
static unsigned int calls, foreignReads, faultAddress, faultValue;

static int imageRead(void* context, unsigned int address, unsigned int* value) {
    const Image* image = (const Image*)context;
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

int run(const unsigned char* bytes, unsigned int size, unsigned int failAddress) {
    Image image = {bytes, size, failAddress};
    calls = foreignReads = 0u;
    return LmAudioIdleValidate(&image, imageRead, &faultAddress, &faultValue);
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

int nullReader(void) {
    return LmAudioIdleValidate(0, 0, &faultAddress, &faultValue);
}
