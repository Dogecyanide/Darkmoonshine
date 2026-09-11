#include "susamune/lm_colour_model.h"

struct TestMemory {
    const unsigned char *bytes;
    unsigned int size, badReads, reads;
};
static unsigned int TestRead(void *opaque, unsigned int address) {
    struct TestMemory *m = (struct TestMemory *)opaque;
    unsigned int offset = address - 0x80000000u;
    ++m->reads;
    if ((address & 3u) || address < 0x80000000u ||
        m->size < 4 || offset > m->size - 4) {
        ++m->badReads;
        return 0;
    }
    return (unsigned int)m->bytes[offset] << 24 |
        (unsigned int)m->bytes[offset + 1] << 16 |
        (unsigned int)m->bytes[offset + 2] << 8 | m->bytes[offset + 3];
}
__declspec(dllexport) int validate(const unsigned char *memory, unsigned int size,
    unsigned int heap, unsigned int model, unsigned int *result) {
    struct TestMemory m = {memory, size, 0, 0};
    struct LmColourModelView view = {{0xAAAAAAAAu, 0xBBBBBBBBu}};
    int valid = LmColourResolveModel(&m, TestRead, heap, model, &view);
    result[0] = view.texture[0]; result[1] = view.texture[1];
    result[2] = m.badReads; result[3] = m.reads;
    return valid;
}
