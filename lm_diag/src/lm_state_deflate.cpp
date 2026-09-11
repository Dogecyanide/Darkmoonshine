#include "../vendor/miniz/lm_miniz_config.h"
#include "../vendor/miniz/miniz.c"
#include "../vendor/miniz/miniz_tdef.c"
#include "../vendor/miniz/miniz_tinfl.c"
#include "../vendor/lz4/state_lz4_config.h"
#include "../vendor/lz4/lz4.c"
#include "susamune/lm_state_deflate.h"

namespace {
struct InflateWorkspace {
    tinfl_decompressor state;
    unsigned char ring[TINFL_LZ_DICT_SIZE];
};
struct FastWorkspace {
    LZ4_stream_t state;
    unsigned char raw[LM_STATE_FAST_BLOCK];
    unsigned char packed[LZ4_COMPRESSBOUND(LM_STATE_FAST_BLOCK)];
};
static_assert(sizeof(tdefl_compressor) <= LM_STATE_DEFLATE_WORKSPACE,
              "LM deflate workspace too small");
static_assert(sizeof(InflateWorkspace) <= LM_STATE_DEFLATE_WORKSPACE,
              "LM inflate workspace too small");
static_assert(sizeof(FastWorkspace) <= LM_STATE_DEFLATE_WORKSPACE,
              "LM fast codec workspace too small");
static_assert(sizeof(unsigned int) == 4, "LM codec sizes must be 32 bits");

typedef __UINTPTR_TYPE__ Address;
bool rangeValid(const void *data, unsigned int size) {
    return !size || (data && reinterpret_cast<Address>(data) <= ~Address(0) - size);
}
bool overlap(const void *a, unsigned int as, const void *b, unsigned int bs) {
    return as && bs && reinterpret_cast<Address>(a) < reinterpret_cast<Address>(b) + bs &&
        reinterpret_cast<Address>(b) < reinterpret_cast<Address>(a) + as;
}
bool workValid(void *workspace) {
    return workspace && !(reinterpret_cast<Address>(workspace) & (alignof(FastWorkspace) - 1)) &&
        rangeValid(workspace, LM_STATE_DEFLATE_WORKSPACE);
}
bool packValid(const unsigned char *src, unsigned int size,
    const LmStateSegment *segments, void *workspace) {
    if (!src || !rangeValid(src, size) || !workValid(workspace) ||
        overlap(src, size, workspace, LM_STATE_DEFLATE_WORKSPACE)) return false;
    if (!segments) return true;
    if (!rangeValid(segments, 2 * sizeof(*segments)) ||
        overlap(segments, 2 * sizeof(*segments), workspace, LM_STATE_DEFLATE_WORKSPACE) ||
        segments[0].size > 0xFFFFFFFFu - segments[1].size) return false;
    for (unsigned int i = 0; i < 2; ++i) {
        const LmStateSegment &s = segments[i];
        if (!s.data) continue; // A null segment counts a deliberately omitted prefix.
        if (!rangeValid(s.data, s.size) || overlap(s.data, s.size, src, size) ||
            overlap(s.data, s.size, workspace, LM_STATE_DEFLATE_WORKSPACE) ||
            overlap(s.data, s.size, segments, 2 * sizeof(*segments))) return false;
    }
    return !segments[0].data || !segments[1].data ||
        !overlap(segments[0].data, segments[0].size, segments[1].data, segments[1].size);
}
bool unpackValid(const LmStateSegment *segments, unsigned int packedSize,
    unsigned char *dst, unsigned int expected, void *workspace) {
    if (!segments || !packedSize || !workValid(workspace) ||
        !rangeValid(segments, 2 * sizeof(*segments)) ||
        overlap(segments, 2 * sizeof(*segments), workspace, LM_STATE_DEFLATE_WORKSPACE) ||
        segments[0].size > 0xFFFFFFFFu - segments[1].size ||
        packedSize > segments[0].size + segments[1].size) return false;
    if (dst && (!rangeValid(dst, expected) ||
        overlap(dst, expected, workspace, LM_STATE_DEFLATE_WORKSPACE) ||
        overlap(dst, expected, segments, 2 * sizeof(*segments)))) return false;
    unsigned int remaining = packedSize;
    for (unsigned int i = 0; i < 2; ++i) {
        const unsigned int used = remaining < segments[i].size ? remaining : segments[i].size;
        if (!rangeValid(segments[i].data, used) ||
            overlap(segments[i].data, used, workspace, LM_STATE_DEFLATE_WORKSPACE) ||
            (dst && overlap(segments[i].data, used, dst, expected))) return false;
        remaining -= used;
    }
    return true;
}

struct PackSink {
    const LmStateSegment *segments;
    unsigned int written;
};

int packOutput(const void *data, int length, void *context) {
    PackSink *sink = static_cast<PackSink *>(context);
    if (length < 0 || static_cast<unsigned int>(length) > 0xFFFFFFFFu - sink->written) return 0;
    const unsigned char *src = static_cast<const unsigned char *>(data);
    unsigned int offset = sink->written, remaining = static_cast<unsigned int>(length);
    sink->written += remaining;
    if (!sink->segments) return 1;
    for (unsigned int i = 0; i < 2 && remaining; ++i) {
        const LmStateSegment &segment = sink->segments[i];
        if (offset >= segment.size) { offset -= segment.size; continue; }
        const unsigned int available = segment.size - offset;
        const unsigned int bytes = remaining < available ? remaining : available;
        if (segment.data) memcpy(segment.data + offset, src, bytes);
        src += bytes; remaining -= bytes; offset = 0;
    }
    return 1;
}

unsigned int readWord(const unsigned char *p) {
    return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) | (unsigned(p[2]) << 8) | p[3];
}
bool packWord(PackSink &sink, unsigned int value) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(value >> 24),
        static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value)};
    return packOutput(bytes, sizeof(bytes), &sink) != 0;
}
struct PackReader {
    const LmStateSegment *segments;
    unsigned int position, limit;
    const unsigned char *take(unsigned int size, unsigned char *scratch) {
        if (size > limit - position) return NULL;
        const unsigned int bank = position < segments[0].size ? 0 : 1;
        const unsigned int offset = bank ? position - segments[0].size : position;
        const unsigned int room = segments[bank].size - offset;
        if (size <= room) {
            position += size;
            return segments[bank].data + offset;
        }
        // Only two spans exist, so a crossing request always begins in bank 0.
        memcpy(scratch, segments[0].data + offset, room);
        memcpy(scratch + room, segments[1].data, size - room);
        position += size;
        return scratch;
    }
};
int fastInflate(PackReader &reader, unsigned char *dst, unsigned int expected,
    FastWorkspace *work) {
    unsigned char header[8];
    const unsigned char *words = reader.take(8, header);
    if (!words || readWord(words) != LM_STATE_FAST_MAGIC ||
        readWord(words + 4) != LM_STATE_FAST_BLOCK) return 0;
    unsigned int decoded = 0, adler = MZ_ADLER32_INIT;
    while (decoded < expected) {
        words = reader.take(8, header);
        if (!words) return 0;
        const unsigned int raw = readWord(words), flags = readWord(words + 4);
        const unsigned int packed = flags & 0x7FFFFFFFu;
        const bool plain = (flags & 0x80000000u) != 0;
        const unsigned int block = expected - decoded < LM_STATE_FAST_BLOCK ?
            expected - decoded : LM_STATE_FAST_BLOCK;
        if (raw != block || !packed || (plain ? packed != raw : packed >= raw)) return 0;
        const unsigned char *bytes = reader.take(packed, work->packed);
        if (!bytes) return 0;
        unsigned char *target = dst ? dst + decoded : work->raw;
        if (!plain) {
            if (LZ4_decompress_safe(reinterpret_cast<const char *>(bytes),
                reinterpret_cast<char *>(target), packed, raw) != static_cast<int>(raw)) return 0;
            bytes = target;
        } else if (dst) memcpy(target, bytes, raw);
        adler = mz_adler32(adler, bytes, raw);
        decoded += raw;
    }
    words = reader.take(4, header);
    return words && reader.position == reader.limit && readWord(words) == adler;
}
}

extern "C" unsigned int LmStateDeflate(const unsigned char *src, unsigned int size,
    const LmStateSegment segments[2], void *workspace) {
    if (!packValid(src, size, segments, workspace)) return 0;
    tdefl_compressor *state = static_cast<tdefl_compressor *>(workspace);
    PackSink sink = {segments, 0};
    if (tdefl_init(state, packOutput, &sink,
            TDEFL_WRITE_ZLIB_HEADER | 128) != TDEFL_STATUS_OKAY) return 0;
    if (tdefl_compress_buffer(state, src, size, TDEFL_FINISH) != TDEFL_STATUS_DONE) return 0;
    return sink.written;
}

extern "C" unsigned int LmStateDeflateFast(const unsigned char *src, unsigned int size,
    const LmStateSegment segments[2], void *workspace) {
    if (!packValid(src, size, segments, workspace)) return 0;
    FastWorkspace *work = static_cast<FastWorkspace *>(workspace);
    PackSink sink = {segments, 0};
    unsigned int adler = MZ_ADLER32_INIT;
    if (!packWord(sink, LM_STATE_FAST_MAGIC) || !packWord(sink, LM_STATE_FAST_BLOCK)) return 0;
    for (unsigned int done = 0; done < size;) {
        const unsigned int raw = size - done < LM_STATE_FAST_BLOCK ? size - done : LM_STATE_FAST_BLOCK;
        const unsigned char *bytes = src + done;
        const int packed = LZ4_compress_fast_extState(&work->state,
            reinterpret_cast<const char *>(bytes), reinterpret_cast<char *>(work->packed),
            raw, sizeof(work->packed), 1);
        if (packed <= 0) return 0;
        adler = mz_adler32(adler, bytes, raw);
        const bool plain = static_cast<unsigned int>(packed) >= raw;
        if (!packWord(sink, raw) || !packWord(sink, plain ? raw | 0x80000000u : packed) ||
            !packOutput(plain ? bytes : work->packed, plain ? raw : packed, &sink)) return 0;
        done += raw;
    }
    return packWord(sink, adler) ? sink.written : 0;
}

extern "C" int LmStateInflate(const LmStateSegment segments[2], unsigned int packedSize,
    unsigned char *dst, unsigned int expected, void *workspace) {
    if (!unpackValid(segments, packedSize, dst, expected, workspace)) return 0;
    if (packedSize >= 4) {
        PackReader reader = {segments, 0, packedSize};
        unsigned char prefix[4];
        const unsigned char *magic = reader.take(4, prefix);
        if (readWord(magic) == LM_STATE_FAST_MAGIC) {
            reader.position = 0;
            return fastInflate(reader, dst, expected, static_cast<FastWorkspace *>(workspace));
        }
    }
    InflateWorkspace *work = static_cast<InflateWorkspace *>(workspace);
    tinfl_init(&work->state);
    unsigned int input = 0, output = 0;
    for (;;) {
        const unsigned int bank = input < segments[0].size ||
            (input == packedSize && packedSize <= segments[0].size) ? 0 : 1;
        const unsigned int bankOffset = bank ? input - segments[0].size : input;
        if (bankOffset > segments[bank].size || !segments[bank].data) return 0;
        size_t consumed = segments[bank].size - bankOffset;
        if (consumed > packedSize - input) consumed = packedSize - input;
        const unsigned int offset = dst ? output : output & (TINFL_LZ_DICT_SIZE - 1);
        unsigned char *base = dst ? dst : work->ring;
        size_t produced = dst ? expected - output : TINFL_LZ_DICT_SIZE - offset;
        unsigned int flags = TINFL_FLAG_PARSE_ZLIB_HEADER;
        if (input + consumed < packedSize) flags |= TINFL_FLAG_HAS_MORE_INPUT;
        if (dst) flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        const tinfl_status status = tinfl_decompress(&work->state,
            segments[bank].data + bankOffset, &consumed, base, base + offset, &produced, flags);
        input += static_cast<unsigned int>(consumed);
        if (produced > expected - output) return 0;
        output += static_cast<unsigned int>(produced);
        if (status == TINFL_STATUS_DONE) return input == packedSize && output == expected;
        if (status < 0 || (!consumed && !produced) ||
            (status == TINFL_STATUS_NEEDS_MORE_INPUT && input == packedSize)) return 0;
    }
}
