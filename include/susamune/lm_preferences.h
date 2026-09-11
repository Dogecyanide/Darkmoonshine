#ifndef SUSAMUNE_LM_PREFERENCES_H
#define SUSAMUNE_LM_PREFERENCES_H

#include "susamune/mem2_map.h"

#define LM_PREFERENCES_MAGIC 0x4C4D5046u
#define LM_PREFERENCES_VERSION 2u
#define LM_PREFERENCES_LEGACY_VERSION 1u
#define LM_PREFERENCES_WORD_COUNT 48u
#define LM_PREFERENCES_VALUE_COUNT 48u
#define LM_PREFERENCES_LEGACY_VALUE_COUNT 46u
#define LM_PREFERENCES_PRESENT_LO 0xFFFFFFFFu
#define LM_PREFERENCES_PRESENT_HI 0x0000FFFFu
#define LM_PREFERENCES_LEGACY_PRESENT_HI 0x00003FFFu
#define LM_PREFERENCES_OK 0u
#define LM_PREFERENCES_NO_FILE 1u
#define LM_PREFERENCES_INVALID 2u
#define LM_PREFERENCES_IO_ERROR 3u
#define LM_PREFERENCES_UNAVAILABLE 4u

/* Boot publication is ARM-owned; after boot only PPC writes control/data. */
struct LmPreferencesBlock {
    unsigned int magic, version, requestSeq, checksum;
    unsigned int presentLo, presentHi, reserved0[2];
    unsigned int ackSeq, status, ready, reserved1[5];
    unsigned int values[LM_PREFERENCES_WORD_COUNT];
};

#define LM_PREFERENCES_PPC_PTR \
    ((volatile struct LmPreferencesBlock *)SUSAMUNE_MEM2_CFG_PPC_BASE)
#define LM_PREFERENCES_PHYS_PTR \
    ((volatile struct LmPreferencesBlock *)SUSAMUNE_MEM2_CFG_PHYS_BASE)

typedef char LmPreferencesSizeCheck[sizeof(struct LmPreferencesBlock) == 256u ? 1 : -1];
typedef char LmPreferencesAckCheck[__builtin_offsetof(struct LmPreferencesBlock, ackSeq) == 32u ? 1 : -1];
typedef char LmPreferencesDataCheck[__builtin_offsetof(struct LmPreferencesBlock, values) == 64u ? 1 : -1];

static inline unsigned int LmPreferencesCrcWord(unsigned int crc,
                                               unsigned int value) {
    unsigned int byte, bit;
    for (byte = 0; byte < 4u; ++byte) {
        crc ^= (value >> (24u - byte * 8u)) & 255u;
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
    }
    return crc;
}

static inline unsigned int LmPreferencesChecksumForVersion(const volatile unsigned int *values,
                                                           unsigned int presentLo,
                                                           unsigned int presentHi,
                                                           unsigned int version) {
    unsigned int i, crc = 0xFFFFFFFFu;
    crc = LmPreferencesCrcWord(crc, version);
    crc = LmPreferencesCrcWord(crc, presentLo);
    crc = LmPreferencesCrcWord(crc, presentHi);
    for (i = 0; i < LM_PREFERENCES_WORD_COUNT; ++i)
        crc = LmPreferencesCrcWord(crc, values[i]);
    return ~crc;
}

static inline unsigned int LmPreferencesChecksum(const volatile unsigned int *values,
                                                 unsigned int presentLo,
                                                 unsigned int presentHi) {
    return LmPreferencesChecksumForVersion(values, presentLo, presentHi,
                                           LM_PREFERENCES_VERSION);
}

static inline int LmPreferencesHas(unsigned int low, unsigned int high,
                                   unsigned int index) {
    if (index >= LM_PREFERENCES_VALUE_COUNT) return 0;
    return index < 32u ? (low & (1u << index)) != 0u :
                        (high & (1u << (index - 32u))) != 0u;
}

static inline int LmPreferencesValid(const volatile struct LmPreferencesBlock *block) {
    unsigned int i;
    if (block->magic != LM_PREFERENCES_MAGIC || block->version != LM_PREFERENCES_VERSION ||
        (block->presentHi & ~LM_PREFERENCES_PRESENT_HI) || block->reserved0[0] ||
        block->reserved0[1]) return 0;
    for (i = 0; i < 5u; ++i) if (block->reserved1[i]) return 0;
    for (i = 0; i < LM_PREFERENCES_VALUE_COUNT; ++i)
        if (!LmPreferencesHas(block->presentLo, block->presentHi, i) &&
            block->values[i] != 0xFFFFFFFFu) return 0;
    return block->checksum == LmPreferencesChecksum(block->values,
                                                   block->presentLo, block->presentHi);
}

#endif
