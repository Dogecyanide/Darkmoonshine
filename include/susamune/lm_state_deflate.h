#ifndef SUSAMUNE_LM_STATE_DEFLATE_H
#define SUSAMUNE_LM_STATE_DEFLATE_H

#define LM_STATE_DEFLATE_CODEC 0x44464C31u
#define LM_STATE_FAST_CODEC 0x4C5A3431u
#define LM_STATE_FAST_MAGIC 0x4C4D4C34u
#define LM_STATE_FAST_BLOCK 0x20000u
#define LM_STATE_DEFLATE_WORKSPACE 0x50000u

typedef struct LmStateSegment {
    unsigned char *data;
    unsigned int size;
} LmStateSegment;

/* 0xFFFFFFFF means no safe tail; the live snapshot and prospective target both
 * precede the staging bytes, even if the target fails validation after decode. */
static inline unsigned int LmStateStagingTail(unsigned int oldRaw,
    unsigned int targetRaw, unsigned int trailer, unsigned int payloadLimit) {
    unsigned int raw = oldRaw > targetRaw ? oldRaw : targetRaw;
    unsigned int end;
    if (trailer > payloadLimit || raw > payloadLimit - trailer) return 0xFFFFFFFFu;
    end = raw + trailer;
    if (end > 0xFFFFFFFFu - 31u) return 0xFFFFFFFFu;
    end = (end + 31u) & ~31u;
    return end <= payloadLimit ? end : 0xFFFFFFFFu;
}

#ifdef __cplusplus
extern "C" {
#endif
/* Returns exact required bytes even if segments are too small (or null).
 * Only the bounded segment prefixes may be written on capacity failure. */
unsigned int LmStateDeflate(const unsigned char *src, unsigned int size,
    const LmStateSegment segments[2], void *workspace);
/* Independent LZ4 blocks, raw fallback, trailing big-endian Adler32. Same count
 * and bounded-prefix contract; 0 indicates invalid arguments/codec failure.
 * Workspace must be naturally aligned and disjoint from source/output. */
unsigned int LmStateDeflateFast(const unsigned char *src, unsigned int size,
    const LmStateSegment segments[2], void *workspace);
/* Null destination fully decodes into private workspace, validating the checksum
 * and exact input/output lengths without changing a live snapshot. A non-null
 * destination can be partially written on corrupt input: dry-validate unchanged
 * input before committing. Source, destination and workspace must not overlap. */
int LmStateInflate(const LmStateSegment segments[2], unsigned int packedSize,
    unsigned char *dst, unsigned int expected, void *workspace);
#ifdef __cplusplus
}
#endif

#endif
