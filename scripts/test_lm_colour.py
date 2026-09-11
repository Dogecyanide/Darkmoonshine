"""Execute the PPC-shared CMPR recolour algorithm natively and verify pixels."""

from __future__ import annotations

import ctypes
import os
import random
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def palette(block: bytes) -> list[tuple[int, int, int, int]]:
    first, second = struct.unpack_from(">HH", block)
    def rgb(colour):
        return ((colour >> 11) * 255 // 31,
                ((colour >> 5) & 63) * 255 // 63, (colour & 31) * 255 // 31, 255)
    a, b = rgb(first), rgb(second)
    if first > second:
        c = tuple((2 * a[i] + b[i]) // 3 for i in range(3)) + (255,)
        d = tuple((a[i] + 2 * b[i]) // 3 for i in range(3)) + (255,)
    else:
        c = tuple((a[i] + b[i]) // 2 for i in range(3)) + (255,)
        d = (0, 0, 0, 0)
    return [a, b, c, d]


def pixels(block: bytes) -> list[tuple[int, int, int, int]]:
    colours = palette(block)
    selectors = int.from_bytes(block[4:], "big")
    return [colours[(selectors >> (30 - i * 2)) & 3] for i in range(16)]


@unittest.skipUnless(os.name == "nt" and (ROOT / "toolchain/clang.exe").exists(),
                     "native Windows test compiler unavailable")
class NativeColourTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="lm-colour-test-")
        directory = Path(cls.temporary.name)
        obj = directory / "colour.obj"
        library = directory / "colour.dll"
        subprocess.run([str(ROOT / "toolchain/clang.exe"),
                        "--target=x86_64-pc-windows-msvc", "-O2", "-fno-stack-protector",
                        "-c", str(ROOT / "scripts/lm_colour_test_bridge.c"),
                        "-I", str(ROOT / "include"), "-o", str(obj)], check=True,
                       capture_output=True, text=True)
        subprocess.run([str(ROOT / "toolchain/lld-link.exe"), "/dll", "/noentry",
                        "/nodefaultlib", f"/out:{library}", str(obj)], check=True,
                       capture_output=True, text=True)
        cls.library = ctypes.CDLL(str(library))
        cls.colour = cls.library.colour_block
        cls.colour.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                              ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
        cls.colour.restype = None

    @classmethod
    def tearDownClass(cls):
        handle = cls.library._handle
        del cls.colour
        del cls.library
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(handle))
        cls.temporary.cleanup()

    def recolour(self, block, rgb):
        source = ctypes.create_string_buffer(block, 8)
        out = ctypes.create_string_buffer(8)
        self.colour(out, source, *rgb)
        return out.raw

    def test_non_green_endpoints_preserve_exact_bytes(self):
        for endpoints in [(0xF800, 0x001F), (0xFFFF, 0), (0xDAD0, 0x9A00), (0, 0xFFFF)]:
            block = struct.pack(">HHI", *endpoints, 0x1BE43972)
            for colour in [(255, 0, 0), (0, 0, 255), (0, 0, 0), (255, 255, 255)]:
                self.assertEqual(self.recolour(block, colour), block)

    def test_opaque_endpoint_order_swap_remaps_all_four_indices(self):
        original = struct.pack(">HHI", 0x07E0, 0x001F, 0x1B1B1B1B)
        output = self.recolour(original, (0, 0, 128))
        self.assertGreater(int.from_bytes(output[:2], "big"), int.from_bytes(output[2:4], "big"))
        self.assertEqual(int.from_bytes(output[4:], "big"), 0x4E4E4E4E)
        self.assertEqual(pixels(output)[0], (0, 0, 131, 255))
        self.assertEqual(pixels(output)[1], (0, 0, 255, 255))

    def test_transparent_mode_swap_keeps_transparent_and_midpoint_selectors(self):
        original = struct.pack(">HHI", 0x07E0, 0x8000, 0x1B1B1B1B)
        output = self.recolour(original, (255, 0, 0))
        self.assertLessEqual(int.from_bytes(output[:2], "big"), int.from_bytes(output[2:4], "big"))
        self.assertEqual(int.from_bytes(output[4:], "big"), 0x4B4B4B4B)
        self.assertEqual([p[3] for p in pixels(output)], [p[3] for p in pixels(original)])
        self.assertEqual(pixels(output)[0], (255, 0, 0, 255))
        self.assertEqual(pixels(output)[1], (131, 0, 0, 255))

    def test_black_collapse_is_black_and_never_becomes_transparent(self):
        original = struct.pack(">HHI", 0x07E0, 0x03E0, 0x1B1B1B1B)
        output = self.recolour(original, (0, 0, 0))
        self.assertEqual(pixels(output), [(0, 0, 0, 255)] * 16)

    def test_random_inputs_always_preserve_alpha_at_every_pixel(self):
        randomizer = random.Random(0x474C4D4A)
        for _ in range(5000):
            original = randomizer.randbytes(8)
            rgb = tuple(randomizer.randrange(256) for _ in range(3))
            output = self.recolour(original, rgb)
            self.assertEqual([p[3] for p in pixels(output)], [p[3] for p in pixels(original)])

    def test_recolouring_uses_immutable_original_so_settings_do_not_accumulate(self):
        original = struct.pack(">HHI", 0x47E0, 0x2380, 0x1BE4DEAD)
        before = bytes(original)
        self.recolour(original, (255, 0, 0))
        self.recolour(original, (0, 0, 0))
        output = self.recolour(original, (255, 255, 255))
        self.assertEqual(original, before)
        self.assertTrue(all(abs(p[0] - p[1]) <= 8 and abs(p[1] - p[2]) <= 8
                            for p in pixels(output)))


if __name__ == "__main__":
    unittest.main()
