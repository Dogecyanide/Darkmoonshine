"""Authenticate the native room-byte and scalar map caches beside ToolData."""

import unittest

try:
    from scripts import test_lm_hud_state as retail
    from scripts import test_lm_scene_owner_audit as capture
except ModuleNotFoundError:
    import test_lm_hud_state as retail
    import test_lm_scene_owner_audit as capture

MAP, MAP_END = 0x803C1C98, 0x803C20C8
ROOM_BYTES, ROOM_BYTES_END = 0x803C2468, 0x803C24E8


class RoomMapNativeEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_room_bytes_are_rebuilt_from_native_room_info_not_os_state(self):
        self.words({0x8002E658: 0x3C60803C, 0x8002E668: 0x3BE32138,
                    0x8002E7D4: 0x800D0168, 0x8002E7D8: 0x2C000002,
                    0x8002E7E0: 0x3BBF0330, 0x8002E7F0: 0x800D0210,
                    0x8002E7FC: 0x987D0000, 0x8002E800: 0x3B9C014C,
                    0x8002E804: 0x3BBD0001, 0x8002E80C: 0x880D0214,
                    0x80018ADC: 0x38652138})
        self.call(0x8002E7F8, 0x80018AC8)
        self.call(0x80018AE0, 0x8002D484)
        self.assertEqual(0x803C2138 + 0x330, ROOM_BYTES)
        self.assertEqual(ROOM_BYTES_END - ROOM_BYTES, 128)

    def test_map_classification_consumes_one_byte_per_four_byte_room_record(self):
        self.words({0x80029E80: 0x3C80803C, 0x80029E84: 0x3CA0803C,
                    0x80029E90: 0x39241C98, 0x80029E9C: 0x3C80803C,
                    0x80029EA0: 0x39052468, 0x80029EA4: 0x38A42E10,
                    0x80029EB4: 0x89680000, 0x80029EB8: 0x7C86382E,
                    0x80029EBC: 0x98890001, 0x80029EC8: 0x98890000,
                    0x80029EF4: 0x98890003, 0x80029F54: 0x98890003,
                    0x80029F5C: 0x39080001, 0x80029F60: 0x39290004})
        self.words({0x80029A4C: 0x3BE31C98, 0x80029A6C: 0x881F0000,
                    0x80029A74: 0x8B7F0001, 0x80029A88: 0x881F0003,
                    0x80029ADC: 0x3BFF0004})

    def test_map_tail_stores_four_float_vectors_through_exact_end(self):
        self.words({0x8002ACB8: 0x38A31C98,
                    0x8002AD38: 0xC0030000, 0x8002AD3C: 0xD0050400,
                    0x8002AD40: 0xC0030004, 0x8002AD44: 0xD0050404,
                    0x8002AD48: 0xC0030008, 0x8002AD4C: 0xD0050408,
                    0x8002A100: 0xC01F0418, 0x8002A104: 0xD01F040C,
                    0x8002A10C: 0xD01F0410, 0x8002A114: 0xD01F0414,
                    0x8002A0A0: 0x38631C98, 0x8002A0A4: 0xD0230418,
                    0x8002A0A8: 0xD043041C, 0x8002A0AC: 0xD0630420,
                    0x8002A4F4: 0xD01F0424, 0x8002A50C: 0xD01F0428,
                    0x8002A518: 0xD01F042C})
        self.assertEqual(MAP + 0x400 + 4 * 3 * 4, MAP_END)


class RoomMapCaptureIntegration(unittest.TestCase):
    setUpClass = classmethod(capture.SceneOwnerCaptureIntegration.setUpClass.__func__)
    value = capture.SceneOwnerCaptureIntegration.value
    ranges = capture.SceneOwnerCaptureIntegration.ranges
    covered = capture.SceneOwnerCaptureIntegration.covered

    def test_each_complete_plain_cache_has_one_nonoverlapping_capture(self):
        for start, end in ((MAP, MAP_END), (ROOM_BYTES, ROOM_BYTES_END)):
            with self.subTest(start=hex(start)):
                overlaps = [(base, size) for base, size in self.ranges()
                            if base < end and start < base + size]
                self.assertEqual(overlaps, [(start, end - start)])
        self.assertEqual(sum(size for _, size in self.ranges()),
                         self.value('kStateStaticsSize'))

    def test_native_room_map_lookup_family_is_complete(self):
        for start, end in ((0x803C1C60, MAP), (MAP, MAP_END),
                           (MAP_END, 0x803C2138),
                           (0x803C2138, 0x803C236C),
                           (0x803C236C, ROOM_BYTES),
                           (ROOM_BYTES, ROOM_BYTES_END),
                           (ROOM_BYTES_END, 0x803C26C8)):
            self.assertTrue(self.covered(start, end - start), hex(start))

    def test_new_caches_do_not_capture_live_dvd_or_boot_render_owners(self):
        self.assertFalse(self.covered(0x803C8460, 4))
        self.assertFalse(self.covered(0x803989E0, 4))
        for function in ('captureStaticRanges', 'restoreStaticRanges', 'storeStaticRanges'):
            body = self.source.split('void ' + function + '() {', 1)[1].split('\n}\n', 1)[0]
            self.assertIn('i < kStateStaticRangeCount', body)
            self.assertIn('range.address', body)
            self.assertIn('range.size', body)


if __name__ == '__main__':
    unittest.main()
