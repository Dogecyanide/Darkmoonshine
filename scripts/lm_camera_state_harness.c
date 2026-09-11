#include "susamune/lm_camera_state.h"

typedef struct {
    const unsigned char* bytes;
    unsigned int size, start;
    const unsigned int* roots;
    unsigned int reads, foreign;
} CameraImage;

static int readCameraWord(void* context, unsigned int address, unsigned int* out) {
    CameraImage* image = (CameraImage*)context;
    unsigned int offset;
    ++image->reads;
    if (!(address & 3u) && address >= LM_CAMERA_ROOTS &&
        address < LM_CAMERA_ROOTS + LM_CAMERA_COUNT * 4u) {
        *out = image->roots[(address - LM_CAMERA_ROOTS) / 4u];
        return 1;
    }
    if ((address & 3u) || address < image->start ||
        address - image->start > image->size || image->size - (address - image->start) < 4u) {
        ++image->foreign;
        return 0;
    }
    offset = address - image->start;
    *out = (unsigned int)image->bytes[offset] << 24 |
        (unsigned int)image->bytes[offset + 1u] << 16 |
        (unsigned int)image->bytes[offset + 2u] << 8 | image->bytes[offset + 3u];
    return 1;
}

#ifdef __cplusplus
extern "C"
#endif
int camera_validate(const unsigned char* bytes, unsigned int size, unsigned int start,
    const unsigned int* roots, const unsigned int* targets, unsigned int head,
    unsigned int tail, unsigned int* diagnostic) {
    CameraImage image = {bytes, size, start, roots, 0u, 0u};
    int valid = LmCameraGameValidate(&image, readCameraWord, targets, start, start + size,
        head, tail, &diagnostic[0], &diagnostic[1]);
    diagnostic[2] = image.reads;
    diagnostic[3] = image.foreign;
    return valid;
}
