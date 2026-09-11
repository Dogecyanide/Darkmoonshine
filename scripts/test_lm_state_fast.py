"""Execute production LML4 codec with guarded buffers and independent decoding."""
import ctypes
import random
import struct
import unittest
import zlib

from lm_quick_codec import BLOCK, MAGIC, decode_fast
import test_lm_state_deflate as legacy

Segment = legacy.Segment


class FastTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        legacy.DeflateTests.setUpClass.__func__(cls)
        cls.lib.LmStateDeflateFast.argtypes = cls.lib.LmStateDeflate.argtypes
        cls.lib.LmStateDeflateFast.restype = ctypes.c_uint
        cls.lib.fast_move.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
        cls.lib.fast_move.restype = ctypes.c_void_p

    tearDownClass = classmethod(legacy.DeflateTests.tearDownClass.__func__)
    setUp = legacy.DeflateTests.setUp
    tearDown = legacy.DeflateTests.tearDown
    segments = legacy.DeflateTests.segments
    decode = legacy.DeflateTests.decode

    def packed(self, data, capacity=None, split=None):
        needed = self.lib.LmStateDeflateFast(data, len(data), None, self.workspace)
        self.assertGreater(needed, 0)
        if capacity is None:
            capacity = needed
        split = capacity // 2 if split is None else split
        a = ctypes.create_string_buffer(split + 32)
        b = ctypes.create_string_buffer(capacity - split + 32)
        ctypes.memset(a, 0xAC, len(a))
        ctypes.memset(b, 0xAD, len(b))
        spans = (Segment * 2)(Segment(ctypes.addressof(a), split),
                             Segment(ctypes.addressof(b), capacity - split))
        actual = self.lib.LmStateDeflateFast(data, len(data), spans, self.workspace)
        self.assertEqual(actual, needed)
        self.assertEqual(a.raw[split:], b'\xAC' * 32)
        self.assertEqual(b.raw[capacity-split:], b'\xAD' * 32)
        return a.raw[:split] + b.raw[:capacity-split], needed

    def test_empty_and_block_boundaries(self):
        for size in (0, 1, 12, 13, BLOCK-1, BLOCK, BLOCK+1, 2*BLOCK, 2*BLOCK+123):
            raw = (bytes(range(251))*((size+250)//251))[:size]
            packed, _ = self.packed(raw)
            self.assertEqual(packed[:8], struct.pack('>II', MAGIC, BLOCK))
            self.assertEqual(decode_fast(packed, size), raw)
            for split in (0, 1, 4, 7, len(packed)//2, len(packed)-4, len(packed)-1, len(packed)):
                self.assertEqual(self.decode(packed, size, split), (1, raw))

    def test_raw_fallback_and_mixed_blocks(self):
        raw = random.Random(421).randbytes(BLOCK) + bytes(BLOCK) + b'final'
        packed, _ = self.packed(raw)
        self.assertEqual(struct.unpack_from('>II', packed, 8), (BLOCK, BLOCK | 0x80000000))
        self.assertLess(struct.unpack_from('>I', packed, 20+BLOCK)[0], BLOCK)
        self.assertEqual(decode_fast(packed, len(raw)), raw)
        self.assertEqual(self.decode(packed, len(raw), BLOCK//2), (1, raw))

    def test_every_span_boundary(self):
        raw = b'abc123'*700
        packed, _ = self.packed(raw)
        for split in range(len(packed)+1):
            self.assertEqual(self.decode(packed, len(raw), split), (1, raw))

    def test_bounded_capacity_returns_exact_prefix(self):
        raw = random.Random(12).randbytes(BLOCK+101)
        full, needed = self.packed(raw)
        for capacity in (0, 1, 7, 8, 15, 16, 31, BLOCK, needed-1):
            partial, actual = self.packed(raw, capacity)
            self.assertEqual(actual, needed)
            self.assertEqual(partial, full[:capacity])

    def test_null_segments_count_and_skip(self):
        raw = b'R'*40000
        full, needed = self.packed(raw)
        tail = ctypes.create_string_buffer(needed+32)
        spans = (Segment * 2)(Segment(None, 9), Segment(ctypes.addressof(tail), needed-9))
        self.assertEqual(self.lib.LmStateDeflateFast(raw, len(raw), spans, self.workspace), needed)
        self.assertEqual(tail.raw[:needed-9], full[9:])
        spans = (Segment * 2)(Segment(None, 0x100), Segment(None, 0x100))
        self.assertEqual(self.lib.LmStateDeflateFast(raw, len(raw), spans, self.workspace), needed)

    def reject(self, packed, expected):
        self.assertEqual(self.decode(packed, expected, len(packed)//2)[0], 0)
        with self.assertRaises(ValueError):
            decode_fast(packed, expected)

    def test_truncation_corruption_extra_bytes_and_lengths(self):
        raw = b'abcdefg'*40000
        packed, _ = self.packed(raw)
        for cut in range(len(packed)):
            self.reject(packed[:cut], len(raw))
        for index in (0, 3, 7, 11, 15, 17, len(packed)//2, len(packed)-1):
            bad = bytearray(packed)
            bad[index] ^= 0x80
            self.reject(bytes(bad), len(raw))
        for suffix in (b'\0', b'\0'*4, packed):
            self.reject(packed+suffix, len(raw))
        for expected in (0, 1, len(raw)-1, len(raw)+1, BLOCK):
            self.reject(packed, expected)

    def test_malformed_block_header_and_match(self):
        for raw, flags in ((0, 0), (33, 0), (32, 33), (32, 32),
                           (32, 0x80000001), (BLOCK+1, 1), (32, 0xFFFFFFFF)):
            self.reject(struct.pack('>IIII', MAGIC, BLOCK, raw, flags)+bytes(40), 32)
        # Valid frame extents but zero/out-of-history offsets, excess literal or match.
        for block in (b'\x10A\0\0\x50abcde', b'\x10A\x02\0\x50abcde',
                      b'\xff\xff\xff', b'\x1fA\x01\0\xff\xff'):
            self.reject(struct.pack('>IIII', MAGIC, BLOCK, 32, len(block))+block+bytes(4), 32)

    def test_external_raw_frame(self):
        raw = random.Random(191).randbytes(BLOCK+3)
        packed = struct.pack('>II', MAGIC, BLOCK)
        for start in range(0, len(raw), BLOCK):
            block = raw[start:start+BLOCK]
            packed += struct.pack('>II', len(block), 0x80000000|len(block)) + block
        packed += struct.pack('>I', zlib.adler32(raw))
        self.assertEqual(self.decode(packed, len(raw), 13), (1, raw))
        self.assertEqual(decode_fast(packed, len(raw)), raw)

    def test_random_malformed_fast_frames_remain_bounded(self):
        rng = random.Random(352)
        for _ in range(1000):
            packed = struct.pack('>II', MAGIC, BLOCK) + rng.randbytes(rng.randrange(1, 250))
            self.reject(packed, rng.randrange(10000))

    def test_alias_and_argument_rejection_do_not_write(self):
        raw = ctypes.create_string_buffer(b'abcd'*128)
        before = raw.raw
        spans = (Segment * 2)(Segment(ctypes.addressof(raw), 32), Segment(None, 0))
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, spans, self.workspace), 0)
        spans[0].data = ctypes.addressof(self.workspace)
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, spans, self.workspace), 0)
        self.assertEqual(self.lib.LmStateDeflateFast(self.workspace, 32, None, self.workspace), 0)
        spans[0].data = ctypes.addressof(spans)
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, spans, self.workspace), 0)
        self.assertEqual(self.lib.LmStateDeflateFast(None, 512, None, self.workspace), 0)
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, None, None), 0)
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, None, ctypes.addressof(self.workspace)+1), 0)
        out = ctypes.create_string_buffer(100)
        spans = (Segment * 2)(Segment(ctypes.addressof(out), 50), Segment(ctypes.addressof(out)+25, 50))
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, spans, self.workspace), 0)
        spans = (Segment * 2)(Segment(None, 0xFFFFFFFF), Segment(None, 1))
        self.assertEqual(self.lib.LmStateDeflateFast(raw, 512, spans, self.workspace), 0)
        self.assertEqual(raw.raw, before)

    def test_decoder_alias_and_unused_span_capacity(self):
        raw = bytes(range(64))*40
        packed, _ = self.packed(raw)
        spans = self.segments(packed, len(packed))
        before = self.a.raw
        self.assertEqual(self.lib.LmStateInflate(spans, len(packed), self.a, len(raw), self.workspace), 0)
        self.assertEqual(self.lib.LmStateInflate(spans, len(packed), self.workspace, len(raw), self.workspace), 0)
        self.assertEqual(self.a.raw, before)
        spans[1].data = None  # Entirely unused second span need not be readable.
        spans[1].size = 100
        self.assertEqual(self.lib.LmStateInflate(spans, len(packed), None, len(raw), self.workspace), 1)

    def test_adler_covers_raw_fallback_bytes(self):
        raw = random.Random(47).randbytes(300)
        packed, _ = self.packed(raw)
        bad = bytearray(packed)
        bad[20] ^= 1
        self.reject(bytes(bad), len(raw))

    def test_private_memmove_overlapping_both_directions(self):
        for source, destination, size in ((0, 0, 0), (0, 0, 128), (0, 1, 127),
                                         (1, 0, 127), (3, 29, 70), (29, 3, 70),
                                         (0, 64, 64), (64, 0, 64)):
            before = bytes(range(128))
            expected = bytearray(before)
            expected[destination:destination+size] = before[source:source+size]
            buffer = ctypes.create_string_buffer(before)
            result = self.lib.fast_move(ctypes.addressof(buffer)+destination,
                                       ctypes.addressof(buffer)+source, size)
            self.assertEqual(result, ctypes.addressof(buffer)+destination)
            self.assertEqual(buffer.raw[:128], expected)
        self.assertIsNone(self.lib.fast_move(None, None, 0))


if __name__ == '__main__':
    unittest.main()
