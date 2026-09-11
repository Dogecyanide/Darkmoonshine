"""Authenticate the plain player/effect cache and its runtime-record boundary."""

from pathlib import Path
import struct
import unittest

try:
    from scripts import test_lm_hud_state as retail
except ModuleNotFoundError:
    import test_lm_hud_state as retail

ROOT = Path(__file__).resolve().parents[1]
START, CACHE, END = 0x803CC718, 0x803CC730, 0x803CC818


class PlayerQueryRetailEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_plain_vector_scratch_ends_at_cache(self):
        self.words({0x8013C218: 0x38C3C724, 0x8013C224: 0x38E3C718,
                    0x8013C220: 0x90860000, 0x8013C22C: 0x90060004,
                    0x8013C23C: 0x90060008, 0x8013C248: 0x90C70000,
                    0x8013C24C: 0x90070004, 0x8013C254: 0x90070008})
        self.assertEqual(START + 2 * 12, CACHE)

    def test_initializer_has_plain_flags_and_two_self_pointers(self):
        self.words({0x80143E30: 0x3C60803D, 0x80143E34: 0x3883C730,
                    0x80143E38: 0x38600000, 0x80143E3C: 0x9864005B,
                    0x80143E40: 0x38040060, 0x80143E44: 0x986400BB,
                    0x80143E48: 0x908400E0, 0x80143E4C: 0x900400E4,
                    0x80143E50: 0x4E800020})
        self.assertEqual(CACHE + 0xE8, END)

    def test_normal_update_refreshes_all_five_reported_heap_pointers(self):
        self.call(0x8000B9EC, 0x80143AD8)
        self.words({0x80143AEC: 0x3BE3C730, 0x80143AF4: 0x807F00E0,
                    0x80143AFC: 0x807F00E4})
        self.call(0x80143AF8, 0x8012A5A8)
        self.call(0x80143B04, 0x8012A748)
        self.words({0x8012A5D0: 0x80030000, 0x8012A5DC: 0x901E0000,
                    0x8012A674: 0x801F0098, 0x8012A678: 0x901E001C,
                    0x8012A67C: 0x801F009C, 0x8012A680: 0x901E0020,
                    0x8012A694: 0x801F0034, 0x8012A698: 0x901E0038,
                    0x8012A69C: 0x801F0038, 0x8012A6A0: 0x901E003C})

    def test_query_consumers_use_cache_and_native_member_function_descriptor(self):
        self.words({0x80143E14: 0x3863C730, 0x80143E18: 0x806300E0,
                    0x80143E24: 0x3863C730, 0x80143E28: 0x806300E4,
                    0x80143C58: 0x3947C730, 0x80143C74: 0x910A00D4,
                    0x80143C80: 0x900A00D8, 0x80143C8C: 0x900A00DC,
                    0x80142A30: 0x809B00E0, 0x80142A3C: 0x8084001C,
                    0x80142B00: 0x399B00D4})
        self.call(0x80142B04, 0x801F58D0)

    def test_next_twelve_bytes_are_excluded_runtime_registration(self):
        self.words({0x80146A3C: 0x38A3C818, 0x80146A4C: 0x3865000C,
                    0x80146A54: 0x38846A84})
        self.call(0x80146A70, 0x801F51C8)
        self.words({0x801F51CC: 0x90050000, 0x801F51D0: 0x90850004,
                    0x801F51D4: 0x90650008, 0x801F51D8: 0x90AD1840})
        self.assertEqual(END + 12, 0x803CC824)


class PlayerQueryFixtureEvidence(unittest.TestCase):
    def test_two_captures_have_different_borrowed_pointers_but_same_self_links(self):
        paths = [ROOT / 'build-lm-emu' / name / 'mem1.bin' for name in (
            'diagnostic-capture-0.3.30', 'diagnostic-capture-0.3.30-newreport')]
        if not all(path.exists() for path in paths):
            self.skipTest('Private diagnostic captures unavailable')
        memories = [path.read_bytes() for path in paths]
        for memory in memories:
            self.assertEqual(len(memory), 0x1800000)
        def word(memory, address):
            return struct.unpack_from('>I', memory, address - 0x80000000)[0]
        for offset in (0, 0x1C, 0x20, 0x38, 0x3C):
            pointers = [word(memory, CACHE + offset) for memory in memories]
            self.assertNotEqual(*pointers)
            self.assertTrue(all(0x80000000 < value < 0x81800000 for value in pointers))
        for memory in memories:
            self.assertEqual(word(memory, CACHE + 0xE0), CACHE)
            self.assertEqual(word(memory, CACHE + 0xE4), CACHE + 0x60)
            self.assertEqual(word(memory, END + 4), 0x80146A84)
            self.assertEqual(word(memory, END + 8), END + 12)


class PlayerQueryCaptureIntegration(unittest.TestCase):
    def test_exact_plain_range_is_captured_and_accounted(self):
        source = (ROOT / 'lm_diag/src/lm_state.cpp').read_text()
        self.assertRegex(source, r'kPlayerQueryCacheStateStart\s*=\s*0x803CC718u;')
        self.assertRegex(source, r'kPlayerQueryCacheStateEnd\s*=\s*0x803CC818u;')
        self.assertRegex(source, r'\{kPlayerQueryCacheStateStart,\s*'
                         r'kPlayerQueryCacheStateEnd - kPlayerQueryCacheStateStart\}')
        self.assertIn('(kPlayerQueryCacheStateEnd - kPlayerQueryCacheStateStart)', source)
        self.assertEqual(END - START, 0x100)


if __name__ == '__main__':
    unittest.main()
