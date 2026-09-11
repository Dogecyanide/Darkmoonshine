"""Authenticate dialogue ownership and demonstrate mixed-epoch cleanup failure."""

from pathlib import Path
import struct
import unittest

try:
    from scripts import test_lm_hud_state as retail
except ModuleNotFoundError:
    import test_lm_hud_state as retail

ROOT = Path(__file__).resolve().parents[1]
START, END = 0x803C3730, 0x803C4448
PICTURES, COUNT, STRIDE = 0x803C4130, 25, 0x18
FIXTURES = [ROOT / 'build-lm-emu' / name / 'mem1.bin' for name in (
    'diagnostic-capture-0.3.30', 'diagnostic-capture-0.3.30-newreport')]


class DialogueRetailEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_crash_is_first_dialogue_picture_virtual_destructor(self):
        self.words({0x8004338C: 0x3BE33730, 0x80043390: 0x387F0A00,
                    0x8003C230: 0x80630010, 0x8003C240: 0x81830000,
                    0x8003C244: 0x38800001, 0x8003C248: 0x818C0008,
                    0x8003C250: 0x4E800021, 0x8003C254: 0x807F0014,
                    0x8003C26C: 0x818C0008})
        for source, target in ((0x80043398, 0x8003C21C),
                               (0x80043BA0, 0x80043378),
                               (0x8000BE2C, 0x80043B80)):
            self.call(source, target)
        self.assertEqual(START + 0xA00, PICTURES)
        self.assertEqual((0xFF200091 + 8) & 0xFFFFFFFF, 0xFF200099)
        self.assertEqual(0 + 8, 8)

    def test_scene_constructor_and_cleanup_cover_six_plus_nineteen(self):
        allocations = [0x800419AC, 0x800419B8, 0x800419C4,
                       0x800419D0, 0x800419DC, 0x80041A00]
        for index, address in enumerate(allocations):
            self.call(address, 0x8003B99C)
            self.assertEqual(self.word(address - 8), 0x387E0A00 + index * STRIDE)
            self.call(0x80043398 + index * 8, 0x8003C21C)
            self.assertEqual(self.word((0x80043390 if index == 0 else 0x80043394 + index * 8)),
                             0x387F0A00 + index * STRIDE)
        self.words({0x80041A08: 0x1C1A0018, 0x80041A1C: 0x3B7B0A90,
                    0x80041A30: 0x2C1A0013, 0x80041A38: 0x3B7B0018,
                    0x800433C8: 0x1C1E0018, 0x800433D0: 0x3BFF0A90,
                    0x800433E0: 0x2C1E0013, 0x800433E4: 0x3BFF0018})
        self.call(0x80041A28, 0x8003B99C)
        self.call(0x800433D8, 0x8003C21C)
        self.assertEqual(6 + 19, COUNT)
        self.assertEqual(PICTURES + COUNT * STRIDE, 0x803C4388)

    def test_static_initializer_independently_confirms_wrapper_boundaries(self):
        self.words({0x80043C00: 0x3BE33730, 0x80043C3C: 0x387F0A90,
                    0x80043C44: 0x38C00018, 0x80043C48: 0x38E00013})
        for index in range(6):
            self.assertEqual(self.word(0x80043C04 + index * 8),
                             0x387F0A00 + index * STRIDE)
            self.call(0x80043C08 + index * 8, 0x8003B950)
        self.call(0x80043C4C, 0x801F534C)

    def test_picture_allocation_and_small_controller_use_current_heap(self):
        self.words({0x8003B9B8: 0x3860017C, 0x8003B9F0: 0x3C608030,
                    0x8003B9F4: 0x380397DC, 0x8003B9FC: 0x93BE0010,
                    0x80043AF8: 0x38600006, 0x80043B1C: 0x93ED0568,
                    0x80043B90: 0x83ED0568})
        self.call(0x8003B9C0, 0x801C9308)
        self.call(0x80043B08, 0x801C9308)
        self.call(0x80043B18, 0x80041980)
        self.call(0x80043BA8, 0x801C9508)
        self.assertEqual(0x804A0AE0 + 0x568, 0x804A1048)

    def test_buffers_palette_and_scalar_arrays_fill_the_complete_plain_span(self):
        self.words({0x80041994: 0x3BC43730,
                    0x80041A74: 0x38FE0000, 0x80041A94: 0x393E0800,
                    0x80041ADC: 0x99070000, 0x80041AE8: 0x99090000,
                    0x80041AF0: 0x38E70200, 0x80041B08: 0x39290080,
                    0x80041B8C: 0x389E0C58, 0x80041B90: 0x38BE0C70,
                    0x80041B94: 0x38DE0C88, 0x80041B98: 0x38FE0CA0,
                    0x80041A88: 0x38DE0CB8, 0x80041A8C: 0x387E0CC8,
                    0x80041A98: 0x395E0CD8, 0x80041A9C: 0x397E0CE8,
                    0x80041A7C: 0x38BE0CF8, 0x80041A80: 0x389E0D08})
        # Constructor clears the first byte of each string, not the whole buffer.
        self.assertEqual(4 * 0x200 + 4 * 0x80, 0xA00)
        self.assertEqual(0xA00 + COUNT * STRIDE, 0xC58)
        self.assertEqual(0xC58 + 4 * 24, 0xCB8)
        self.assertEqual(0xCB8 + 6 * 4 * 4, END - START)


class MixedEpochFixtures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not all(path.exists() for path in FIXTURES):
            raise unittest.SkipTest('Both optional private MEM1 captures are required')
        cls.images = [path.read_bytes() for path in FIXTURES]
        if any(len(image) != 0x1800000 for image in cls.images):
            raise AssertionError('Wrong private MEM1 fixture size')

    @staticmethod
    def word(image, address):
        if address & 3 or not 0x80000000 <= address <= 0x817FFFFC:
            raise AssertionError(f'Foreign forensic read {address:08X}')
        return struct.unpack_from('>I', image, address - 0x80000000)[0]

    def endpoints(self, image):
        return [self.word(image, PICTURES + i * STRIDE + 16) for i in range(COUNT)]

    def allocations(self, image):
        heap = self.word(image, 0x804A0B98)
        node = self.word(image, heap + 0x7C)
        previous, result = 0, {}
        while node:
            self.assertLess(len(result), 8192)
            self.assertNotIn(node + 16, result)
            self.assertEqual(self.word(image, node) >> 16, 0x484D)
            self.assertEqual(self.word(image, node + 8), previous)
            result[node + 16] = (self.word(image, node + 4), self.word(image, node) & 255)
            previous, node = node, self.word(image, node + 12)
        self.assertEqual(previous, self.word(image, heap + 0x80))
        return result

    def test_both_epochs_have_complete_valid_exact_owned_pictures(self):
        for image in self.images:
            allocation = self.allocations(image)
            pointers = self.endpoints(image)
            self.assertEqual(len(set(pointers)), COUNT)
            for pointer in pointers:
                self.assertEqual(allocation[pointer], (0x17C, 2))
                self.assertEqual(self.word(image, pointer), 0x802F97DC)

    def test_live_wrappers_with_saved_game_reproduce_invalid_vtable_class(self):
        saved, live = self.images
        saved_pointers, live_pointers = self.endpoints(saved), self.endpoints(live)
        self.assertEqual(sum(a != b for a, b in zip(saved_pointers, live_pointers)), COUNT)
        self.assertEqual([self.word(saved, pointer) for pointer in live_pointers], [0] * COUNT)
        self.assertEqual((saved_pointers[0], live_pointers[0]), (0x8136F540, 0x81382D08))
        self.assertEqual((saved_pointers[-1], live_pointers[-1]), (0x813723C0, 0x81385B88))
        # This is byte-only analysis. No state is imported or game code executed.
        restored_owners = saved[START - 0x80000000:END - 0x80000000]
        for index in range(COUNT):
            pointer = struct.unpack_from('>I', restored_owners, 0xA00 + index * STRIDE + 16)[0]
            self.assertEqual(self.word(saved, pointer), 0x802F97DC)


class DialogueCaptureIntegration(unittest.TestCase):
    def test_capture_whole_plain_manager_not_just_the_crashed_pointer(self):
        source = (ROOT / 'lm_diag/src/lm_state.cpp').read_text()
        self.assertRegex(source, r'kDialogueOwnerStateStart\s*=\s*0x803C3730u;')
        self.assertRegex(source, r'kDialogueOwnerStateEnd\s*=\s*0x803C4448u;')
        self.assertRegex(source, r'\{kDialogueOwnerStateStart,\s*kDialogueOwnerStateEnd - kDialogueOwnerStateStart\}')
        self.assertIn('(kDialogueOwnerStateEnd - kDialogueOwnerStateStart)', source)
        self.assertRegex(source, r'kSnapshotVersion\s*=\s*28u;')
        self.assertIn('kStateStaticsSize == 0x18D6Cu', source)
        self.assertIn('kCameraObjectStateOffset == 0x18EB4u', source)
        self.assertIn('kHeapDataOffset == 0x191C0u', source)
        # The font and destructor exclusion still precedes this new plain range.
        self.assertRegex(source, r'kGbhHudOwnerStateEnd\s*=\s*0x803C3388u;')
        self.assertRegex(source, r'kHudPictureOwnerStateStart\s*=\s*0x803C3400u;')
        self.assertRegex(source, r'kHudPictureOwnerStateEnd\s*=\s*0x803C3730u;')
        self.assertRegex(source, r'kTimerHudOwnerStateStart\s*=\s*0x803C4448u;')
        self.assertRegex(source, r'kGameSbss1Start\s*=\s*0x804A0CB0u;')
        self.assertRegex(source, r'kGameSbss1End\s*=\s*0x804A1D10u;')


if __name__ == '__main__':
    unittest.main()
