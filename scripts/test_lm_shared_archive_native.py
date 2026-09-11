"""Authenticate JP shared archive lifetime and resource-only capture boundaries."""

import struct
import unittest

try:
    from scripts import test_lm_hud_state as retail
except ModuleNotFoundError:
    import test_lm_hud_state as retail


class SharedArchiveNativeEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def text_words(self):
        for index in range(7):
            offset, address, size = (struct.unpack_from(">I", self.raw, base + index * 4)[0]
                                     for base in (0, 0x48, 0x90))
            for delta in range(0, size, 4):
                yield address + delta, struct.unpack_from(">I", self.raw, offset + delta)[0]

    def test_parent_publication_is_boot_only_and_mission_cleanup_is_inert(self):
        self.words({0x80066324: 0x4E800020,
                    0x80066328: 0x906D07D0, 0x8006632C: 0x4E800020,
                    0x80066330: 0x806D07D0, 0x80066334: 0x4E800020})
        self.call(0x8000BE68, 0x80066324)
        self.call(0x8000E3A8, 0x80066328)
        stores, callers = [], []
        for address, instruction in self.text_words():
            if instruction & 0xFC1FFFFF == 0x900D07D0:
                stores.append(address)
            if instruction & 0xFC000003 == 0x48000001:
                delta = instruction & 0x03FFFFFC
                if delta & 0x02000000:
                    delta -= 0x04000000
                if (address + delta) & 0xFFFFFFFF == 0x80066328:
                    callers.append(address)
        self.assertEqual(stores, [0x80066328])
        self.assertEqual(callers, [0x8000E3A8])
        self.assertEqual(0x804A0AE0 + 0x7D0, 0x804A12B0)

    def test_boot_loads_japanese_parent_and_switches_back_to_sys(self):
        resource = b"/Game/game.szp\0"
        self.assertEqual(self.data(0x802F4274, len(resource)), resource)
        self.words({0x8000E3DC: 0x38600010,
                    0x8000E3EC: 0x3C808001, 0x8000E3F4: 0x3884E338,
                    0x8000E408: 0x806D00B4,
                    0x80006124: 0x806D00B8})
        self.call(0x8000E3E4, 0x800060CC)
        self.call(0x8000E3E8, 0x80006118)
        self.call(0x8000E404, 0x800065BC)
        self.call(0x8000E40C, 0x801C8E94)

    def test_callback_separates_resource_payload_transport_and_owner(self):
        self.words({0x8000E350: 0x80640064,
                    0x8000E358: 0x38800020, 0x8000E35C: 0x38A00000,
                    0x8000E370: 0x389F0000, 0x8000E378: 0x807E0064,
                    0x8000E380: 0x38600068, 0x8000E394: 0x389F0000,
                    0x8000E398: 0x38A00000, 0x8000E39C: 0x38C00000,
                    0x80007098: 0x80630004, 0x8000709C: 0x3803001F,
                    0x800070A0: 0x54030034,
                    0x801C8EE4: 0x800D1514})
        for address, target in ((0x8000E354, 0x80007098),
                                (0x8000E360, 0x801C8EA4),
                                (0x8000E374, 0x800070A8),
                                (0x8000E37C, 0x8000604C),
                                (0x8000E384, 0x801C9308),
                                (0x8000E3A0, 0x801CEA18)):
            self.call(address, target)

    def test_nested_wrappers_borrow_parent_resource_not_transport_memory(self):
        self.call(0x800617A4, 0x80066330)
        self.words({0x800617B0: 0x818C0014, 0x800617B8: 0x4E800021,
                    0x800617BC: 0x7C7B1B79, 0x800617CC: 0x38600068,
                    0x800617E0: 0x389B0000, 0x800617E4: 0x38A00000,
                    0x800617E8: 0x38C00001, 0x800617F0: 0x93BE0004})
        self.call(0x800617D0, 0x801C9308)
        self.call(0x800617EC, 0x801CEA18)
        self.call(0x800617FC, 0x80061010)

    def test_mem_archive_pointer_tables_and_fetch_cache_are_resource_relative(self):
        self.words({0x801CECA0: 0x9083005C,
                    0x801CECA8: 0x80030008, 0x801CECB0: 0x901F0044,
                    0x801CECB8: 0x80030004, 0x801CECC0: 0x901F0048,
                    0x801CECC8: 0x8003000C, 0x801CECD0: 0x901F004C,
                    0x801CECD8: 0x80030014, 0x801CECE0: 0x901F0050,
                    0x801CECE8: 0x8065000C, 0x801CECEC: 0x80050008,
                    0x801CECF8: 0x901F0060,
                    0x801CED18: 0x907F0038,
                    0x801CED34: 0x80040010, 0x801CED40: 0x80630060,
                    0x801CED44: 0x80040008, 0x801CED48: 0x7C030214,
                    0x801CED4C: 0x90040010})
        self.call(0x801CED14, 0x801C8FC4)
        self.assertFalse(any(self.word(address) & 0xFC000001 == 0x48000001
                             for address in range(0x801CED34, 0x801CED68, 4)))

    def test_parent_backing_free_flag_is_zero_but_nested_flag_is_one(self):
        self.words({0x8000E39C: 0x38C00000, 0x800617E8: 0x38C00001,
                    0x801CEC8C: 0x2C060001,
                    0x801CECFC: 0x4082000C, 0x801CED00: 0x38000001,
                    0x801CED08: 0x38000000, 0x801CED0C: 0x981F0064,
                    0x801CEB08: 0x881E0064, 0x801CEB10: 0x41820018,
                    0x801CEB14: 0x807E005C, 0x801CEB20: 0x809E0038})
        self.call(0x801CEB24, 0x801C8F1C)
        self.call(0x801CEB34, 0x801CF8F0)

    def test_audio_arena_and_aram_source_have_different_owners(self):
        self.words({0x8000E688: 0x80AD00B4,
                    0x8000E68C: 0x3C60000C, 0x8000E690: 0x38800020,
                    0x8000E698: 0x3C80000C,
                    0x8000E6A8: 0x806D01C4,
                    0x8000E418: 0x806D01C4})
        self.call(0x8000E694, 0x801C8EA4)
        self.call(0x8000E6A0, 0x80186724)
        self.call(0x8000E6C8, 0x8000E3C4)
        self.assertNotEqual(0x804A0AE0 + 0x1C4, 0x804A12B0)


if __name__ == "__main__":
    unittest.main()
