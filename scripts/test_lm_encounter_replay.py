"""Authenticate encounter replay prerequisites against local clean JP data."""
from pathlib import Path
import unittest

from audit_lm_encounters import field_hash, rows
from test_lm_door_state import DoorRetailProofTests

ROOT = Path(__file__).resolve().parents[1]
EXTRACTED = ROOT / "build/gaddwarp/extracted/clean"


class EncounterRetailProofTests(DoorRetailProofTests):
    def test_eventload_is_a_persistent_play_limit(self):
        # Descriptor +24 is EventLoad, copied to record +4C. Nonzero limits
        # compare the persistent count at 803C2E10 +20 + event number.
        self.check({0x8002D63C: 0x3A610050, 0x8002D79C: 0x38D30000,
                    0x8002D7A0: 0x38BF00F4, 0x8002D830: 0x3861002C,
                    0x8002B164: 0x80040024, 0x8002B16C: 0xB003004C,
                    0x8002B1D4: 0x801F0024, 0x8002B1E4: 0x60000004,
                    0x8002B1F8: 0x38842E10, 0x8002B208: 0x88630020,
                    0x8002B20C: 0x7C030000, 0x8002B218: 0x60000040,
                    0x8002BD14: 0xA81C0038, 0x8002BD1C: 0x88640020,
                    0x8002BD20: 0x38030001, 0x8002BD24: 0x98040020})
        self.check({0x802F5B0C: 0x4576656E, 0x802F5B10: 0x744C6F61,
                    0x802F5B14: 0x64000000})

    def test_native_foyer_entry_setter_is_pointer_only_and_links_bottom(self):
        self.check({0x8001875C: 0x7C7E1B78, 0x8001876C: 0x881E0006,
                    0x80018770: 0x60000080, 0x80018774: 0x981E0006,
                    0x80018778: 0x801E0014, 0x8001877C: 0x60000008,
                    0x80018790: 0x801E0010, 0x800187A4: 0xA00300A0,
                    0x800187A8: 0x60000002, 0x800187AC: 0xB00300A0,
                    0x800187C0: 0x28000002, 0x800187D0: 0x38848C50,
                    0x800187D8: 0xA804003C, 0x800187E0: 0x1C00014C,
                    0x800187F4: 0x881E0006, 0x800187F8: 0x60000080,
                    0x800187FC: 0x981E0006})


class EncounterAssetProofTests(unittest.TestCase):
    def event_rows(self, map_id):
        path = EXTRACTED / f"Map/map{map_id}/jmp/eventinfo"
        if not path.exists(): self.skipTest("Optional extracted clean JP assets unavailable")
        return rows(path)

    def test_jmap_field_hash_uses_retail_32_bit_overflow(self):
        self.assertEqual(field_hash("appear_flag"), 0x00ED3950)
        self.assertEqual(field_hash("disappear_flag"), 0x01B8D569)
        self.assertEqual(field_hash("EventLoad"), 0x01F68D77)
        self.assertEqual(field_hash("room_no"), 0x00C9952D)

    def test_room_resets_rearm_the_exact_exhausted_records(self):
        records = {r["EventNo"]: r for r in self.event_rows(2)}
        for event, trigger in ((22, 169), (61, 14), (76, 198)):
            self.assertEqual(records[event]["EventLoad"], 1)
            self.assertEqual(records[event]["EventFlag"], trigger)
        self.assertEqual(records[50]["EventLoad"], 0)
        nursery = EXTRACTED / "Event/event22/text/event22.txt"
        self.assertIn('<CHECKFLAG>(89)"null""start"', nursery.read_text(encoding="shift_jis"))
        warp = EXTRACTED / "Event/event50/text/event50.txt"
        self.assertIn("<WARP>(10)", warp.read_text(encoding="shift_jis"))

    def test_boss_intros_require_rearm_not_extra_story_flags(self):
        for map_id, event in ((9, 75), (10, 64), (11, 72), (13, 66)):
            record = next(r for r in self.event_rows(map_id) if r["EventNo"] == event)
            self.assertEqual(record["EventLoad"], 1)
            self.assertEqual(record["EventFlag"], 0)
            self.assertEqual(record["disappear_flag"], 0)


if __name__ == "__main__":
    unittest.main()
