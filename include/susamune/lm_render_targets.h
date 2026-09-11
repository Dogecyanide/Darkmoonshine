#ifndef SUSAMUNE_LM_RENDER_TARGETS_H
#define SUSAMUNE_LM_RENDER_TARGETS_H

#define LM_RENDER_TARGET_OWNER 0x803C4B6Cu
#define LM_RENDER_TARGET_OWNER_SIZE 0x114u
#define LM_RENDER_TARGET_MAX_BLOCKS 8192u

typedef int (*LmRenderTargetReadWord)(void* context, unsigned int address,
                                      unsigned int* value);

typedef struct {
    void* context;
    LmRenderTargetReadWord read;
    unsigned int address, value;
} LmRenderTargetReader;

static inline int LmRenderTargetRead(LmRenderTargetReader* r, unsigned int address) {
    r->address = address;
    r->value = 0u;
    return r->read(r->context, address, &r->value);
}

static inline int LmRenderTargetExpected(LmRenderTargetReader* r,
    unsigned int address, unsigned int expected, unsigned int mask) {
    return LmRenderTargetRead(r, address) && (r->value & mask) == expected;
}

static inline int LmRenderTargetRange(unsigned int base, unsigned int size,
    unsigned int start, unsigned int end) {
    return (base & 3u) == 0u && base >= start && base < end &&
           size != 0u && size <= end - base;
}

/* GXLoadTexObj changes each command's high-byte register ID. Its address,
 * dimensions, sampling format and load geometry still describe this buffer. */
static inline int LmRenderTargetTexture(LmRenderTargetReader* r,
    unsigned int object, unsigned int backing, unsigned int width,
    unsigned int height) {
    return LmRenderTargetExpected(r, object + 8u,
               (width - 1u) | ((height - 1u) << 10u) | (3u << 20u), 0xFFFFFFu) &&
        LmRenderTargetExpected(r, object + 12u,
               (backing >> 5u) & 0x1FFFFFu, 0x1FFFFFu) &&
        LmRenderTargetExpected(r, object + 20u, 3u, 0xFFFFFFFFu) &&
        LmRenderTargetExpected(r, object + 28u,
               ((width / 4u) * (height / 4u) << 16u) | 0x0202u, 0xFFFFFFFFu);
}

static inline int LmRenderTargetCheck(LmRenderTargetReader* r,
    unsigned int start, unsigned int end, unsigned int usedHead,
    unsigned int usedTail) {
    const unsigned int sizes[2] = {0x96000u, 0x20000u};
    unsigned int buffers[2], i, node, previous = 0u, count = 0u, found = 0u;
    r->address = start;
    r->value = end;
    if (start < 0x80000000u || end > 0x81800000u ||
        !LmRenderTargetRange(start, 16u, start, end)) return 0;
    for (i = 0u; i < 2u; ++i) {
        if (!LmRenderTargetRead(r, LM_RENDER_TARGET_OWNER + 0xA8u + i * 4u)) return 0;
        buffers[i] = r->value;
        if ((buffers[i] & 31u) != 0u || buffers[i] < start + 16u ||
            !LmRenderTargetRange(buffers[i], sizes[i], start, end)) return 0;
    }
    r->address = LM_RENDER_TARGET_OWNER + 0xACu;
    r->value = buffers[1];
    if (buffers[0] - 16u < buffers[1] + sizes[1] &&
        buffers[1] - 16u < buffers[0] + sizes[0]) return 0;
    if (!LmRenderTargetTexture(r, LM_RENDER_TARGET_OWNER + 0x60u,
                              buffers[0], 640u, 480u) ||
        !LmRenderTargetTexture(r, LM_RENDER_TARGET_OWNER + 0x80u,
                              buffers[1], 256u, 256u)) return 0;

    node = usedHead;
    while (node) {
        unsigned int tag, bytes, next;
        r->address = previous ? previous + 12u : start;
        r->value = node;
        if (++count > LM_RENDER_TARGET_MAX_BLOCKS ||
            !LmRenderTargetRange(node, 16u, start, end) ||
            !LmRenderTargetRead(r, node)) return 0;
        tag = r->value;
        if ((tag >> 16u) != 0x484Du || !LmRenderTargetRead(r, node + 4u)) return 0;
        bytes = r->value;
        if (bytes > end - node - 16u ||
            !LmRenderTargetExpected(r, node + 8u, previous, 0xFFFFFFFFu) ||
            !LmRenderTargetRead(r, node + 12u)) return 0;
        next = r->value;
        for (i = 0u; i < 2u; ++i) {
            if (node == buffers[i] - 16u) {
                r->address = node + (bytes != sizes[i] ? 4u : 0u);
                r->value = bytes != sizes[i] ? bytes : tag;
                if (bytes != sizes[i] || (tag & 0xFFu) != 0x0Du) return 0;
                found |= 1u << i;
            } else if (node < buffers[i] + sizes[i] &&
                       buffers[i] - 16u < node + 16u + bytes) {
                r->address = node;
                r->value = buffers[i];
                return 0;
            }
        }
        previous = node;
        node = next;
    }
    r->address = usedTail;
    r->value = previous;
    if (previous != usedTail) return 0;
    r->address = LM_RENDER_TARGET_OWNER + ((found & 1u) ? 0xACu : 0xA8u);
    r->value = (found & 1u) ? buffers[1] : buffers[0];
    return found == 3u;
}

/* Validate each saved/live endpoint independently. Reader and list anchors
 * must address the same image; no game write or address relocation is done. */
static inline int LmRenderTargetsValidate(void* context, LmRenderTargetReadWord read,
    unsigned int heapStart, unsigned int heapEnd, unsigned int usedHead,
    unsigned int usedTail, unsigned int* faultAddress, unsigned int* faultValue) {
    LmRenderTargetReader reader;
    int valid = 0;
    reader.context = context;
    reader.read = read;
    reader.address = reader.value = 0u;
    if (read) valid = LmRenderTargetCheck(&reader, heapStart, heapEnd, usedHead, usedTail);
    if (faultAddress) *faultAddress = valid ? 0u : reader.address;
    if (faultValue) *faultValue = valid ? 0u : reader.value;
    return valid;
}

#endif
