#include <susamune/lm_persistent_profile.h>

typedef struct {
    const unsigned char* bytes;
    unsigned int size, failAddress;
} Image;
static LmPersistentProfile current, saved;
static unsigned int calls, foreignReads, faultAddress, faultValue, difference;

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

int capture(const unsigned char* bytes, unsigned int size, unsigned int root,
    unsigned int system, unsigned int config, unsigned int generation,
    unsigned int failAddress) {
    Image image = {bytes, size, failAddress};
    calls = foreignReads = 0u;
    return LmPersistentCapture(&image, imageRead, root, system, config,
                              generation, &current, &faultAddress, &faultValue);
}

void keep(void) { saved = current; }
int matches(void) { return LmPersistentMatch(&saved, &current, &difference); }
int valid(unsigned int generation) { return LmPersistentProfileValid(&current, generation); }
void mutate(unsigned int word, unsigned int value, int checksum) {
    if (word < sizeof(current) / 4u) ((unsigned int*)&current)[word] = value;
    if (checksum) current.checksum = LmPersistentChecksum(&current);
}
unsigned int word(unsigned int index) {
    return index < sizeof(current) / 4u ? ((const unsigned int*)&current)[index] : 0u;
}
unsigned int metric(unsigned int index) {
    switch (index) {
        case 0: return calls;
        case 1: return foreignReads;
        case 2: return faultAddress;
        case 3: return faultValue;
        case 4: return difference;
        default: return 0u;
    }
}
