"""Run the actual freestanding LM codec/auth code against hostile byte streams."""
from __future__ import annotations

import ctypes
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LmStateStorageNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required for the codec/auth tests")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-codec-")
        output = Path(cls.temp.name) / ("codec.dll" if os.name == "nt" else "codec.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_state_codec_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace") or "Native C compilation failed")
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.pack.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint]
        cls.lib.pack.restype = ctypes.c_uint
        cls.lib.unpack.argtypes = cls.lib.pack.argtypes
        cls.lib.unpack.restype = ctypes.c_int
        cls.lib.authenticate.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint64, ctypes.c_uint64]
        cls.lib.authenticate.restype = ctypes.c_uint64

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def roundtrip(self, data: bytes):
        output = ctypes.create_string_buffer(len(data) + (len(data) // 32768 + 1) * 2 + 32)
        size = self.lib.pack(data, len(data), output, len(output))
        self.assertEqual(self.lib.pack(data, len(data), None, 0xFFFFFFFF), size)
        if data:
            self.assertGreater(size, 0)
        self.assertEqual(self.lib.unpack(output, size, None, len(data)), 1)
        decoded = ctypes.create_string_buffer(len(data) + 32)
        ctypes.memset(decoded, 0xAC, len(decoded))
        self.assertEqual(self.lib.unpack(output, size, decoded, len(data)), 1)
        self.assertEqual(decoded.raw[:len(data)], data)
        self.assertEqual(decoded.raw[len(data):], b"\xAC" * 32)
        return size

    def test_boundaries_and_overlapping_matches(self):
        for size in (0, 1, 2, 3, 4, 5, 32768, 32771, 65535, 65536, 1000000):
            for seed in (b"\0", b"abcd", bytes(range(251))):
                with self.subTest(size=size, period=len(seed)):
                    self.roundtrip((seed * (size // len(seed) + 1))[:size])

    def test_incompressible_and_heap_sized_payloads(self):
        rng = random.Random(925)
        self.roundtrip(rng.randbytes(400000))
        size = self.roundtrip(bytes(13 * 1024 * 1024))
        self.assertLess(size, 4096)

    def test_capacity_failures_never_write_past_destination(self):
        data = random.Random(932).randbytes(100000)
        for capacity in (0, 1, 2, 17, 64, 32767, 65535):
            out = ctypes.create_string_buffer(capacity + 32)
            ctypes.memset(out, 0xDB, len(out))
            self.assertEqual(self.lib.pack(data, len(data), out, capacity), 0)
            self.assertEqual(out.raw[capacity:], b"\xDB" * 32)

    def test_count_only_reports_required_capacity_after_a_short_staging_buffer(self):
        data = random.Random(145).randbytes(4 * 1024 * 1024) + bytes(8 * 1024 * 1024)
        required = self.lib.pack(data, len(data), None, 0xFFFFFFFF)
        self.assertGreater(required, 4 * 1024 * 1024)
        staging = ctypes.create_string_buffer(4 * 1024 * 1024)
        self.assertEqual(self.lib.pack(data, len(data), staging, len(staging)), 0)
        self.assertEqual(self.lib.pack(data, len(data), None, 0xFFFFFFFF), required)

    def test_malformed_streams(self):
        malformed = [(b"\0", 1), (b"\0\0", 1),
                     (b"\x80\0\0\0", 4), (b"\x80\0\0\1", 4),
                     (b"\0\0A\x80\0\0\2", 5),
                     (b"\0\0A\x80\0\0\1", 4),
                     (b"\0\0A\0\0B", 1), (b"\0\0A", 2)]
        for stream, expected in malformed:
            with self.subTest(stream=stream, expected=expected):
                self.assertEqual(self.lib.unpack(stream, len(stream), None, expected), 0)

    def test_every_truncation_of_a_valid_stream_is_rejected(self):
        data = bytes(range(251)) * 200
        out = ctypes.create_string_buffer(len(data) + 128)
        size = self.lib.pack(data, len(data), out, len(out))
        for cut in range(size):
            self.assertEqual(self.lib.unpack(out, cut, None, len(data)), 0)

    def test_random_stream_bounds_match_dry_run(self):
        rng = random.Random(997)
        for _ in range(2000):
            data = rng.randbytes(rng.randrange(128))
            expected = rng.randrange(128)
            output = ctypes.create_string_buffer(expected + 32)
            ctypes.memset(output, 0xAD, len(output))
            dry = self.lib.unpack(data, len(data), None, expected)
            actual = self.lib.unpack(data, len(data), output, expected)
            self.assertEqual(dry, actual)
            self.assertEqual(output.raw[expected:], b"\xAD" * 32)

    def test_siphash_reference_vectors_and_session_authentication(self):
        k0, k1 = 0x0706050403020100, 0x0F0E0D0C0B0A0908
        vectors = {0: 0x726FDB47DD0E0E31, 1: 0x74F839C593DC67FD,
                   2: 0x0D6C8009D9A94F5A, 15: 0xA129CA6149BE45E5}
        for length, expected in vectors.items():
            data = bytes(range(length))
            self.assertEqual(self.lib.authenticate(data, length, k0, k1), expected)
        data = bytes(range(256)) * 100
        tag = self.lib.authenticate(data, len(data), k0, k1)
        self.assertNotEqual(tag, self.lib.authenticate(data, len(data), k0 ^ 1, k1))
        self.assertNotEqual(tag, self.lib.authenticate(b"x" + data[1:], len(data), k0, k1))


class LmStateKernelWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required for worker fault-injection tests")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-worker-")
        output = Path(cls.temp.name) / ("worker.dll" if os.name == "nt" else "worker.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-I", str(ROOT / "include"),
                   "-I", str(ROOT / "launcher/fatfs"),
                   str(ROOT / "scripts/lm_state_kernel_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace") or "Worker compilation failed")
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.request.argtypes = [ctypes.c_uint] * 3
        cls.lib.add_import.argtypes = [ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint]
        cls.lib.snapshot.restype = ctypes.c_void_p
        cls.lib.mailbox.restype = ctypes.c_void_p

    @classmethod
    def tearDownClass(cls):
        cls.lib.reset_worker()
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.lib.reset_worker()

    def drain(self):
        for _ in range(4096):
            if not self.lib.pending():
                return
            self.lib.step()
        self.fail("Storage worker did not terminate")

    def archive(self, payload: bytes, **changes):
        values = dict(magic=0x4C4D5341, version=1, header_size=64,
                      payload_size=len(payload), game=0x474C4D4A,
                      snapshot_version=16, build=123, session=987,
                      raw_size=len(payload) - 256, trailer_size=256,
                      crc=0, generation=1, auth_hi=0, auth_lo=0, reserved0=0, reserved1=0)
        values.update(changes)
        return struct.pack("=16I", *values.values()) + payload

    def test_export_publishes_only_after_complete_synced_write(self):
        self.lib.request(1, 1, 50000)
        for _ in range(7):
            self.lib.step()
            self.assertEqual(self.lib.file_size(1, 0), -1)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.lib.file_size(1, 0), 50064)
        self.assertEqual(self.lib.file_size(1, 1), -1)

    def test_cache_publication_preserves_complete_aligned_patch_prefix(self):
        size = 0x57F000
        maximum_count = (size - 4) // 8
        for count in (0, 1, 3, 4, 4000, maximum_count):
            with self.subTest(count=count):
                self.lib.publish_cache(count)
                prefix = (4 + count * 8 + 31) & ~31
                self.assertEqual(self.lib.cache_base(), 0x11900000 + prefix)
                self.assertEqual(self.lib.cache_size(), size - prefix)
                self.assertEqual(self.lib.cache_base() + self.lib.cache_size(), 0x11E7F000)
        for count in (maximum_count + 1, 0xFFFFFFFF):
            self.lib.publish_cache(count)
            self.assertEqual(self.lib.cache_size(), 0)

    def test_export_never_overwrites_an_existing_archive(self):
        self.lib.add_import(1, b"previous", 8)
        self.lib.request(1, 1, 50000)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.lib.result_id(), 2)
        self.assertEqual(self.lib.file_size(1, 0), 8)
        self.assertEqual(self.lib.file_size(2, 0), 50064)

    def test_all_export_io_failures_leave_previous_archives_intact(self):
        for operation in (1, 3, 4, 5, 6):
            with self.subTest(operation=operation):
                self.lib.reset_worker()
                self.lib.add_import(1, b"previous", 8)
                self.lib.fault(operation, 1)
                self.lib.request(1, 2, 50000)
                self.drain()
                self.assertNotEqual(self.lib.status(), 0)
                self.assertEqual(self.lib.file_size(1, 0), 8)
                self.assertEqual(self.lib.file_size(2, 0), -1)

    def test_import_is_bounded_and_exact(self):
        payload = random.Random(55).randbytes(50000)
        data = self.archive(payload)
        self.lib.add_import(7, data, len(data))
        self.lib.request(2, 7, 0)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 50000), payload)
        self.assertEqual(ctypes.string_at(self.lib.snapshot() + 50000, 32), b"\xAD" * 32)

    def test_malformed_import_header_never_touches_snapshot(self):
        payload = b"x" * 1000
        cases = [dict(raw_size=0xFFFFFFFF), dict(payload_size=0xFFFFFFFF),
                 dict(raw_size=0), dict(trailer_size=0), dict(reserved0=1),
                 dict(game=0x474D534A), dict(header_size=32), dict(version=2)]
        for fields in cases:
            with self.subTest(fields=fields):
                self.lib.reset_worker()
                data = self.archive(payload, **fields)
                self.lib.add_import(7, data, len(data))
                self.lib.request(2, 7, 0)
                self.drain()
                self.assertNotEqual(self.lib.status(), 0)
                self.assertEqual(ctypes.string_at(self.lib.snapshot(), 2000), b"\xAD" * 2000)

    def test_torn_or_extended_file_is_rejected_before_payload(self):
        for delta in (-1, 1):
            self.lib.reset_worker()
            data = self.archive(b"x" * 1000)
            data = data[:-1] if delta == -1 else data + b"x"
            self.lib.add_import(7, data, len(data))
            self.lib.request(2, 7, 0)
            self.drain()
            self.assertNotEqual(self.lib.status(), 0)
            self.assertEqual(ctypes.string_at(self.lib.snapshot(), 2000), b"\xAD" * 2000)

    def test_reset_cancels_old_session_before_another_chunk(self):
        data = self.archive(b"x" * 50000)
        self.lib.add_import(7, data, len(data))
        self.lib.request(2, 7, 0)
        for _ in range(4):
            self.lib.step()
        before = ctypes.string_at(self.lib.snapshot(), 50000)
        self.lib.cancel_session()
        self.drain()
        self.assertNotEqual(self.lib.status(), 0)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 50000), before)


if __name__ == "__main__":
    unittest.main()
