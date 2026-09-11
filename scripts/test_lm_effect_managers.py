"""Authenticate GLMJ01 owner boundaries and the reload crash's native path."""

import hashlib
import os
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOL = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
SHA1 = "722005ea9c1eab54b114f814734d8f327e5614ee"
STARTS = (0x803CC9A4, 0x803CCC58, 0x803CCF0C)
SIZE = 0x2A8
RECORDS = (0x803CC998, 0x803CCC4C, 0x803CCF00)


class SnapshotBoundaryTests(unittest.TestCase):
    def test_split_footprints_do_not_capture_destructor_records(self):
        self.assertEqual(sum(SIZE for _ in STARTS), 0x7F8)
        self.assertEqual(STARTS[-1] + SIZE, 0x803CD1B4)
        for start in STARTS:
            for record in RECORDS:
                self.assertTrue(start + SIZE <= record or record + 12 <= start)
        for i in range(2):
            self.assertEqual(STARTS[i] + SIZE, RECORDS[i + 1])

    def test_snapshot_integrates_three_exact_ranges(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        for i, start in enumerate(STARTS):
            name = f"kModelEffectManager{i}StateStart"
            self.assertRegex(source, rf"{name}\s*=\s*0x{start:08X}u;")
            self.assertIn("{" + name + ", kModelEffectManagerStateSize}", source)
        self.assertRegex(source, r"kModelEffectManagerStateSize\s*=\s*0x2A8u;")
        self.assertRegex(source, r"kSnapshotVersion\s*=\s*28(?:u)?;")

    def test_lazy_manager_includes_flag_and_excludes_registration(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertRegex(source, r"kLazyModelEffectStateStart\s*=\s*0x803CC46Cu;")
        self.assertRegex(source, r"kLazyModelEffectStateEnd\s*=\s*0x803CC718u;")
        self.assertIn("{kLazyModelEffectStateStart, kLazyModelEffectStateEnd - "
                      "kLazyModelEffectStateStart}", source)
        self.assertEqual(0x803CC718 - 0x803CC46C, 0x2AC)
        self.assertEqual(0x803CC46C + 0x2A8, 0x803CC714)
        self.assertEqual(0x803CC460 + 12, 0x803CC46C)


class AuthenticatedRetailTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not DOL.exists():
            raise unittest.SkipTest("Clean Japanese DOL unavailable; set LM_CLEAN_DOL")
        cls.raw = DOL.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != SHA1:
            raise AssertionError("Wrong clean DOL: retail evidence must not run on another build")
        cls.sections = []
        for count, offbase, addrbase, sizebase in ((7, 0, 0x48, 0x90),
                                                  (11, 0x1C, 0x64, 0xAC)):
            for i in range(count):
                off, addr, size = (struct.unpack_from(">I", cls.raw, base + 4 * i)[0]
                                   for base in (offbase, addrbase, sizebase))
                if size:
                    cls.sections.append((addr, addr + size, off))

    def word(self, address):
        for start, end, offset in self.sections:
            if start <= address and address + 4 <= end:
                return struct.unpack_from(">I", self.raw, offset + address - start)[0]
        self.fail(f"Unmapped retail address {address:08X}")

    def words(self, expected):
        for address, instruction in expected.items():
            with self.subTest(address=f"{address:08X}"):
                self.assertEqual(self.word(address), instruction)

    def call(self, address, target):
        word = self.word(address)
        self.assertEqual(word & 0xFC000003, 0x48000001)  # relative bl
        delta = word & 0x03FFFFFC
        if delta & 0x02000000:
            delta -= 0x04000000
        self.assertEqual((address + delta) & 0xFFFFFFFF, target)

    def test_constructor_pairs_each_object_with_separate_record(self):
        self.words({0x80150208: 0x3BA3C998, 0x80150210: 0x3B9D000C,
                    0x80150230: 0x38BD0000, 0x80150238: 0x3B9D02C0,
                    0x8015025C: 0x38BD02B4, 0x80150264: 0x3B9D0574,
                    0x80150288: 0x38BD0568})
        for address in (0x80150234, 0x80150260, 0x8015028C):
            self.call(address, 0x801F51C8)
        self.words({0x801F51CC: 0x90050000, 0x801F51D0: 0x90850004,
                    0x801F51D4: 0x90650008, 0x801F51D8: 0x90AD1840})

    def test_scene_initialization_owns_arrays_and_private_heap(self):
        self.call(0x80156ABC, 0x8014FC0C)
        for site, target in ((0x8014FC28, 0x8014E668),
                             (0x8014FC30, 0x8014F9EC),
                             (0x8014FC38, 0x8014FAFC)):
            self.call(site, target)
        self.words({0x8014E684: 0x38600A8C, 0x8014E690: 0x907D005C,
                    0x8014E694: 0x38600E10, 0x8014E69C: 0x907D0060,
                    0x8014E6A0: 0x387D005C, 0x8014E6A4: 0x3880001E,
                    0x8014E6A8: 0x38A0000F, 0x8014E6DC: 0x907D0000})
        self.call(0x8014E6AC, 0x80134954)
        self.call(0x8014E6D8, 0x801CADDC)
        self.assertEqual(15 * 0xB4, 0xA8C)
        self.assertEqual(30 * 0x78, 0xE10)

    def test_scene_cleanup_destroys_all_three_generations(self):
        self.call(0x80156C80, 0x8014FF50)
        self.words({0x8014FF68: 0x807F0068, 0x8014FF70: 0x807F006C,
                    0x8014FF78: 0x807F000C, 0x8014FF80: 0x807F031C,
                    0x8014FF88: 0x807F0320, 0x8014FF90: 0x807F02C0,
                    0x8014FF98: 0x807F05D0, 0x8014FFA0: 0x807F05D4,
                    0x8014FFA8: 0x807F0574})
        for site in (0x8014FF6C, 0x8014FF74, 0x8014FF84, 0x8014FF8C,
                     0x8014FF9C, 0x8014FFA4):
            self.call(site, 0x801C956C)
        for site in (0x8014FF7C, 0x8014FF94, 0x8014FFAC):
            self.call(site, 0x801CAE50)

    def test_model_update_precedes_grain_draw_and_writes_exact_field(self):
        self.call(0x8000BBCC, 0x80156B0C)
        self.call(0x80156BA0, 0x8014FC94)
        self.call(0x8014FCEC, 0x801C2418)
        self.call(0x8000BC7C, 0x8000BA64)
        self.words({0x8014FCE8: 0x807D0044,
                    0x801C24D4: 0x801F0074, 0x801C24D8: 0x901F0094,
                    0x801C24DC: 0x801F007C, 0x801C24E0: 0x901F0098})

    def test_grain_pool_counts_and_self_sentinel_are_native_invariants(self):
        self.words({0x8012EA5C: 0x3BE3BF48, 0x8012EA9C: 0x388005DC,
                    0x8012EAA0: 0x38A00050, 0x80125F60: 0x381B0064,
                    0x80125F74: 0x387B0220, 0x80125F80: 0x907B03D8,
                    0x80125FA0: 0x901B021C, 0x80125FB8: 0x38190040,
                    0x80125FBC: 0x90190094, 0x80125FE0: 0x3B3901B8,
                    0x80126028: 0x381B000C, 0x80126040: 0x901B0060,
                    0x80126064: 0x3BFF0054})
        self.call(0x8012EAA4, 0x80128BF4)

    def test_first_grain_manager_owns_15_controllers_and_1500_particles(self):
        self.call(0x8000C0B8, 0x8012A890)
        self.words({0x8012A8A8: 0x3BE3BAF0, 0x8012A8D0: 0x386019E0,
                    0x8012A8E0: 0x907F0000, 0x8012A8E4: 0x3C600002,
                    0x8012A8E8: 0x3863EC40, 0x8012A8F4: 0x907F0004,
                    0x8012A8FC: 0x388005DC, 0x8012A900: 0x38A0000F})
        self.call(0x8012A904, 0x80125F4C)
        self.call(0x8000BE50, 0x8012B280)
        self.assertEqual((15 * 0x1B8 + 31) & ~31, 0x19E0)
        self.assertEqual((1500 * 0x54 + 31) & ~31, 0x1EC40)
        self.words({0x80128C0C: 0x1CA001B8, 0x80128C18: 0x3805001F,
                    0x80128C24: 0x54030034, 0x80128C38: 0x1C600054,
                    0x80128C3C: 0x3803001F, 0x80128C40: 0x54030034})

    def test_both_grain_lists_use_native_reciprocal_links(self):
        self.words({0x80125F94: 0x90630140, 0x80125F9C: 0x90630144,
                    0x80125FCC: 0x90A5004C, 0x80125FD4: 0x90A50050,
                    0x80126000: 0x93A50140, 0x80126008: 0x90BD0144,
                    0x80126058: 0x93DF004C, 0x80126060: 0x93FE0050,
                    0x8012607C: 0x901F004C, 0x80126084: 0x93E30050,
                    0x80129F40: 0x83BD004C, 0x80129DA8: 0xA89D0024})

    def test_lazy_constructor_registers_separate_record_and_clears_flag(self):
        self.words({0x80134900: 0x3BE3C460, 0x80134908: 0x3BDF000C,
                    0x80134920: 0x38000000, 0x80134928: 0x981F02B4,
                    0x8013492C: 0x387E0000, 0x80134930: 0x38843B38,
                    0x80134934: 0x38BF0000})
        self.call(0x80134910, 0x8014E5C8)
        self.call(0x80134938, 0x801F51C8)

    def test_lazy_allocation_owns_pools_and_scene_model_heap(self):
        self.words({0x80134760: 0x3863C46C, 0x80134770: 0x880302A8,
                    0x80134780: 0x38000001, 0x80134784: 0x981F0000,
                    0x80133BB8: 0x38600A8C, 0x80133BC4: 0x907D005C,
                    0x80133BC8: 0x38601770, 0x80133BD0: 0x907D0060,
                    0x80133BD8: 0x38800032, 0x80133BDC: 0x38A0000F,
                    0x80133BE4: 0x83ED1514, 0x80133C10: 0x907D0000,
                    0x80133C2C: 0x386000A0, 0x80133C3C: 0x808D0C78,
                    0x80133C48: 0x80840030, 0x80133C4C: 0x8084005C,
                    0x80133C54: 0x939E0044})
        self.call(0x8013477C, 0x80133B9C)
        self.call(0x80133BE0, 0x80134954)
        self.call(0x80133C0C, 0x801CADDC)
        self.call(0x80133C50, 0x801C1DA8)
        self.assertEqual(50 * 0x78, 0x1770)

    def test_lazy_cleanup_and_first_draw_consume_same_owner(self):
        self.call(0x80156C84, 0x80134838)
        self.call(0x80134860, 0x8014E944)
        self.words({0x80134844: 0x3863C46C, 0x80134854: 0x880302A8,
                    0x80134864: 0x38000000, 0x80134868: 0x981F0000,
                    0x8014E958: 0x8063005C, 0x8014E960: 0x807F0060,
                    0x8014E968: 0x807F0000})
        self.call(0x8014E95C, 0x801C956C)
        self.call(0x8014E964, 0x801C956C)
        self.call(0x8014E96C, 0x801CAE50)
        self.call(0x80156BA4, 0x801347D0)
        self.words({0x801347DC: 0x3863C46C, 0x801347E4: 0x880302A8,
                    0x8014E7B0: 0x8063024C, 0x8014E810: 0x80640078,
                    0x8014E830: 0x807F0044})
        self.call(0x801347F0, 0x8014E798)
        self.call(0x8014E834, 0x801C2418)
        self.call(0x8000BB10, 0x8012B1D0)


if __name__ == "__main__":
    unittest.main()
