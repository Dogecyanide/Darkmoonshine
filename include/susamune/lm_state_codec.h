#ifndef SUSAMUNE_LM_STATE_CODEC_H
#define SUSAMUNE_LM_STATE_CODEC_H

/* Heapless bounded LZ stream. Literal token: BE16(length-1), then bytes.
 * Match token: BE16(0x8000|length-4), BE16(backward distance). */
static unsigned int LmStatePack(const unsigned char *src, unsigned int size,
    unsigned char *dst, unsigned int capacity, unsigned int *table) {
#define LM_STATE_EMIT(value) do { if (dst) dst[out] = (unsigned char)(value); ++out; } while (0)
    unsigned int i, pos = 0, anchor = 0, out = 0;
    for (i = 0; i < 4096; ++i) table[i] = 0;
    while (pos + 4 <= size) {
        unsigned int word = (unsigned int)src[pos] << 24 |
            (unsigned int)src[pos + 1] << 16 |
            (unsigned int)src[pos + 2] << 8 | src[pos + 3];
        unsigned int hash = (word * 2654435761u) >> 20;
        unsigned int previous = table[hash];
        unsigned int length = 0;
        table[hash] = pos + 1;
        if (previous && pos + 1 - previous <= 65535) {
            previous -= 1;
            while (length < 32771 && pos + length < size &&
                   src[previous + length] == src[pos + length]) ++length;
        }
        if (length < 4) { ++pos; continue; }
        while (anchor < pos) {
            unsigned int count = pos - anchor;
            if (count > 32768) count = 32768;
            if (out > capacity || count + 2 > capacity - out) return 0;
            LM_STATE_EMIT((count - 1) >> 8);
            LM_STATE_EMIT(count - 1);
            for (i = 0; i < count; ++i) { LM_STATE_EMIT(src[anchor]); ++anchor; }
        }
        if (out > capacity || capacity - out < 4) return 0;
        word = 0x8000u | (length - 4);
        LM_STATE_EMIT(word >> 8);
        LM_STATE_EMIT(word);
        word = pos - previous;
        LM_STATE_EMIT(word >> 8);
        LM_STATE_EMIT(word);
        pos += length;
        anchor = pos;
    }
    while (anchor < size) {
        unsigned int count = size - anchor;
        if (count > 32768) count = 32768;
        if (out > capacity || count + 2 > capacity - out) return 0;
        LM_STATE_EMIT((count - 1) >> 8);
        LM_STATE_EMIT(count - 1);
        for (i = 0; i < count; ++i) { LM_STATE_EMIT(src[anchor]); ++anchor; }
    }
    return out;
#undef LM_STATE_EMIT
}

/* A null output performs the identical bounds walk before any destination is
 * changed. An exact decoded length rejects truncation and appended tokens. */
static int LmStateUnpack(const unsigned char *src, unsigned int size,
    unsigned char *dst, unsigned int expected) {
    unsigned int in = 0, out = 0;
    while (in < size) {
        unsigned int token, count, i;
        if (size - in < 2) return 0;
        token = (unsigned int)src[in] << 8 | src[in + 1];
        in += 2;
        count = (token & 0x7fffu) + ((token & 0x8000u) ? 4 : 1);
        if (out > expected || count > expected - out) return 0;
        if (token & 0x8000u) {
            unsigned int distance;
            if (size - in < 2) return 0;
            distance = (unsigned int)src[in] << 8 | src[in + 1];
            in += 2;
            if (!distance || distance > out) return 0;
            if (dst) for (i = 0; i < count; ++i)
                dst[out + i] = dst[out + i - distance];
        } else {
            if (count > size - in) return 0;
            if (dst) for (i = 0; i < count; ++i) dst[out + i] = src[in + i];
            in += count;
        }
        out += count;
    }
    return out == expected;
}

#endif
