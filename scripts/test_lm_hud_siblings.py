"""Authenticated native evidence for scene HUD siblings omitted in format 19."""

import unittest

try:
    from scripts import test_lm_hud_state as retail
except ModuleNotFoundError:
    import test_lm_hud_state as retail


class HudSiblingEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_snapshot_captures_all_audited_siblings(self):
        source = (retail.ROOT / "lm_diag/src/lm_state.cpp").read_text()
        for name, start, end in (("kTimerHudOwnerState", 0x803C4448, 0x803C4628),
                                 ("kElementHudOwnerState", 0x803C4718, 0x803C4868),
                                 ("kBooRadarOwnerState", 0x803C49C0, 0x803C49D8)):
            self.assertIn(f"{name}Start = 0x{start:08X}u", source)
            self.assertIn(f"{name}End = 0x{end:08X}u", source)
            self.assertRegex(source, rf"\{{{name}Start,\s*{name}End - {name}Start\}}")
        self.assertIn("kSnapshotVersion = 28u", source)

    def test_element_base_is_exact_crash_b_consumer(self):
        self.words({0x80045AA4: 0x3BC64718,
                    0x80045DC4: 0x387E0138,
                    0x8003BBA0: 0x807D0010,
                    0x8003BBAC: 0x80C300EC,
                    0x8003BBB0: 0xA0E6003E,
                    0x8003BBB4: 0xA0C6003C})
        self.call(0x80045DCC, 0x8003BB5C)
        self.assertEqual(0x803C4718 + 0x138, 0x803C4850)
        resource = b"/kawano/emeter/meter_base.bti\0"
        self.assertEqual(self.data(0x802FB430, len(resource)), resource)
        self.words({0x80045840: 0x387E0138,
                    0x80045844: 0x389D01F8,
                    0x800457CC: 0x3BA5B238})
        self.assertEqual(0x802FB238 + 0x1F8, 0x802FB430)

    def test_element_table_creation_and_cleanup_own_fourteen_wrappers(self):
        self.words({0x800457C4: 0x3BC44718,
                    0x800457F4: 0x2C1A000A,
                    0x800457FC: 0x3B7B0018,
                    0x8004581C: 0x3B9C00F0,
                    0x80045830: 0x2C1A0003,
                    0x80045838: 0x3B9C0018,
                    0x80045E88: 0x3BE34718,
                    0x80045EAC: 0x2C1D000A,
                    0x80045EC4: 0x3BDE00F0,
                    0x80045ED4: 0x2C1D0003,
                    0x80045EE0: 0x387F0138})
        for site in (0x800457EC, 0x80045828, 0x80045848):
            self.call(site, 0x8003B99C)
        for site in (0x80045EA4, 0x80045ECC, 0x80045EE4):
            self.call(site, 0x8003C21C)
        self.words({0x80045F14: 0x38C00018,
                    0x80045F1C: 0x38E0000A,
                    0x80045F28: 0x3BC34718,
                    0x80045F44: 0x387E00F0,
                    0x80045F4C: 0x38C00018,
                    0x80045F50: 0x38E00003,
                    0x80045F58: 0x387E0138})
        self.call(0x80045F5C, 0x8003B950)

    def test_element_controller_and_scalars_are_scene_owned_sbss_roots(self):
        self.words({0x8003DAB4: 0x38600020,
                    0x8003DACC: 0x92CD0620,
                    0x8003E5F4: 0x806D0620,
                    0x8003E1DC: 0x806D0620,
                    0x80045854: 0xB06D0624,
                    0x8004585C: 0xB00D0626,
                    0x80045868: 0xB0CD0628,
                    0x80045A6C: 0x900D062C})
        for site, target in ((0x8003DAB8, 0x801C9308),
                             (0x8003DAC8, 0x80045724),
                             (0x80045738, 0x800457A8),
                             (0x8003E5FC, 0x80045754),
                             (0x80045778, 0x80045E74),
                             (0x8003E1E0, 0x80045A84)):
            self.call(site, target)

    def test_timer_table_has_three_plus_sixteen_scene_owned_wrappers(self):
        self.call(0x8003DA74, 0x80043E40)
        self.call(0x80043E54, 0x80043EC4)
        self.call(0x8003E5A8, 0x80043E70)
        self.call(0x80043E94, 0x800447CC)
        self.words({0x80043EE4: 0x3BA44460,
                    0x80043F2C: 0x3BDE01D8,
                    0x80043F30: 0x3BBD0048,
                    0x80043F44: 0x2C1C0010,
                    0x80043F4C: 0x3BBD0018,
                    0x800447E0: 0x3BE34460,
                    0x8004480C: 0x3BFF0048,
                    0x8004481C: 0x2C1E0010,
                    0x80044820: 0x3BFF0018,
                    0x80044854: 0x3BE34460,
                    0x80044878: 0x387F0048,
                    0x80044880: 0x38C00018,
                    0x80044884: 0x38E00010})
        for site in (0x80043EFC, 0x80043F08, 0x80043F14, 0x80043F3C):
            self.call(site, 0x8003B99C)
        for site in (0x800447EC, 0x800447F4, 0x800447FC, 0x80044814):
            self.call(site, 0x8003C21C)
        for site in (0x8004485C, 0x80044864, 0x8004486C):
            self.call(site, 0x8003B950)
        text = b"/kawano/newtime/ncolon.bti\0"
        self.assertEqual(self.data(0x802FA9D0, len(text)), text)

    def test_boo_radar_is_one_scene_hud_picture(self):
        self.call(0x8003DB00, 0x8004FFFC)
        self.call(0x80050010, 0x80050080)
        self.call(0x800500B0, 0x8003B99C)
        self.call(0x8003E614, 0x8005002C)
        self.call(0x80050050, 0x800504E8)
        self.call(0x800504FC, 0x8003C21C)
        self.call(0x80050524, 0x8003B950)
        self.words({0x80050090: 0x3884ADB8,
                    0x800500A8: 0x386549C0,
                    0x800504F4: 0x386349C0,
                    0x8005051C: 0x386349C0,
                    0x8003DB04: 0x92CD06B0})
        text = b"/kawano/teresar.bti\0"
        self.assertEqual(self.data(0x8032ADB8, len(text)), text)

    def test_goodnight_picture_uses_mission_scene_lifecycle(self):
        self.call(0x8000BFF4, 0x80043C64)
        self.call(0x80043C9C, 0x8003B99C)
        self.call(0x8000BE64, 0x80043DD4)
        self.call(0x80043DF8, 0x8003C21C)
        self.call(0x80043E2C, 0x8003B950)
        self.words({0x80043C90: 0x3BE44448,
                    0x80043C94: 0x3883A798,
                    0x80043DF4: 0x38634448,
                    0x80043E24: 0x38634448})
        text = b"/kawano/goodnight/Gnight.bti\0"
        self.assertEqual(self.data(0x802FA798, len(text)), text)

    def test_boundaries_have_no_unproven_gap_or_destructor_record(self):
        self.assertEqual(0x803C4448 + (1 + 3 + 16) * 0x18, 0x803C4628)
        self.assertEqual(0x803C4718 + (10 + 3 + 1) * 0x18, 0x803C4868)
        self.assertEqual(0x803C49C0 + 0x18, 0x803C49D8)
        self.assertEqual(0x1E0 + 0x150 + 0x18, 840)
        self.words({0x800460BC: 0xD4074868,
                    0x800460EC: 0x7FE3FB78,
                    0x80046118: 0x906D0644})
        self.call(0x80046110, 0x801CADDC)
        self.call(0x8004611C, 0x801C8E94)


if __name__ == "__main__":
    unittest.main()
