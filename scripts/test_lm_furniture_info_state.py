"""Authenticate FurnitureInfo's GAME alias and the .43 post-warp door failure."""
import hashlib
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

ROOT, END = 0x803C236C, 0x803C2468


class FurnitureInfoNativeEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_initializer_borrows_mission_tool_and_caches_all_field_indices(self):
        self.words({0x8002E630: 0x3C60803C, 0x8002E638: 0x3863236C,
                    0x8002DAAC: 0x387F0160, 0x8002DAB8: 0x907E0000,
                    0x8002DAC4: 0x901E0004, 0x8002DACC: 0x807E0000,
                    0x8002DAD4: 0x80630004, 0x8002DAD8: 0x80030000,
                    0x8002DADC: 0x901E0004, 0x8002DCC8: 0x907E0080})
        self.call(0x8002E640, 0x8002DA8C)
        self.call(0x8002DAB0, 0x800B8658)
        self.assertEqual(self.data(0x802F5B78, 14), b'FurnitureInfo\0')
        fields = [self.word(a) & 0xFFFF for a in range(0x8002DA8C, 0x8002DCE4, 4)
                  if self.word(a) >> 16 == 0x907E]
        self.assertEqual(sorted(fields), [0] + list(range(8, 0x84, 4)))

    def test_whole_tail_is_plain_decoded_property_state_not_a_second_owner(self):
        self.words({0x8002DE8C: 0x80A30000, 0x8002DE98: 0x80E50004,
                    0x8002DEC0: 0xD0030084, 0x8002E00C: 0x9083009C,
                    0x8002E52C: 0x988300E8, 0x8002E5F0: 0x908300F4,
                    0x8002E624: 0xD00300F8, 0x8002E628: 0x4E800020,
                    0x8002E8B0: 0x3C60803C, 0x8002E8B4: 0x3863236C,
                    0x8002E8BC: 0x90030004})
        code = [self.word(a) for a in range(0x8002DE8C, 0x8002E62C, 4)]
        fields = [word & 0xFFFF for word in code if word >> 16 in (0xD003, 0x9083, 0x9883)]
        self.assertEqual(sorted(fields), list(range(0x84, 0xFC, 4)))
        # Straight-line JMP decoding, with no allocation/service/OS callbacks.
        self.assertFalse(any(word >> 26 in (16, 18) for word in code))
        self.assertEqual(END - ROOT, 0xFC)

    def test_door_consumer_fault_is_jmp_magic_used_as_pointer(self):
        self.words({0x8007AB2C: 0x3C60803C, 0x8007AB30: 0x3A63236C,
                    0x8007AB34: 0x38730000, 0x8002DCF4: 0x3B630000,
                    0x8002DCFC: 0x549E063E, 0x8002DD0C: 0x807B0000,
                    0x8002DD10: 0x801B0024, 0x8002DD14: 0x80630004,
                    0x8002DD18: 0x1CC0000C, 0x8002DD1C: 0x8003000C,
                    0x8002DE0C: 0x801B0004})
        self.call(0x8007AB40, 0x8002DCE4)
        self.assertEqual(int.from_bytes(b'RARC', 'big') + 12, 0x5241524F)


class FurnitureInfoCaptureProof(capture.SceneOwnerCaptureIntegration):
    def test_whole_plain_owner_once_in_all_three_manifest_paths(self):
        self.assertEqual(self.ranges().count((ROOT, END - ROOT)), 1)
        self.assertTrue(self.covered(ROOT, END - ROOT))
        self.assertEqual(self.value('kFurnitureInfoStateStart'), ROOT)
        self.assertEqual(self.value('kFurnitureInfoStateEnd'), END)
        self.assertEqual(self.value('kSnapshotVersion'), 28)
        self.assertTrue(any(name == 'furnitureInfo' and start == ROOT and size == END - ROOT
                            for name, start, size in archives.schema(27)[0]))
        self.assertFalse(any(name == 'furnitureInfo' for name, _, _ in archives.schema(26)[0]))
        for function in ('captureStaticRanges', 'restoreStaticRanges', 'storeStaticRanges'):
            body = self.source.split('void ' + function + '() {', 1)[1].split('\n}\n', 1)[0]
            self.assertIn('i < kStateStaticRangeCount', body)
            self.assertIn('range.address', body)
            self.assertIn('range.size', body)
        # Overlapping manifests can overwrite neighboring sections on decode.
        ranges = sorted(self.ranges())
        self.assertTrue(all(a + n <= b for (a, n), (b, _) in zip(ranges, ranges[1:])))
        # Adjacent scene additions do not pull live-service exclusions in.
        for address in (0x803989E0, 0x803C8460, 0x803CC460, 0x803CCC4C):
            self.assertFalse(self.covered(address, 4))


class FurnitureInfoPrivateRegression(unittest.TestCase):
    def test_original_43_archive_proves_exact_future_tool_mismatch(self):
        path = retail.ROOT.parent / 'sd-captures/lm-0.3.43-boneyard-door-20260908/lm_states/archive_00000001.lms'
        if not path.exists(): self.skipTest('Private .43 Boneyard/door capture unavailable')
        data = path.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),
                         '146779974a83acbf4ddb62ac39d387a505e135fd09dacc170ea2a3be0b6274e1')
        result = archives.inspect_bytes(data)
        h = result.header
        self.assertEqual(result.envelope['buildCrc'], 0xB1F105D5)
        self.assertEqual(h['version'], 26)

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
        files = rarc.rarc_files(image(backing, size))
        furniture_info = [value for key, value in files.items()
                          if key.lower().endswith('/furnitureinfo')]
        self.assertEqual(len(furniture_info), 1)
        tool = table + 9 * 8
        vtable, jmp = words(tool, 2)
        self.assertEqual((tool, vtable, jmp), (0x80E6C000, 0x8034F1D4, 0x80D54FE0))
        self.assertTrue(archives.inside(jmp, len(furniture_info[0]), backing, backing + size))
        self.assertEqual(len(furniture_info[0]), 143712)
        self.assertEqual(image(jmp, len(furniture_info[0])), furniture_info[0])
        self.assertEqual(words(jmp, 4), (731, 35, 0x1B4, 0xC4))
        # The observed scene reload moved the live ToolData array by +0x220.
        future_tool = tool + 0x220
        self.assertEqual(future_tool, 0x80E6C220)
        self.assertEqual(words(future_tool, 2), (0x80BB5805, 0x52415243))
        self.assertNotEqual(words(future_tool)[0], vtable)
        self.assertEqual(words(future_tool + 4)[0] + 12, 0x5241524F)
        self.assertNotIn('static.furnitureInfo.crc', result.facts)


if __name__ == '__main__':
    unittest.main()
