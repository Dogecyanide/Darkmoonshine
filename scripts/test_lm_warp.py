"""Check recovered warp destinations against source records and clean-JP ABI."""

from __future__ import annotations

import hashlib
import importlib.util
import re
import struct
import unittest
from pathlib import Path

from gen_lm_warp_points import points, render


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "lm_diag/src/lm_warp.cpp").read_text(encoding="utf-8")
POINTS = {int(row["id"]): row for row in points()}
DESTINATIONS = re.findall(
    r'\{"([^"]+)", (\d+)u, (\d+)u, (\d+)u, (\d+|0xFF)u\}', SOURCE
)
CLEAN_DOL = ROOT / "build-lm-diag/clean_glmj_main.dol"
EXTRACTED = ROOT / "build/gaddwarp/extracted"


class WarpDestinationTests(unittest.TestCase):
    def test_generated_data_matches_recovered_csv(self) -> None:
        self.assertEqual((ROOT / "lm_diag/src/lm_warp_points.inc").read_text(), render())
        self.assertEqual(len(POINTS), 114)
        self.assertEqual(set(range(124)) - POINTS.keys(),
                         {82, 84, 93, 94, 95, 96, 99, 105, 108, 121})

    def test_every_mansion_choice_and_alternate_has_the_expected_room(self) -> None:
        self.assertEqual(len(DESTINATIONS), 68)
        for name, map_id, point, alternate, room in DESTINATIONS:
            if int(map_id) != 2:
                continue
            for identifier in (int(point), int(alternate)):
                with self.subTest(name=name, point=identifier):
                    self.assertIn(identifier, POINTS)
                    self.assertEqual(int(POINTS[identifier]["room_no"]), int(room))
                    self.assertTrue(-6000 <= float(POINTS[identifier]["x"]) <= 6000)
                    self.assertTrue(-1500 <= float(POINTS[identifier]["y"]) <= 2500)
                    self.assertTrue(-8000 <= float(POINTS[identifier]["z"]) <= 2000)

    def test_boss_maps_match_recovered_event01(self) -> None:
        self.assertEqual({name: int(map_id) for name, map_id, *_ in DESTINATIONS
                          if int(map_id) != 2},
                         {"Chauncey": 10, "Bogmire": 13, "Boolossus": 11, "King Boo": 9})

    @unittest.skipUnless(CLEAN_DOL.exists(), "maintainer clean-JP DOL not available")
    def test_hook_calls_the_verified_retail_setup_abi(self) -> None:
        from dolreader.dol import DolFile

        data = CLEAN_DOL.read_bytes()
        self.assertEqual(hashlib.sha1(data).hexdigest(),
                         "722005ea9c1eab54b114f814734d8f327e5614ee")
        with CLEAN_DOL.open("rb") as stream:
            dol = DolFile(stream)
            dol.seek(0x800E2E00)
            self.assertEqual(dol.read(12).hex(), "7fe3fb78809f0e4048000c15")
            dol.seek(0x800E3A74)
            self.assertEqual(dol.read(4).hex(), "480000b5")
        spec = importlib.util.spec_from_file_location("warp_patches", ROOT / "lm_diag/patches.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        hooks = [p for p in module.patches if p.get("sym") == "lmWarpPrepareAppearance"]
        self.assertEqual(len(hooks), 1)
        self.assertEqual((hooks[0]["lmj"], hooks[0]["expected"]), (0x800E2E08, 0x48000C15))

    @unittest.skipUnless(EXTRACTED.exists(), "maintainer extracted comparison data not available")
    def test_cloned_template_preserves_full_player_create_row(self) -> None:
        clean = (EXTRACTED / "clean/Map/map2/jmp/characterinfo").read_bytes()
        patched = (EXTRACTED / "patched/Map/map2/jmp/characterinfo").read_bytes()
        self.assertEqual(struct.unpack_from(">4I", clean), (131, 23, 0x124, 0xB8))
        self.assertEqual(struct.unpack_from(">4I", patched), (244, 23, 0x124, 0xB8))
        self.assertEqual(clean[16:0x124], patched[16:0x124])
        template = clean[0x124 + 39 * 0xB8:0x124 + 40 * 0xB8]
        self.assertEqual(template[0x18:0x1E], b"luige\0")
        self.assertEqual(struct.unpack_from(">I", template, 0xAC)[0], 0)
        editable = set(range(12)) | set(range(16, 20)) | set(range(0x98, 0x9C)) | set(range(0xAC, 0xB0))
        string_ranges = set(range(0x18, 0x98))
        exposed = {int(p) for _, m, p, _, _ in DESTINATIONS if int(m) == 2}
        exposed |= {int(p) for _, m, _, p, _ in DESTINATIONS if int(m) == 2}
        seen = set()
        for i in range(244):
            row = patched[0x124 + i * 0xB8:0x124 + (i + 1) * 0xB8]
            if row[0x18:0x1E] != b"luige\0":
                continue
            identifier = struct.unpack_from(">I", row, 0xAC)[0]
            if identifier not in exposed:
                continue
            seen.add(identifier)
            with self.subTest(point=identifier):
                for j in range(0xB8):
                    if j not in editable | string_ranges:
                        self.assertEqual(row[j], template[j], f"extra scalar field at {j:#x}")
                for offset in (0x18, 0x38, 0x58, 0x78):
                    value = row[offset:offset + 32].split(b"\0", 1)[0]
                    original = template[offset:offset + 32].split(b"\0", 1)[0]
                    if 63 <= identifier <= 75 and offset == 0x58:
                        self.assertEqual(value, b"(nulll)")
                        self.assertEqual(original, b"(null)")
                    else:
                        self.assertEqual(value, original)
                recovered = POINTS[identifier]
                for offset, field in ((0, "x"), (4, "y"), (8, "z"), (0x10, "yaw")):
                    self.assertEqual(struct.pack(">f", float(recovered[field])), row[offset:offset + 4])
        self.assertEqual(seen, exposed)


if __name__ == "__main__":
    unittest.main()
