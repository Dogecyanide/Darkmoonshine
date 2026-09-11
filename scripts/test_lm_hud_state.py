"""Authenticate GLMJ01 scene-owned HUD roots and excluded font lifetime."""

import hashlib
import os
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOL = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
SHA1 = "722005ea9c1eab54b114f814734d8f327e5614ee"
RANGES = (("kGbhHudOwnerState", 0x803C3238, 0x803C3388),
          ("kHudPictureOwnerState", 0x803C3400, 0x803C3730))


class SnapshotBoundaryTests(unittest.TestCase):
    def test_complete_picture_tables_without_font_or_destructor_record(self):
        self.assertEqual(9 * 0x18, 0xD8)
        self.assertEqual(0x803C3238 + 0xD8, 0x803C3310)
        self.assertEqual(0xD8 + 0x78, RANGES[0][2] - RANGES[0][1])
        self.assertEqual((10 + 10 + 1 + 13) * 0x18, RANGES[1][2] - RANGES[1][1])
        self.assertEqual(sum(end - start for _, start, end in RANGES), 0x480)
        self.assertEqual(0x803C3388 + 12, 0x803C3394)
        self.assertEqual(0x803C3394 + 0x6C, 0x803C3400)
        for _, start, end in RANGES:
            self.assertTrue(end <= 0x803C3388 or start >= 0x803C3400)

    def test_snapshot_integrates_exact_scene_owned_ranges(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        for name, start, end in RANGES:
            self.assertRegex(source, rf"{name}Start\s*=\s*0x{start:08X}u;")
            self.assertRegex(source, rf"{name}End\s*=\s*0x{end:08X}u;")
            self.assertRegex(source, rf"\{{{name}Start,\s*{name}End - {name}Start\}}")
        self.assertRegex(source, r"kSnapshotVersion\s*=\s*28(?:u)?;")


class AuthenticatedRetailTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not DOL.exists():
            raise unittest.SkipTest("Clean Japanese DOL unavailable; set LM_CLEAN_DOL")
        cls.raw = DOL.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != SHA1:
            raise AssertionError("Wrong clean DOL: HUD evidence requires the authenticated JP retail DOL")
        cls.sections = []
        for count, offbase, addrbase, sizebase in ((7, 0, 0x48, 0x90),
                                                  (11, 0x1C, 0x64, 0xAC)):
            for i in range(count):
                off, addr, size = (struct.unpack_from(">I", cls.raw, base + 4 * i)[0]
                                   for base in (offbase, addrbase, sizebase))
                if size:
                    cls.sections.append((addr, addr + size, off))

    def data(self, address, size):
        for start, end, offset in self.sections:
            if start <= address and address + size <= end:
                begin = offset + address - start
                return self.raw[begin:begin + size]
        self.fail(f"Unmapped retail range {address:08X}+{size:X}")

    def word(self, address):
        return struct.unpack(">I", self.data(address, 4))[0]

    def words(self, expected):
        for address, instruction in expected.items():
            with self.subTest(address=f"{address:08X}"):
                self.assertEqual(self.word(address), instruction)

    def call(self, address, target):
        word = self.word(address)
        self.assertEqual(word & 0xFC000003, 0x48000001)
        delta = word & 0x03FFFFFC
        if delta & 0x02000000:
            delta -= 0x04000000
        self.assertEqual((address + delta) & 0xFFFFFFFF, target)

    def test_crash_is_gbh_shadow_picture_not_particle_or_sys_archive(self):
        self.words({0x8003D210: 0x3BE33238, 0x8003D220: 0x387F0060,
                    0x8003BB2C: 0x807F0010, 0x8003BB34: 0x80C300EC,
                    0x8003BB3C: 0xA0E6003E, 0x8003BB40: 0xA0C6003C,
                    0x8003CABC: 0x387F0060, 0x8003CAA0: 0x3BC498F0,
                    0x8003CAC4: 0x389E0148})
        self.call(0x8003D224, 0x8003BAF8)
        self.call(0x8003CACC, 0x8003B99C)
        resource = b"/kawano/base/cgbk_kage.tim"
        self.assertEqual(self.data(0x802F9A38, len(resource)), resource)
        self.assertEqual(0x803C3238 + 4 * 0x18, 0x803C3298)
        self.assertEqual(0x101 + 0x3E, 0x13F)

    def test_picture_wrapper_is_plain_cpu_state_with_current_heap_objects(self):
        self.words({0x8003B954: 0x90830000, 0x8003B95C: 0x90830004,
                    0x8003B960: 0x90830008, 0x8003B970: 0x9803000C,
                    0x8003B9B8: 0x3860017C, 0x8003B9FC: 0x93BE0010,
                    0x8003B9F0: 0x3C608030, 0x8003B9F4: 0x380397DC,
                    0x801C931C: 0x800D1514, 0x801C9334: 0x818C000C,
                    0x8003C230: 0x80630010, 0x8003C254: 0x807F0014})
        self.call(0x8003B9C0, 0x801C9308)
        self.call(0x8003B9EC, 0x8003AAF4)

    def test_gbh_static_constructor_and_scene_cleanup_cover_all_nine(self):
        self.words({0x8003E690: 0x3BE33238, 0x8003E4E4: 0x3BC33238})
        for i in range(9):
            self.call(0x8003E698 + i * 8, 0x8003B950)
            self.assertEqual(self.word(0x8003E694 + i * 8), 0x387F0000 + i * 0x18)
        for site in (0x8003E588, 0x8003E590, 0x8003E598, 0x8003E5B0,
                     0x8003E5B8, 0x8003E5C0, 0x8003E5C8, 0x8003E5D0,
                     0x8003E5D8):
            self.call(site, 0x8003C21C)
        self.call(0x80037B4C, 0x8003D418)
        self.call(0x8000BE30, 0x8003E4C8)

    def test_gbh_adjacent_roots_are_screens_panes_and_fade_controllers(self):
        self.words({0x8003D43C: 0x386000F4, 0x8003D4A0: 0x92DE00D8,
                    0x8003D5A0: 0x92DE00DC, 0x8003D7C4: 0x92DE00E0,
                    0x8003D6A0: 0x92DE00E4, 0x8003D878: 0x3B1E00E8,
                    0x8003D87C: 0x3B5E00F8, 0x8003D880: 0x3B9C0120,
                    0x8003D8A0: 0x90780000, 0x8003D8BC: 0x907A0000,
                    0x8003D8D8: 0x2C160004, 0x8003D8E8: 0x3B9C000C,
                    0x8003DA14: 0x387E0114, 0x8003B0F4: 0x90830000,
                    0x8003B118: 0xB0030004, 0x8003B11C: 0xB0830006,
                    0x8003B120: 0x98A30008,
                    0x8003E52C: 0x807E00D8, 0x8003E554: 0x3BBE00E4})
        self.call(0x8003D444, 0x801C9308)
        self.call(0x8003D8D0, 0x8003B0E8)
        self.call(0x8003DA24, 0x8003B0E8)
        self.words({0x8003E6E4: 0x387F0120, 0x8003E6EC: 0x38C0000C,
                    0x8003E6F0: 0x38E00004})

    def test_digit_tables_have_twenty_one_scene_owned_picture_wrappers(self):
        self.call(0x8003DA5C, 0x8003F200)
        self.call(0x8003E59C, 0x8003F38C)
        self.words({0x8003F220: 0x3BE33400, 0x8003F238: 0x3B5C00F0,
                    0x8003F28C: 0x2C19000A, 0x8003F294: 0x3B9C0018,
                    0x8003F298: 0x3B5A0018, 0x8003F2A0: 0x387F01E0,
                    0x8003F3A0: 0x3BE33400, 0x8003F3D4: 0x2C1C000A,
                    0x8003F3E4: 0x387F01E0})
        for site in (0x8003F248, 0x8003F26C, 0x8003F2A8):
            self.call(site, 0x8003B99C)
        for site in (0x8003F3C4, 0x8003F3CC, 0x8003F3E8):
            self.call(site, 0x8003C21C)
        self.words({0x8004014C: 0x3BC33400, 0x80040170: 0x38C00018,
                    0x80040174: 0x38E0000A, 0x8004017C: 0x387E01E0})

    def test_health_table_has_thirteen_wrappers_and_scene_owned_allocations(self):
        self.call(0x8003DA90, 0x80041854)
        self.call(0x80041868, 0x80040870)
        self.call(0x8003E5E4, 0x8004188C)
        self.words({0x80040898: 0x3BA435F8, 0x800418B0: 0x3BE335F8,
                    0x80041914: 0x3BE335F8, 0x80041958: 0x387F00A8,
                    0x80041960: 0x38C00018, 0x80041964: 0x38E00006})
        for i in range(7):
            self.call(0x8004191C + i * 8, 0x8003B950)
        for site in (0x800408A4, 0x800408B0, 0x800408BC):
            self.call(site, 0x8003B99C)
        for site in (0x800418BC, 0x800418C4, 0x800418CC):
            self.call(site, 0x8003C21C)

    def test_excluded_gap_is_persistent_font_and_runtime_destructor_record(self):
        self.words({0x8003F1D0: 0x38633394, 0x8003F1E4: 0x38840E18,
                    0x8003F1E8: 0x38A53388,
                    0x801F51CC: 0x90050000, 0x801F51D0: 0x90850004,
                    0x801F51D4: 0x90650008, 0x801F51D8: 0x90AD1840,
                    0x8003E728: 0x3BE43394, 0x8003E73C: 0x3860005C,
                    0x8003E75C: 0x93CD0508})
        self.call(0x8003F1D8, 0x801D0D3C)
        self.call(0x8003F1EC, 0x801F51C8)
        self.call(0x8000E728, 0x8003E70C)
        self.call(0x8003E738, 0x801D0EB4)


if __name__ == "__main__":
    unittest.main()
