"""Authenticated ownership proof for the ten room-prop pictures and view origin."""

import re
import struct
import unittest

try:
    from scripts import test_lm_hud_state as retail
except ModuleNotFoundError:
    import test_lm_hud_state as retail


START, PICTURE_END, END = 0x803C1C60, 0x803C1C88, 0x803C1C98
VTABLE, PICTURE_SIZE = 0x80386E60, 0x15C


class RoomPropNativeEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_initializer_clears_all_ten_picture_roots_before_map_gate(self):
        self.words({0x80027DA0: 0x38600000, 0x80027DAC: 0x38A00008,
                    0x80027DB8: 0x3BE41C60, 0x80027DC8: 0xB06D0374,
                    0x80027DF0: 0x2005000A, 0x80027DF4: 0x2C05000A,
                    0x80027E00: 0x90640000, 0x80027E04: 0x38840004,
                    0x80027E0C: 0x800D0168, 0x80027E10: 0x2C000002,
                    0x80027E14: 0x4082050C, 0x80028314: 0x54A0103A,
                    0x80028318: 0x7C9F0214})
        for index in range(8):
            self.assertEqual(self.word(0x80027DCC + index * 4),
                             0x907F0000 + index * 4)
        self.assertEqual(START + 10 * 4, PICTURE_END)

    def test_actor_array_has_eleven_entries_and_already_captured_sbss_roots(self):
        self.call(0x80010A68, 0x80027D9C)
        self.words({0x80027E18: 0x3800000B, 0x80027E1C: 0x1C600388,
                    0x80027E20: 0xB00D0374, 0x80027E24: 0x38630008,
                    0x80027E38: 0x38C00388, 0x80027E3C: 0x38E0000B,
                    0x80027E44: 0x906D0370})
        self.call(0x80027E28, 0x801C9408)
        self.call(0x80027E40, 0x801F5504)
        self.assertEqual(0x804A0AE0 + 0x370, 0x804A0E50)

    def test_four_native_pictures_have_exact_size_constructor_and_table_stores(self):
        for size_site, allocation_site, constructor_site, store_site, offset in (
                (0x80028208, 0x80028234, 0x80028260, 0x80028264, 0),
                (0x80028268, 0x8002826C, 0x80028298, 0x8002829C, 4),
                (0x800282A0, 0x800282A4, 0x800282D0, 0x800282D4, 8),
                (0x800282D8, 0x800282DC, 0x80028308, 0x8002830C, 12)):
            self.assertEqual(self.word(size_site), 0x3860015C)
            self.call(allocation_site, 0x801C9308)
            self.call(constructor_site, 0x801AE840)
            self.assertEqual(self.word(store_site), 0x935F0000 | offset)
        self.words({0x801AE868: 0x3C608038, 0x801AE870: 0x38036E60,
                    0x801AE874: 0x901E0000})

    def test_picture_resources_are_parent_archive_room_textures(self):
        for address, text in ((0x802F5178, b"/iwamoto/bath.bti\0"),
                              (0x802F518C, b"/iwamoto/bed.bti\0"),
                              (0x802F51A0, b"/iwamoto/zizi_k.bti\0"),
                              (0x802F51B4, b"/iwamoto/tea_k.bti\0")):
            self.assertEqual(self.data(address, len(text)), text)
        for site in (0x80028240, 0x80028278, 0x800282B0, 0x800282E8):
            self.call(site, 0x80066330)

    def test_draw_uses_same_fixed_roots_and_picture_texture_fields(self):
        self.call(0x800111A4, 0x8002837C)
        self.call(0x800115B8, 0x80028594)
        self.words({0x80027978: 0x3C80803C, 0x8002798C: 0x38041C60,
                    0x80027994: 0x8063007C, 0x800279A0: 0x5463103A,
                    0x800279A8: 0x81430000, 0x800279B4: 0x880A00FC,
                    0x800279E4: 0x806A00EC})

    def test_cleanup_count_and_virtual_dispatch_match_exact_new_crash(self):
        self.call(0x8000BE78, 0x80011650)
        self.call(0x80011664, 0x80028610)
        self.words({0x8002862C: 0xA80D0374, 0x80028648: 0x800D0370,
                    0x80028650: 0x807D0128, 0x80028660: 0x93FD0128,
                    0x80028664: 0x3BDE0388, 0x80028678: 0x806D0370,
                    0x80028684: 0x3BC31C60, 0x8002868C: 0x807E0000,
                    0x8002869C: 0x81830000, 0x800286A4: 0x818C0008,
                    0x800286B4: 0x2C1C000A, 0x800286B8: 0x3BDE0004})
        self.call(0x8002865C, 0x8000604C)
        self.call(0x8002867C, 0x801C9508)
        self.assertEqual(0xAFEFEFFF + 8, 0xAFEFF007)

    def test_adjacent_view_origin_is_three_float_state_not_an_os_owner(self):
        self.words({0x80028924: 0x3BE41C88, 0x800289F4: 0xC004008C,
                    0x800289F8: 0xD01F0000, 0x80028A04: 0xC0030004,
                    0x80028A0C: 0xD01F0004, 0x80028A10: 0xC0040094,
                    0x80028A14: 0xD01F0008, 0x80028A1C: 0xC004008C,
                    0x80028A20: 0xD01F0000, 0x80028A24: 0xC0040090,
                    0x80028A28: 0xD01F0004, 0x80028A2C: 0xC0040094,
                    0x80028A30: 0xD01F0008, 0x80028F90: 0x38A41C88,
                    0x80028F94: 0x80850000, 0x80028F98: 0x80050004,
                    0x80028F9C: 0x90830044, 0x80028FA0: 0x90030048,
                    0x80028FA4: 0x80050008, 0x80028FA8: 0x9003004C})
        self.assertEqual(START + 10 * 4 + 16, END)
        # The next symbol has independent byte-record consumers, not more pictures.
        self.words({0x80029A4C: 0x3BE31C98, 0x80029A6C: 0x881F0000,
                    0x80029A74: 0x8B7F0001, 0x80029ADC: 0x3BFF0004})


class RoomPropCaptureProof(unittest.TestCase):
    def test_manifest_captures_full_family_once_and_keeps_format_boundary(self):
        source = (retail.ROOT / "lm_diag/src/lm_state.cpp").read_text()
        source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
        self.assertIn("kSnapshotVersion = 28u", source)
        name = "kRoomPropPictureState"
        for suffix, address in (("Start", START), ("End", END)):
            self.assertIn(f"{name}{suffix} = 0x{address:08X}u", source)
        manifest = re.search(r"kStateStaticRanges\[\]\s*=\s*\{(.*?)\n\};",
                             source, re.S).group(1)
        self.assertEqual(len(re.findall(
            rf"\{{{name}Start,\s*{name}End - {name}Start\}}", manifest)), 1)
        self.assertIn(f"({name}End - {name}Start)", source.split(
            "constexpr u32 kStateStaticsSize =", 1)[1].split(";", 1)[0])
        for function in ("captureStaticRanges", "restoreStaticRanges", "storeStaticRanges"):
            body = source.split(f"void {function}() {{", 1)[1].split("\n}\n", 1)[0]
            self.assertIn("i < kStateStaticRangeCount", body)
            self.assertIn("range = kStateStaticRanges[i]", body)
            self.assertIn("range.address", body)
            self.assertIn("range.size", body)
        for global_name in ("kGameSbss1Start", "kGameSbss1End"):
            match = re.search(rf"{global_name} = (0x[0-9A-Fa-f]+)u", source)
            setattr(self, global_name, int(match.group(1), 16))
        self.assertLessEqual(self.kGameSbss1Start, 0x804A0E50)
        self.assertGreaterEqual(self.kGameSbss1End, 0x804A0E56)


class RoomPropFixtureProof(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        paths = [retail.ROOT / "build-lm-emu" / name / "mem1.bin" for name in
                 ("diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]
        if not all(path.exists() for path in paths):
            raise unittest.SkipTest("Optional private MEM1 captures unavailable")
        cls.images = [path.read_bytes() for path in paths]

    def word(self, image, address):
        self.assertEqual(address & 3, 0)
        self.assertGreaterEqual(address, 0x80000000)
        self.assertLessEqual(address + 4, 0x80000000 + len(image))
        return struct.unpack_from(">I", image, address - 0x80000000)[0]

    def used_allocations(self, image):
        word = lambda address: self.word(image, address)
        heap = word(0x804A0B98)
        start, end = word(heap + 0x30), word(heap + 0x34)
        node, tail = word(heap + 0x7C), word(heap + 0x80)
        previous, entries = 0, {}
        while node:
            self.assertLess(len(entries), 8192)
            self.assertGreaterEqual(node, start)
            self.assertLessEqual(node + 16, end)
            tag, size, prev, next_node = (word(node + i * 4) for i in range(4))
            self.assertEqual(tag >> 16, 0x484D)
            self.assertLessEqual(node + 16 + size, end)
            self.assertEqual(prev, previous)
            self.assertNotIn(node + 16, entries)
            entries[node + 16] = (size, tag & 0xFF)
            previous, node = node, next_node
        self.assertEqual(previous, tail)
        return entries

    def test_each_complete_endpoint_owns_four_native_game_pictures(self):
        for image in self.images:
            allocations = self.used_allocations(image)
            pointers = [self.word(image, START + i * 4) for i in range(10)]
            self.assertEqual(pointers[4:], [0] * 6)
            self.assertEqual(len(set(pointers[:4])), 4)
            for pointer in pointers[:4]:
                self.assertEqual(allocations.get(pointer), (PICTURE_SIZE, 1))
                self.assertEqual(self.word(image, pointer), VTABLE)

    def test_mixed_generation_roots_fail_even_when_both_original_heaps_are_valid(self):
        pointers = [[self.word(image, START + i * 4) for i in range(4)]
                    for image in self.images]
        for first, second in zip(*pointers):
            self.assertNotEqual(first, second)
        for heap_index, owner_index in ((0, 1), (1, 0)):
            for pointer in pointers[owner_index]:
                self.assertNotEqual(self.word(self.images[heap_index], pointer), VTABLE)
        offset = PICTURE_END - 0x80000000
        self.assertNotEqual(self.images[0][offset:offset + 12],
                            self.images[1][offset:offset + 12])


if __name__ == "__main__":
    unittest.main()
