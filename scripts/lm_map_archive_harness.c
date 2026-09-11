#include "susamune/lm_map_archive.h"

typedef struct {
    const unsigned char* bytes;
    unsigned int size, fail, reads, foreign;
} Image;

static int readWord(void* context, unsigned int address, unsigned int* value) {
    Image* image = (Image*)context;
    const unsigned char* p;
    ++image->reads;
    if (address == image->fail) return 0;
    if ((address & 3u) || address < 0x80000000u || image->size < 4u ||
        address - 0x80000000u > image->size - 4u) {
        ++image->foreign;
        return 0;
    }
    p = image->bytes + address - 0x80000000u;
    *value = (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 |
        (unsigned int)p[2] << 8 | p[3];
    return 1;
}

int mapValidate(const unsigned char* bytes, unsigned int size,
    const LmMapArchiveRoots* roots, unsigned int fail, unsigned int* result) {
    Image image = {bytes, size, fail, 0u, 0u};
    int valid = LmMapArchiveValidate(&image, readWord, roots, &result[0], &result[1]);
    result[2] = image.reads;
    result[3] = image.foreign;
    return valid;
}

int mapNull(void) { return LmMapArchiveValidate(0, 0, 0, 0, 0); }
