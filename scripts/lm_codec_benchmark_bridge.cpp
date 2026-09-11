#include <cstring>
#include "susamune/lm_state_deflate.h"
#include "susamune/lm_state_storage.h"
#include "susamune/lm_crc32.h"

#if defined(_WIN32)
#define API extern "C" __declspec(dllexport)
#else
#define API extern "C"
#endif

API unsigned int lm_bench_constant(unsigned int index) {
    switch (index) {
    case 0: return LM_STATE_DEFLATE_WORKSPACE;
    case 1: return SUSAMUNE_LM_CACHE_SIZE - SUSAMUNE_LM_MAILBOX_SIZE - LM_STATE_DEFLATE_WORKSPACE;
    case 2: return LM_STATE_STORAGE_PAYLOAD_MAX - 0x2000u;
    case 4: return NIN_MEM2_FILE_PATCH_SIZE;
#ifdef LM_STATE_FAST_CODEC
    case 3: return 1;
#endif
    default: return 0;
    }
}

API unsigned int lm_bench_pack(unsigned int fast, const unsigned char *source,
    unsigned int size, unsigned char *destination, unsigned int capacity, void *workspace) {
    const LmStateSegment output[2] = {{destination, capacity}, {nullptr, 0}};
#ifdef LM_STATE_FAST_CODEC
    if (fast) return LmStateDeflateFast(source, size, output, workspace);
#else
    if (fast) return 0;
#endif
    return LmStateDeflate(source, size, output, workspace);
}

API int lm_bench_unpack(const unsigned char *source, unsigned int packed,
    unsigned char *destination, unsigned int size, void *workspace) {
    const LmStateSegment input[2] = {{const_cast<unsigned char *>(source), packed}, {nullptr, 0}};
    return LmStateInflate(input, packed, destination, size, workspace);
}

API int lm_bench_load(const unsigned char *source, unsigned int packed,
    unsigned char *destination, unsigned int size, void *workspace) {
    return lm_bench_unpack(source, packed, nullptr, size, workspace) &&
        lm_bench_unpack(source, packed, destination, size, workspace);
}

API void lm_bench_copy(unsigned char *destination, const unsigned char *source, unsigned int size) {
    std::memcpy(destination, source, size);
}

// Frozen RC3 snapshot crcByte loop; no hardware intrinsics or zlib shortcut.
API unsigned int lm_bench_crc_bitwise(const unsigned char *source, unsigned int size) {
    unsigned int crc = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < size; ++i) {
        crc ^= source[i];
        for (unsigned int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

API unsigned int lm_bench_crc_table(const unsigned char *source, unsigned int size) {
    unsigned int crc = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < size; ++i) crc = LmCrc32Byte(crc, source[i]);
    return crc ^ 0xFFFFFFFFu;
}

API unsigned int lm_bench_evict(volatile unsigned char *buffer, unsigned int size) {
    unsigned int value = 0;
    for (unsigned int i = 0; i < size; i += 64) {
        buffer[i] = static_cast<unsigned char>(buffer[i] + 1);
        value += buffer[i];
    }
    return value;
}
