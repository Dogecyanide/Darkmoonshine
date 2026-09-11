"""Exercise the exact heapless PPC deflate bridge with guarded native buffers."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = 0x50000


class Segment(ctypes.Structure):
    _fields_ = [('data', ctypes.c_void_p), ('size', ctypes.c_uint)]


class DeflateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler and Path('C:/msys64/mingw64/bin/g++.exe').exists():
            compiler = 'C:/msys64/mingw64/bin/g++.exe'
        if not compiler:
            raise unittest.SkipTest('Native C++ compiler required')
        cls.temp = tempfile.TemporaryDirectory(prefix='lm-deflate-')
        dll = Path(cls.temp.name) / ('codec.dll' if os.name == 'nt' else 'codec.so')
        env = os.environ.copy()
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
        subprocess.run([compiler, '-shared', '-O2', '-std=c++17', '-fPIC',
                        '-I', str(ROOT / 'include'),
                        str(ROOT / 'lm_diag/src/lm_state_deflate.cpp'),
                        str(ROOT / 'scripts/lm_state_deflate_harness.cpp'), '-o', str(dll)],
                       check=True, env=env, capture_output=True)
        cls.lib = ctypes.CDLL(str(dll))
        cls.lib.LmStateDeflate.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                         ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.LmStateDeflate.restype = ctypes.c_uint
        cls.lib.LmStateInflate.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                         ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p]
        cls.lib.LmStateInflate.restype = ctypes.c_int
        cls.lib.staging_tail.argtypes = [ctypes.c_uint] * 4
        cls.lib.staging_tail.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        if os.name == 'nt':
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.workspace = ctypes.create_string_buffer(WORKSPACE + 32)
        ctypes.memset(self.workspace, 0xCD, len(self.workspace))

    def tearDown(self):
        self.assertEqual(self.workspace.raw[WORKSPACE:], b'\xCD' * 32)

    def segments(self, data, split):
        self.a = ctypes.create_string_buffer(data[:split])
        self.b = ctypes.create_string_buffer(data[split:])
        return (Segment * 2)(Segment(ctypes.addressof(self.a), split),
                             Segment(ctypes.addressof(self.b), len(data) - split))

    def packed(self, data, split=None, capacity=None):
        needed = self.lib.LmStateDeflate(data, len(data), None, self.workspace)
        self.assertGreater(needed, 0)
        capacity = needed if capacity is None else capacity
        split = capacity // 2 if split is None else min(split, capacity)
        a = ctypes.create_string_buffer(split + 32)
        b = ctypes.create_string_buffer(capacity - split + 32)
        ctypes.memset(a, 0xAC, len(a))
        ctypes.memset(b, 0xAD, len(b))
        segments = (Segment * 2)(Segment(ctypes.addressof(a), split),
                                 Segment(ctypes.addressof(b), capacity - split))
        self.assertEqual(self.lib.LmStateDeflate(data, len(data), segments, self.workspace), needed)
        self.assertEqual(a.raw[split:], b'\xAC' * 32)
        self.assertEqual(b.raw[capacity-split:], b'\xAD' * 32)
        return (a.raw[:split] + b.raw[:capacity-split])[:needed], needed

    def decode(self, packed, expected, split):
        segments = self.segments(packed, split)
        dry = self.lib.LmStateInflate(segments, len(packed), None, expected, self.workspace)
        output = ctypes.create_string_buffer(expected + 32)
        ctypes.memset(output, 0xEF, len(output))
        actual = self.lib.LmStateInflate(segments, len(packed), output, expected, self.workspace)
        self.assertEqual(dry, actual)
        self.assertEqual(output.raw[expected:], b'\xEF' * 32)
        return actual, output.raw[:expected]

    def test_roundtrip_two_segment_boundaries(self):
        for data in (b'', b'a', bytes(range(251)) * 997, bytes(13 * 1024 * 1024),
                     random.Random(305).randbytes(400000)):
            packed, _ = self.packed(data)
            self.assertEqual(zlib.decompress(packed), data)
            for split in (0, 1, 2, len(packed)//2, len(packed)-1, len(packed)):
                self.assertEqual(self.decode(packed, len(data), split), (1, data))

    def test_capacity_failure_is_counted_and_guarded(self):
        data = random.Random(17).randbytes(100000)
        for capacity in (0, 1, 2, 31, 32767, 70000):
            partial, needed = self.packed(data, capacity=capacity)
            self.assertEqual(len(partial), capacity)
            self.assertGreater(needed, capacity)

    def test_truncated_corrupt_appended_streams(self):
        data = bytes(range(251)) * 400
        packed, _ = self.packed(data)
        for cut in range(len(packed)):
            self.assertEqual(self.decode(packed[:cut], len(data), cut//2)[0], 0)
        for index in (0, 1, 7, len(packed)//2, len(packed)-1):
            bad = packed[:index] + bytes([packed[index] ^ 0x80]) + packed[index+1:]
            self.assertEqual(self.decode(bad, len(data), len(bad)//2)[0], 0)
        self.assertEqual(self.decode(packed+b'X', len(data), len(packed))[0], 0)
        for size in (len(data)-1, len(data)+1, 0, 31):
            self.assertEqual(self.decode(packed, size, len(packed)//2)[0], 0)

    def test_external_deflate_streams_and_window_wraps(self):
        data = random.Random(334).randbytes(80000) + bytes(500000)
        for level in (0, 1, 6, 9):
            packed = zlib.compress(data, level)
            self.assertEqual(self.decode(packed, len(data), 32768), (1, data))

    def test_random_malformed_streams_are_bounded(self):
        rng = random.Random(835)
        for _ in range(1000):
            data = rng.randbytes(rng.randrange(1, 200))
            self.decode(data, rng.randrange(1000), len(data)//2)

    def test_staging_extent_checks(self):
        tail = self.lib.staging_tail
        limit = 0xFD0000
        for old, target in ((0, 0), (12767712, 12767712), (8000000, 14000000),
                            (14000000, 8000000), (limit-6400, limit-6400)):
            result = tail(old, target, 6400, limit)
            self.assertGreaterEqual(result, max(old, target)+6400)
            self.assertLessEqual(result, limit)
            self.assertEqual(result % 32, 0)
        for args in ((0xFFFFFFFF, 0, 6400, limit), (0, limit, 6400, limit),
                     (0, 0, limit+1, limit), (0xFFFFFFF0, 0, 0, 0xFFFFFFFF)):
            self.assertEqual(tail(*args), 0xFFFFFFFF)

    def test_larger_target_failure_preserves_rollback_staging(self):
        # Use the same alias pattern as Wii: second packed segment is physically
        # inside the raw arena, beyond BOTH old and incoming state extents.
        old = random.Random(483).randbytes(700000)
        target = random.Random(543).randbytes(1000000)
        limit = 1400000
        raw = ctypes.create_string_buffer(limit+32)
        ctypes.memset(raw, 0xAE, len(raw))
        ctypes.memmove(raw, old, len(old))
        primary = ctypes.create_string_buffer(450000+32)
        ctypes.memset(primary, 0xAC, len(primary))
        tail = self.lib.staging_tail(len(old), len(target), 0, limit)
        staging = (Segment * 2)(Segment(ctypes.addressof(primary), 450000),
                               Segment(ctypes.addressof(raw)+tail, limit-tail))
        packed_old = self.lib.LmStateDeflate(raw, len(old), staging, self.workspace)
        self.assertGreater(packed_old, 450000)
        self.assertLess(packed_old, 450000+limit-tail)
        cached, _ = self.packed(target)
        source = self.segments(cached, len(cached))
        self.assertEqual(self.lib.LmStateInflate(source, len(cached), None,
                                                len(target), self.workspace), 1)
        self.assertEqual(raw.raw[:len(old)], old)
        self.assertEqual(self.lib.LmStateInflate(source, len(cached), raw,
                                                len(target), self.workspace), 1)
        # A target header/layout failure now forces the exact rollback path.
        self.assertEqual(self.lib.LmStateInflate(staging, packed_old, raw,
                                                len(old), self.workspace), 1)
        self.assertEqual(raw.raw[:len(old)], old)
        self.assertEqual(raw.raw[limit:], b'\xAE'*32)
        self.assertEqual(primary.raw[450000:], b'\xAC'*32)

    def test_actual_capture_when_available(self):
        capture = ROOT / 'build-lm-emu/diagnostic-capture-0.3.30/fake-vmem.bin'
        if not capture.exists():
            self.skipTest('Local private Dolphin memory capture unavailable')
        data = capture.read_bytes()
        size = struct.unpack_from('>I', data, 16)[0]
        data = data[:size+6400]
        packed, needed = self.packed(data, split=0x400000-0x50100)
        self.assertLess(needed, 0x57F000-32)
        self.assertGreater(needed, 0x400000)
        self.assertEqual(self.decode(packed, len(data), 0x400000-0x50100), (1, data))
        print(f'Actual captured LM snapshot: {len(data)} raw, {needed} deflate bytes')


if __name__ == '__main__':
    unittest.main()
