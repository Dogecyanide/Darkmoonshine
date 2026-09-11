"""Exercise the actual Moonshine-geometry LM input renderer in native memory."""
from __future__ import annotations
import ctypes
import os
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SIZE = 640 * 480 * 2
BASE = bytes((70, 128, 70, 128)) * (SIZE // 4)


@unittest.skipUnless(os.name == "nt" and (ROOT / "toolchain/clang.exe").exists(),
                     "native Windows compiler unavailable")
class InputRendererTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-input-test-")
        directory = Path(cls.temp.name)
        obj, library = directory / "input.obj", directory / "input.dll"
        subprocess.run([str(ROOT / "toolchain/clang.exe"), "--target=x86_64-pc-windows-msvc",
            "-O2", "-fno-stack-protector", "-fno-exceptions", "-fno-rtti", "-c",
            str(ROOT / "scripts/lm_input_test_bridge.cpp"), "-I", str(ROOT / "include"),
            "-I", str(ROOT / "lm_diag/include"), "-o", str(obj)], check=True,
            capture_output=True, text=True)
        subprocess.run([str(ROOT / "toolchain/lld-link.exe"), "/dll", "/noentry", "/nodefaultlib",
                        f"/out:{library}", str(obj)], check=True, capture_output=True, text=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.draw_input.argtypes = [ctypes.c_void_p, ctypes.c_uint] + [ctypes.c_int] * 4 + [ctypes.c_uint] * 3
        cls.lib.box.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 4 + [ctypes.c_uint] * 2
        cls.lib.polygon.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]

    @classmethod
    def tearDownClass(cls):
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(cls.lib._handle))
        del cls.lib
        cls.temp.cleanup()

    def frame(self):
        buffer = ctypes.create_string_buffer(b"\xAD" * 32 + BASE + b"\xAC" * 32, SIZE + 64)
        return buffer, ctypes.addressof(buffer) + 32

    def render(self, buttons=0, mx=0, my=0, cx=0, cy=0, left=0, right=0, error=0):
        buffer, address = self.frame()
        self.lib.draw_input(address, buttons, mx, my, cx, cy, left, right, error)
        self.assertEqual(buffer.raw[:32], b"\xAD" * 32)
        self.assertEqual(buffer.raw[-32:], b"\xAC" * 32)
        return buffer.raw[32:-32]

    def test_default_geometry_changes_only_moonshine_panel(self):
        data = self.render()
        self.assertNotEqual(data, BASE)
        for y in range(480):
            if 314 <= y < 434:
                self.assertEqual(data[y * 1280:y * 1280 + 32], BASE[y * 1280:y * 1280 + 32])
                self.assertEqual(data[y * 1280 + 396:(y + 1) * 1280], BASE[y * 1280 + 396:(y + 1) * 1280])
            else:
                self.assertEqual(data[y * 1280:(y + 1) * 1280], BASE[y * 1280:(y + 1) * 1280])

    def test_buttons_fill_only_on_press_and_sticks_move(self):
        off = self.render()
        for button, x, y in [(0x100, 138, 66), (0x200, 113, 89), (0x400, 164, 50),
                             (0x800, 119, 41), (0x10, 144, 34), (0x1000, 91, 64)]:
            at = (314 + y) * 1280 + (16 + x) * 2
            self.assertGreater(self.render(buttons=button)[at], off[at])
        self.assertNotEqual(self.render(mx=100, my=100, cx=-100, cy=-100), off)

    def test_trigger_click_extends_past_analog_fill(self):
        analog = self.render(left=170, right=170)
        click = self.render(buttons=0x60, left=170, right=170)
        at = (314 + 14) * 1280 + (16 + 70) * 2
        self.assertGreater(click[at], analog[at])

    def test_dpad_directions_have_separate_indicators(self):
        off = self.render()
        for bit, x, y in ((1, 16, 95), (2, 32, 95), (4, 24, 103), (8, 24, 87)):
            at = (314 + y) * 1280 + (16 + x) * 2
            self.assertGreater(self.render(buttons=bit)[at], off[at])
            self.assertEqual(self.render(buttons=bit ^ 15)[at], off[at])

    def test_z_does_not_gate_the_tools_overlay(self):
        source = (ROOT / "lm_diag/src/lm_diag.cpp").read_text()
        call = source.split("LMPractice::draw(", 1)[1].split("drawStatusPopup", 1)[0]
        self.assertIn("LMTools::draw", call)
        self.assertNotIn("kButtonZ", call)

    def test_disconnected_pad_cannot_show_stale_buttons(self):
        self.assertEqual(self.render(buttons=0xFFFF, mx=100, left=255, error=255), self.render())

    def test_pair_chroma_and_untouched_neighbor_luma(self):
        buffer, address = self.frame()
        self.lib.box(address, 1, 0, 1, 1, 0xFF0000, 255)
        self.assertEqual(buffer.raw[32], 70)
        self.assertEqual(buffer.raw[34], 82)
        self.assertEqual(buffer.raw[33], 109)
        self.assertEqual(buffer.raw[35], 184)
        self.assertEqual(buffer.raw[36:-32], BASE[4:])

    def test_opaque_fast_path_matches_pair_reference_for_every_edge_parity(self):
        rng = random.Random(0xFF474C4D)
        def blend(old, value, alpha): return (old * (255 - alpha) + value * alpha + 127) // 255
        for _ in range(300):
            buffer, address = self.frame()
            initial = rng.randbytes(1280)
            ctypes.memmove(address, initial, 1280)
            expected = bytearray(initial)
            x, width = rng.randrange(-5, 645), rng.randrange(1, 645)
            rgb = rng.randrange(1 << 24)
            r, g, b = rgb >> 16, (rgb >> 8) & 255, rgb & 255
            y = 16 + ((66*r + 129*g + 25*b + 128) >> 8)
            cb = 128 + ((-38*r - 74*g + 112*b + 128) >> 8)
            cr = 128 + ((112*r - 94*g - 18*b + 128) >> 8)
            left, right = max(0, min(640, x)), max(0, min(640, x + width))
            if left < right:
                for pair in range(left & ~1, right, 2):
                    a, b = int(pair >= left), int(pair + 1 < right)
                    chroma_alpha = (255 * (a + b) + 1) // 2
                    if a: expected[pair * 2] = y
                    if b: expected[pair * 2 + 2] = y
                    expected[pair * 2 + 1] = blend(expected[pair * 2 + 1], cb, chroma_alpha)
                    expected[pair * 2 + 3] = blend(expected[pair * 2 + 3], cr, chroma_alpha)
            self.lib.box(address, x, 0, width, 1, rgb, 255)
            self.assertEqual(buffer.raw[32:32 + 1280], expected)
            self.assertEqual(buffer.raw[32 + 1280:-32], BASE[1280:])
            self.assertEqual(buffer.raw[:32], b"\xAD" * 32)
            self.assertEqual(buffer.raw[-32:], b"\xAC" * 32)

    def test_clipped_shapes_never_touch_guard_bytes(self):
        rng = random.Random(0x474C4D4A)
        buffer, address = self.frame()
        for _ in range(1000):
            self.lib.box(address, rng.randrange(-1000, 1000), rng.randrange(-1000, 1000),
                         rng.randrange(-10, 1000), rng.randrange(-10, 1000), rng.randrange(1 << 24), rng.randrange(256))
        for points in [(-30, -30, 90, 0, 0, 90), (500, 440, 670, 450, 630, 600),
                       (-2048, -2048, -2048, 2048, 2048, 2048, 2048, -2048)]:
            values = (ctypes.c_short * len(points))(*points)
            for stroke in (0, 1):
                self.lib.polygon(address, values, len(points) // 2, stroke)
        self.assertEqual(buffer.raw[:32], b"\xAD" * 32)
        self.assertEqual(buffer.raw[-32:], b"\xAC" * 32)

    def test_render_preview(self):
        from PIL import Image
        data = self.render(buttons=0x120, mx=60, my=70, cx=-50, cy=-35, left=100, right=170)
        output = bytearray(640 * 480 * 3)
        def clamp(x): return min(255, max(0, x))
        for pair in range(SIZE // 4):
            y0, cb, y1, cr = data[pair * 4:pair * 4 + 4]
            for side, y in enumerate((y0, y1)):
                c, d, e = y - 16, cb - 128, cr - 128
                at = (pair * 2 + side) * 3
                output[at:at + 3] = bytes((clamp((298*c + 409*e + 128) >> 8),
                                          clamp((298*c - 100*d - 208*e + 128) >> 8),
                                          clamp((298*c + 516*d + 128) >> 8)))
        preview = ROOT / "build-lm-diag/input-display-preview.png"
        preview.parent.mkdir(exist_ok=True)
        Image.frombytes("RGB", (640, 480), bytes(output)).save(preview)


if __name__ == "__main__":
    unittest.main()
