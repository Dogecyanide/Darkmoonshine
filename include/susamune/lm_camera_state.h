#ifndef SUSAMUNE_LM_CAMERA_STATE_H
#define SUSAMUNE_LM_CAMERA_STATE_H

#define LM_CAMERA_ROOTS 0x80399BE0u
#define LM_CAMERA_COUNT 3u
#define LM_CAMERA_BYTES 0xECu
#define LM_CAMERA_MAX_BLOCKS 8192u

typedef int (*LmCameraReadWord)(void*, unsigned int, unsigned int*);
typedef struct {
    void* context;
    LmCameraReadWord read;
    unsigned int fault, value;
} LmCameraReader;

static inline int LmCameraWord(LmCameraReader* r, unsigned int address) {
    r->fault = address;
    r->value = 0u;
    return r->read(r->context, address, &r->value);
}

static inline int LmCameraRange(unsigned int address, unsigned int bytes,
    unsigned int start, unsigned int end) {
    return !(address & 3u) && address >= start && address < end &&
        bytes && bytes <= end - address;
}

/* These plain camera blocks are direct GAME allocations, not interior pointers
 * or retained SYS owners. Validate each endpoint in its own memory image. */
static inline int LmCameraGameCheck(LmCameraReader* r, const unsigned int* targets,
    unsigned int start, unsigned int end, unsigned int head, unsigned int tail) {
    unsigned int i, j, node = head, previous = 0u, count = 0u, found = 0u;
    unsigned int cameraStarts[LM_CAMERA_COUNT];
    if (!targets || start < 0x80000000u || end > 0x81800000u || start >= end)
        return 0;
    for (i = 0u; i < LM_CAMERA_COUNT; ++i) {
        unsigned int padding;
        r->fault = LM_CAMERA_ROOTS + i * 4u;
        r->value = targets[i];
        if (!LmCameraRange(targets[i], LM_CAMERA_BYTES, start + 16u, end)) return 0;
        if (!LmCameraWord(r, LM_CAMERA_ROOTS + i * 4u) || r->value != targets[i])
            return 0;
        if (!LmCameraWord(r, targets[i] - 16u)) return 0;
        padding = (r->value >> 8u) & 0x7Fu;
        if ((padding & 3u) || padding > targets[i] - 16u - start) return 0;
        cameraStarts[i] = targets[i] - 16u - padding;
        for (j = 0u; j < i; ++j)
            if (cameraStarts[i] < targets[j] + LM_CAMERA_BYTES &&
                cameraStarts[j] < targets[i] + LM_CAMERA_BYTES) return 0;
    }
    while (node) {
        unsigned int tag, padding, bytes, next;
        r->fault = previous ? previous + 12u : start;
        r->value = node;
        if (++count > LM_CAMERA_MAX_BLOCKS || !LmCameraRange(node, 16u, start, end) ||
            !LmCameraWord(r, node)) return 0;
        tag = r->value;
        padding = (tag >> 8u) & 0x7Fu;
        if ((tag >> 16u) != 0x484Du || (padding & 3u) || padding > node - start ||
            !LmCameraWord(r, node + 4u)) return 0;
        bytes = r->value;
        if ((bytes & 3u) || bytes > end - node - 16u ||
            !LmCameraWord(r, node + 8u) || r->value != previous ||
            !LmCameraWord(r, node + 12u)) return 0;
        next = r->value;
        for (i = 0u; i < LM_CAMERA_COUNT; ++i) {
            if (node == targets[i] - 16u) {
                r->fault = node;
                r->value = tag;
                if (bytes != LM_CAMERA_BYTES || (tag & 0xFFu) != 1u ||
                    node - padding != cameraStarts[i]) return 0;
                found |= 1u << i;
            } else if (node - padding < targets[i] + LM_CAMERA_BYTES &&
                       cameraStarts[i] < node + 16u + bytes) return 0;
        }
        previous = node;
        node = next;
    }
    r->fault = tail;
    r->value = previous;
    return previous == tail && found == 7u;
}

static inline int LmCameraGameValidate(void* context, LmCameraReadWord read,
    const unsigned int* targets, unsigned int start, unsigned int end,
    unsigned int head, unsigned int tail, unsigned int* fault, unsigned int* value) {
    LmCameraReader r;
    int valid;
    r.context = context;
    r.read = read;
    r.fault = r.value = 0u;
    valid = read && LmCameraGameCheck(&r, targets, start, end, head, tail);
    if (fault) *fault = valid ? 0u : r.fault;
    if (value) *value = valid ? 0u : r.value;
    return valid;
}
#endif
