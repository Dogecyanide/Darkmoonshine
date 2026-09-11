#ifndef SUSAMUNE_LM_COLOUR_MODEL_H
#define SUSAMUNE_LM_COLOUR_MODEL_H

#define LM_COLOUR_TEXTURE_BYTES 0x2000u
#define LM_COLOUR_TEXTURE_HEADER_BYTES 32u

typedef unsigned int (*LmColourReadWord)(void *context, unsigned int address);
struct LmColourModelView { unsigned int texture[2]; };

static int LmColourMem1Range(unsigned int address, unsigned int size) {
    return address >= 0x80000000u && size <= 0x01800000u &&
        address <= 0x81800000u - size;
}
static int LmColourInside(unsigned int address, unsigned int size,
                           unsigned int start, unsigned int end) {
    return start < end && size <= end - start && address >= start && address <= end - size;
}
static int LmColourOverlaps(unsigned int a, unsigned int sizeA,
                             unsigned int b, unsigned int sizeB) {
    /* Call only after both ranges have passed MEM1/heap bounds checks. */
    return a < b + sizeB && b < a + sizeA;
}

/* Read callbacks see only aligned MEM1 words in previously validated ranges.
 * Pixel CRC authentication is separate, before retaining an original copy. */
static int LmColourResolveModel(void *context, LmColourReadWord read,
    unsigned int heap, unsigned int model, struct LmColourModelView *result) {
    unsigned int start, end, table, count, tableBytes, texture[2], i;
    const unsigned int textureBytes = LM_COLOUR_TEXTURE_HEADER_BYTES + LM_COLOUR_TEXTURE_BYTES;
    if (!result) return 0;
    result->texture[0] = result->texture[1] = 0;
    if (!read || (heap & 3u) || !LmColourMem1Range(heap, 0x3Cu)) return 0;
    if (read(context, heap) != 0x8038886Cu) return 0;
    start = read(context, heap + 0x30u);
    end = read(context, heap + 0x34u);
    if (start >= end || ((start | end) & 15u) || !LmColourMem1Range(start, end - start) ||
        read(context, heap + 0x38u) != end - start) return 0;
    if ((model & 31u) || !LmColourInside(model, 0x80u, start, end)) return 0;
    if (read(context, model) != 0x04B40000u || read(context, model + 4u) != 0x0CA60000u ||
        read(context, model + 8u) != 0x0041001Cu ||
        read(context, model + 0x10u) != 0x06D20C6Au ||
        (read(context, model + 0x28u) & 0xFFFFu) != 19u) return 0;
    count = read(context, model + 0x20u) >> 16;
    if (count < 7u || count > 64u) return 0;
    tableBytes = count * 4u;
    table = read(context, model + 0x60u);
    if ((table & 3u) || !LmColourInside(table, tableBytes, start, end) ||
        LmColourOverlaps(table, tableBytes, model, 0x80u)) return 0;
    texture[0] = read(context, table + 3u * 4u);
    texture[1] = read(context, table + 6u * 4u);
    for (i = 0; i < 2u; ++i) {
        if ((texture[i] & 31u) || !LmColourInside(texture[i], textureBytes, start, end) ||
            LmColourOverlaps(texture[i], textureBytes, model, 0x80u) ||
            LmColourOverlaps(texture[i], textureBytes, table, tableBytes)) return 0;
        if (read(context, texture[i]) != 0x0A000080u ||
            read(context, texture[i] + 4u) != 0x00800000u) return 0;
    }
    if (LmColourOverlaps(texture[0], textureBytes, texture[1], textureBytes)) return 0;
    result->texture[0] = texture[0];
    result->texture[1] = texture[1];
    return 1;
}
#endif
