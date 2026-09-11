"""Execute the same bounded SYS-model validator used before runtime colour writes."""
from __future__ import annotations
import ctypes
import os
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x80000000
HEAP, START, END = BASE + 0x1000, BASE + 0x1100, BASE + 0x3F000
MODEL, TABLE = BASE + 0x4000, BASE + 0x9000
TEXTURES = (BASE + 0x10000, BASE + 0x16000)


@unittest.skipUnless(os.name == "nt" and (ROOT / "toolchain/clang.exe").exists(),
                     "native Windows compiler unavailable")
class ColourModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-colour-model-test-")
        directory = Path(cls.temp.name)
        obj, library = directory / "model.obj", directory / "model.dll"
        subprocess.run([str(ROOT / "toolchain/clang.exe"), "--target=x86_64-pc-windows-msvc",
            "-O2", "-fno-stack-protector", "-c", str(ROOT / "scripts/lm_colour_model_test_bridge.c"),
            "-I", str(ROOT / "include"), "-o", str(obj)], check=True, capture_output=True, text=True)
        subprocess.run([str(ROOT / "toolchain/lld-link.exe"), "/dll", "/noentry", "/nodefaultlib",
                        f"/out:{library}", str(obj)], check=True, capture_output=True, text=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.validate.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
                                     ctypes.c_uint, ctypes.c_void_p]
        cls.lib.validate.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(cls.lib._handle))
        del cls.lib
        cls.temp.cleanup()

    def setUp(self):
        self.data = bytearray(0x40000)
        for address, value in {
            HEAP: 0x8038886C, HEAP + 0x30: START, HEAP + 0x34: END,
            HEAP + 0x38: END - START, MODEL: 0x04B40000, MODEL + 4: 0x0CA60000,
            MODEL + 8: 0x0041001C, MODEL + 0x10: 0x06D20C6A,
            MODEL + 0x20: 0x00150000, MODEL + 0x28: 0x00100013,
            MODEL + 0x60: TABLE, TABLE + 12: TEXTURES[0], TABLE + 24: TEXTURES[1],
            TEXTURES[0]: 0x0A000080, TEXTURES[0] + 4: 0x00800000,
            TEXTURES[1]: 0x0A000080, TEXTURES[1] + 4: 0x00800000,
        }.items(): self.put(address, value)

    def put(self, address, value):
        struct.pack_into(">I", self.data, address - BASE, value)

    def validate(self, valid, heap=HEAP, model=MODEL, data=None):
        memory = ctypes.create_string_buffer(bytes(self.data if data is None else data))
        output = (ctypes.c_uint * 4)()
        actual = self.lib.validate(memory, len(memory) - 1, heap, model, output)
        self.assertEqual(bool(actual), valid)
        self.assertEqual(output[2], 0, "validator attempted an unaligned or out-of-bounds read")
        if not valid: self.assertEqual(list(output[:2]), [0, 0])
        return output

    def test_relocated_live_layout_and_on_disc_counts_are_supported(self):
        self.assertEqual(list(self.validate(True)[:2]), list(TEXTURES))
        self.put(MODEL + 0x20, 0x00140000)
        self.put(MODEL + 0x28, 0x00130013)
        self.assertEqual(list(self.validate(True)[:2]), list(TEXTURES))

    def test_invalid_heap_and_model_pointers_are_rejected_before_read(self):
        for heap in (0, 0x7FFFFFFC, 0xFFFFFFFF, HEAP + 1, 0x817FFFE0):
            with self.subTest(heap=heap): self.validate(False, heap=heap)
        for model in (0, 0xFFFFFFFF, MODEL + 1, START - 32, END - 64, END, 0x81800000):
            with self.subTest(model=model): self.validate(False, model=model)

    def test_heap_identity_extent_and_header_signature_fail_closed(self):
        for address, value in [(HEAP, 0), (HEAP + 0x30, END), (HEAP + 0x30, START + 1),
                               (HEAP + 0x34, END - 1), (HEAP + 0x34, START),
                               (HEAP + 0x34, 0xFFFFFFFF), (HEAP + 0x38, 0xFFFFFFFF),
                               (MODEL, 0), (MODEL + 4, 0), (MODEL + 8, 0),
                               (MODEL + 0x10, 0), (MODEL + 0x28, 0x00100014)]:
            with self.subTest(address=hex(address), value=value):
                old = struct.unpack_from(">I", self.data, address - BASE)[0]
                self.put(address, value); self.validate(False); self.put(address, old)

    def test_texture_count_and_complete_table_extent_are_bounded(self):
        for count in (0, 6, 65, 65535):
            self.put(MODEL + 0x20, count << 16); self.validate(False)
        self.put(MODEL + 0x20, 21 << 16)
        for table in (0, 0xFFFFFFFF, TABLE + 1, END - 80, MODEL + 0x40, END):
            self.put(MODEL + 0x60, table); self.validate(False)

    def test_complete_texture_ranges_alignment_and_formats_are_bounded(self):
        for texture in (0, 0xFFFFFFFF, TEXTURES[0] + 1, START - 32, END - 0x2000, END):
            self.put(TABLE + 12, texture); self.validate(False)
        self.put(TABLE + 12, TEXTURES[0])
        for address, value in [(TEXTURES[0], 0x09000080), (TEXTURES[0] + 4, 0x00400000),
                               (TEXTURES[1], 0), (TEXTURES[1] + 4, 0)]:
            old = struct.unpack_from(">I", self.data, address - BASE)[0]
            self.put(address, value); self.validate(False); self.put(address, old)

    def test_texture_header_table_and_other_texture_cannot_overlap(self):
        for second in (TEXTURES[0], TEXTURES[0] + 0x1000):
            self.put(second, 0x0A000080); self.put(second + 4, 0x00800000)
            self.put(TABLE + 24, second); self.validate(False)
        self.put(TABLE + 24, TEXTURES[1])
        self.put(MODEL + 0x40, 0x0A000080); self.put(MODEL + 0x44, 0x00800000)
        self.put(TABLE + 12, MODEL + 0x40); self.validate(False)
        overlapping_table = TEXTURES[0] + 0x100
        self.put(MODEL + 0x60, overlapping_table)
        self.put(overlapping_table + 12, TEXTURES[0]); self.put(overlapping_table + 24, TEXTURES[1])
        self.validate(False)

    def test_random_hostile_pointer_words_never_escape_before_rejection(self):
        rng = random.Random(0x474C4D4A)
        for address in (MODEL + 0x60, TABLE + 12, TABLE + 24):
            old = struct.unpack_from(">I", self.data, address - BASE)[0]
            for _ in range(1000):
                self.put(address, rng.getrandbits(32)); self.validate(False)
            self.put(address, old)

    def test_actual_dolphin_mem1_fixture_resolves_and_authenticates_both_images(self):
        path = ROOT / "build-lm-emu/diagnostic-capture-0.3.30/mem1.bin"
        if not path.exists(): self.skipTest("private Dolphin MEM1 fixture not available")
        data = path.read_bytes()
        word = lambda a: struct.unpack_from(">I", data, a - BASE)[0]
        result = self.validate(True, heap=word(0x804A0B94), model=word(0x803435B4), data=data)
        self.assertEqual(list(result[:2]), [0x80AB6B20, 0x80AB8FA0])
        for texture, crc in zip(result[:2], (0x65714408, 0x4C866065)):
            start = texture - BASE + 32
            self.assertEqual(zlib.crc32(data[start:start + 0x2000]), crc)


if __name__ == "__main__":
    unittest.main()
