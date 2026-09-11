"""Authenticate RoomInfo's borrowed GAME owner and the .42 post-warp failure."""
import hashlib
from pathlib import Path
import struct
import unittest

try:
    from scripts import compare_lm_archives as archives
    from scripts import inspect_rarc_asset as rarc
    from scripts import test_lm_hud_state as retail
    from scripts import test_lm_scene_owner_audit as capture
except ModuleNotFoundError:
    import compare_lm_archives as archives
    import inspect_rarc_asset as rarc
    import test_lm_hud_state as retail
    import test_lm_scene_owner_audit as capture

ROOT, END = 0x803C2138, 0x803C236C


class RoomInfoNativeEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_scene_setup_initializes_exact_fixed_plain_owner(self):
        self.words({0x8002E658: 0x3C60803C, 0x8002E668: 0x3BE32138,
                    0x8002D0BC: 0x38000004, 0x8002D0C4: 0x3800FFFF,
                    0x8002D0DC: 0x389E0000, 0x8002D0E4: 0xB0040004,
                    0x8002D1E0: 0xB0040082, 0x8002D1E4: 0x38840080,
                    0x8002D1EC: 0x387F0038, 0x8002D1F8: 0x907E0000,
                    0x8002D218: 0x907E0204, 0x8002D2C8: 0x907E0230})
        self.call(0x8002E67C, 0x8002D0B4)
        self.call(0x8002D1F0, 0x800B8658)
        self.assertEqual(self.data(0x802F5A50, 9), b'RoomInfo\0')
        # Root pointer + 256 halfword row indices + twelve field indices.
        self.assertEqual(4 + 256 * 2 + 12 * 4, END - ROOT)

    def test_borrowed_tool_is_selected_from_missions_captured_game_array(self):
        self.words({0x800B8668: 0x800D0CE8, 0x800B9788: 0x807E0018,
                    0x800B97B4: 0x801E0014, 0x800B97B8: 0x83FE0010,
                    0x800B97CC: 0x3BFF0008, 0x800B97D8: 0x801F0004,
                    0x800B9818: 0x7FE3FB78, 0x800B92C4: 0x57631838,
                    0x800B92D0: 0x38630008, 0x800B92EC: 0x38C00008,
                    0x800B92F4: 0x907E0010, 0x800B9340: 0x801E0010,
                    0x800B9348: 0x7C60E214})
        self.call(0x800B8670, 0x800B9764)
        self.call(0x800B92D4, 0x801C9408)
        self.call(0x800B92F0, 0x801F5504)
        self.call(0x800B934C, 0x800BB168)
        self.assertEqual(0x804A0AE0 + 0xCE8, 0x804A17C8)

    def test_crashed_render_consumer_pairs_fixed_owner_with_game_jmp_data(self):
        self.words({0x80013C18: 0x3C60803C, 0x80013C24: 0x38632138,
                    0x80013C34: 0x808D0218, 0x80013C38: 0x83C4000C,
                    0x8002D418: 0x54800DFC, 0x8002D420: 0xA8040004,
                    0x8002D434: 0x80A30000, 0x8002D43C: 0x8003020C,
                    0x8002D440: 0x80E50004, 0x8002D448: 0x8007000C,
                    0x8002D44C: 0x80A70008, 0x8002D470: 0x7C87202E})
        self.call(0x80013C40, 0x8002D418)
        self.assertEqual(0x80E6C2F0 + 0x2073FA69, 0xA15ABD59)


class RoomInfoCaptureProof(capture.SceneOwnerCaptureIntegration):
    def test_exact_owner_is_captured_once_as_part_of_scene_lookup_family(self):
        self.assertEqual(self.ranges().count((ROOT, END - ROOT)), 1)
        self.assertTrue(self.covered(ROOT, END - ROOT))
        self.assertTrue(self.covered(END, 0xFC))
        self.assertEqual(self.value('kSnapshotVersion'), 28)
        self.assertEqual(self.value('kStateStaticsSize'), 0x18D6C)
        self.assertEqual(self.value('kCameraObjectStateOffset'), 0x18EB4)
        self.assertEqual(self.value('kHeapDataOffset'), 0x191C0)
        self.assertEqual(self.value('kHeapDataOffset') - archives.schema(26)[2], 0x5A0)
        for function in ('captureStaticRanges', 'restoreStaticRanges', 'storeStaticRanges'):
            body = self.source.split('void ' + function + '() {', 1)[1].split('\n}\n', 1)[0]
            self.assertIn('i < kStateStaticRangeCount', body)
            self.assertIn('range.address', body)
            self.assertIn('range.size', body)


class RoomInfoPrivateRegression(unittest.TestCase):
    def test_original_42_archive_proves_exact_future_owner_mismatch(self):
        path = retail.ROOT.parent / 'sd-captures/lm-0.3.42-user-test4-20260908/lm_states/archive_00000005.lms'
        if not path.exists(): self.skipTest('Private .42 test-4 capture unavailable')
        data = path.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),
                         'afb85c4e05fc91ce2c7a526c65b7f3afcc996b8f7053563a5263628b078cea9d')
        result = archives.inspect_bytes(data)
        h = result.header
        self.assertEqual(result.envelope['buildCrc'], 0x9D445DA8)
        self.assertEqual(h['version'], 25)
        def image(address, size):
            self.assertTrue(archives.inside(address, size, h['heapStart'], h['heapEnd']))
            offset = 64 + h['heapDataOffset'] + address - h['heapStart']
            return data[offset:offset + size]
        def words(address, count=1):
            return struct.unpack('>' + str(count) + 'I', image(address, count * 4))
        table, count = words(h['missionMode'] + 16, 2)
        self.assertEqual((table, count), (0x80E6BFB8, 25))
        self.assertEqual(words(table - 24, 2), (0x484D000B, 8 + count * 8))
        self.assertEqual(words(table - 8, 2), (8, count))
        backing = words(h['mapArchive'] + 0x40)[0]
        size = words(backing + 4)[0]
        map_bytes = image(backing, size)
        # Pin this known fixture before using the existing retail-asset parser.
        files = rarc.rarc_files(map_bytes)
        room_info = [value for key, value in files.items() if key.lower().endswith('/roominfo')]
        self.assertEqual(len(room_info), 1)
        tool = table + 21 * 8
        vtable, jmp = words(tool, 2)
        self.assertEqual((tool, vtable, jmp), (0x80E6C060, 0x8034F1D4, 0x80DA20C0))
        self.assertTrue(archives.inside(jmp, len(room_info[0]), backing, backing + size))
        self.assertEqual(image(jmp, len(room_info[0])), room_info[0])
        self.assertEqual(words(jmp, 4), (72, 13, 0xAC, 0x50))
        future_tool = tool + 0x220
        self.assertEqual(future_tool, 0x80E6C280)
        self.assertNotEqual(words(future_tool)[0], vtable)
        # Future RoomInfo's pointer instead finds archive links in restored GAME.
        self.assertEqual(words(future_tool + 4)[0], 0x80E6C2F0)
        self.assertNotIn('static.roomInfo.crc', result.facts)
        self.assertTrue(any(start == ROOT and size == END - ROOT
                            for _, start, size in archives.schema(26)[0]))


if __name__ == '__main__':
    unittest.main()
