"""Room-tool recipe, packed-index, and accepted-reload regression contracts."""

import ctypes
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_lm_door_state import DoorRetailProofTests

ROOT = Path(__file__).resolve().parents[1]


class RoomToolsNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-room-tools-")
        output = Path(cls.temp.name) / ("room.dll" if os.name == "nt" else "room.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_room_tools_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        subprocess.run(command, check=True, capture_output=True, env=env)
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.roomMatches.argtypes = [ctypes.c_uint] * 4
        cls.lib.roomMatches.restype = ctypes.c_int
        cls.lib.resetRecipe.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        cls.lib.resetRecipe.restype = ctypes.c_int
        cls.lib.darkPersistence.argtypes = [ctypes.c_uint]
        cls.lib.darkPersistence.restype = ctypes.c_uint
        cls.lib.foyerCompanion.argtypes = [ctypes.c_uint] * 4
        cls.lib.foyerCompanion.restype = ctypes.c_uint
        cls.lib.resetEvent.argtypes = [ctypes.c_uint] * 2
        cls.lib.resetEvent.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def recipe(self, room, items=0):
        out = (ctypes.c_uint * 26)(*[0xDEADBEEF] * 26)
        ok = self.lib.resetRecipe(room, items, out)
        self.assertEqual(out[24], 0xDEADBEEF)
        self.assertEqual(out[25], 0xDEADBEEF)
        if not ok:
            return None
        self.assertLessEqual(out[0], 11)
        return out[1], [(out[2 + i * 2], out[3 + i * 2]) for i in range(out[0])]

    def test_packed_parlor_index_is_not_logical_room_id(self):
        packed = 0x24010323
        self.assertEqual(self.lib.roomMatches(packed, packed, packed, 74), 1)
        self.assertNotEqual(packed >> 24, packed & 255)
        self.assertEqual(self.lib.roomMatches(0x2A010427, packed, packed, 74), 0)

    def test_count_last_row_and_sentinels(self):
        last = 0x49FF0546
        self.assertEqual(self.lib.roomMatches(last, last, last, 74), 1)
        for count in (0, 73, 75, 0xFFFFFFFF):
            self.assertEqual(self.lib.roomMatches(last, last, last, count), 0)
        for invalid in (0xFFFFFFFF, 0x4AFF0546, 0x49FF05FF):
            self.assertEqual(self.lib.roomMatches(invalid, last, last, 74), 0)
            self.assertEqual(self.lib.roomMatches(last, invalid, last, 74), 0)
            self.assertEqual(self.lib.roomMatches(last, last, invalid, 74), 0)

    def test_reset_repair_values(self):
        expected = {3: [13], 14: [41, 19], 16: [74, 59, 67],
                    24: [46, 47, 89], 25: [51, 37, 65], 28: [25, 18, 38, 36],
                    35: [14, 8], 40: [83, 6, 198], 41: [49, 50, 52], 55: [40],
                    57: [31, 177, 178, 179, 180, 181, 182, 183, 55],
                    59: [81], 61: [28], 66: [63, 64], 70: [66]}
        for room in range(74):
            dark, edits = self.recipe(room)
            self.assertEqual(dark, int(room != 70))
            self.assertEqual(edits, [(flag, 0) for flag in expected.get(room, [])])
        for invalid in (74, 255, 0xFFFFFFFF):
            self.assertIsNone(self.recipe(invalid))

    def test_scoped_event_play_count_rearms(self):
        expected = {24: 22, 35: 61, 40: 76}
        for room in range(74):
            self.assertEqual(self.lib.resetEvent(room, 0), expected.get(room, 0xFFFFFFFF))
            self.assertEqual(self.lib.resetEvent(room, 1), 64 if room == 24 else 0xFFFFFFFF)
            self.assertEqual(self.lib.resetEvent(room, 2), 0xFFFFFFFF)

    def test_foyer_pair_is_symmetric_and_uses_logical_ids(self):
        # Deliberately distinct table row and logical room.
        record = 0x17010349
        self.assertEqual(self.lib.foyerCompanion(2, 23, 74, record), 73)
        self.assertEqual(self.lib.foyerCompanion(73, 23, 74, record), 2)
        self.assertEqual(self.lib.foyerCompanion(35, 23, 74, record), 0xFFFFFFFF)
        for index, count, packed in ((23, 23, record), (24, 74, record),
                (23, 75, record), (23, 0, record), (23, 74, 0xFFFFFFFF),
                (23, 74, 0x170000FF), (23, 74, 0x17000002)):
            self.assertEqual(self.lib.foyerCompanion(2, index, count, packed), 0xFFFFFFFF)

    def test_fortune_teller_reverts_only_already_shown_items(self):
        for mask in range(32):
            dark, edits = self.recipe(3, mask)
            expected = [(13, 0)]
            for i in range(5):
                if mask & (1 << i):
                    expected += [(21 + i * 3, 1), (20 + i * 3, 0)]
            self.assertEqual((dark, edits), (1, expected))

    def test_reset_preserves_all_other_persistence_bits(self):
        for before in range(65536):
            self.assertEqual(self.lib.darkPersistence(before), before & ~2)

    def test_real_room_table_uses_74_rows_and_distinct_ids(self):
        path = ROOT / "build-lm-emu/diagnostic-capture-0.3.30/mem1.bin"
        if not path.exists():
            self.skipTest("Optional real MEM1 unavailable")
        raw = path.read_bytes()
        word = lambda address: struct.unpack_from(">I", raw, address - 0x80000000)[0]
        count = raw[0x4A0CF4]
        self.assertEqual(count, 74)
        table = word(0x804A0CF0)
        seen = set()
        for index in range(count):
            packed = word(table + index * 0x14C + 0x10)
            self.assertEqual(self.lib.roomMatches(packed, packed, packed, count), 1)
            seen.add(packed & 255)
        self.assertEqual(seen, set(range(74)))


class RoomToolsRetailTests(DoorRetailProofTests):
    def test_blackout_live_setter_and_room_persistence_consumers(self):
        self.check({0x800374AC: 0x2C1F0001, 0x800374B4: 0x3860003D,
                    0x800374C0: 0x3860003D, 0x800374CC: 0x480810CD,
                    0x800197F0: 0x8003000C, 0x800197F4: 0x5400463E,
                    0x800197F8: 0x1C00014C,
                    0x800186AC: 0x80030010, 0x800186B4: 0x38842E10,
                    0x800186B8: 0x388400A0, 0x800186BC: 0x54000DFC,
                    0x800186C0: 0x7C04022E, 0x800186C4: 0x540007BD,
                    0x800186D0: 0x60000080, 0x800186DC: 0x60000008,
                    0x800198E4: 0x54000DFC, 0x800198F0: 0x540007FA,
                    0x80019920: 0xA804003C, 0x80019924: 0x5400063E})


class RoomToolsIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.practice = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        cls.warp = (ROOT / "lm_diag/src/lm_warp.cpp").read_text()

    def test_room_changes_only_follow_successful_queue(self):
        armed = self.warp.split("if (sPhase == Phase::Armed)", 1)[1]
        prepare = armed.index("LMPractice::prepareRoomReload")
        queue = armed.index("if (!LmWarpPublishAndQueue")
        fail = armed.index('fail("WARP: SCENE QUEUE BUSY"')
        commit = armed.index("LMPractice::commitRoomReload")
        self.assertLess(prepare, queue)
        self.assertLess(queue, fail)
        self.assertLess(fail, commit)
        self.assertIn("return;", armed[fail:commit])
        self.assertIn("if (active()) { LMNotice::show(LM_POPUP_BUSY); return false; }",
                      self.warp.split("bool requestRoomReload(", 1)[1])

    def test_reset_writes_only_persistence_and_recipe_not_live_objects(self):
        commit = self.practice.split("void commitRoomReload(", 1)[1]
        reset = commit.split("for (u32 i = 0u; i < sResetPersistenceCount;", 1)[1]
        self.assertIn("LmRoomDarkPersistence(readHalf(address))", reset)
        self.assertIn("sResetRecipe.edits[i].id", reset)
        for forbidden in ("kSaveRequestAddress", "kRoomLightAddress", "killActor", "player +"):
            self.assertNotIn(forbidden, reset)

    def test_clear_recipes_cover_every_logical_mansion_room(self):
        def array(name):
            content = re.search(rf"constexpr u8 {name}\[\] = \{{(.*?)\}};", self.practice, re.S)[1]
            return {int(value) for value in re.findall(r"(\d+)u", content)}
        generic = array("kClearGeneric235Rooms")
        empty = array("kClearGeneric236Rooms")
        explicit = re.search(r"constexpr RoomClearRecipe kRoomClearRecipes\[\] = \{(.*?)\};", self.practice, re.S)[1]
        explicit = {int(value) for value in re.findall(r"\{(\d+)u,", explicit)}
        self.assertEqual(generic | empty | explicit, set(range(72)))
        self.assertFalse(generic & empty or generic & explicit or empty & explicit)

    def test_menu_selects_real_boo_safe_warps_and_element_module(self):
        self.assertIn("LMWarp::request(sSelection, sBooSafe)", self.practice)
        self.assertIn("sPage == Page::Warps && edge(buttons, kButtonX)", self.practice)
        self.assertIn("LMElements::apply(sElementChoice, playerAddress())", self.practice)
        self.assertIn("!LMState::readyForActionNow()", self.practice.split("bool beginAction()", 1)[1])
        self.assertNotIn("static_cast<u32>(playerRoom) == loadedRoom", self.practice)

    def test_reset_coverage_uses_only_recovered_entries(self):
        rows = re.search(r"constexpr Destination kDestinations\[\] = \{(.*?)\};",
                         self.warp, re.S)[1]
        rooms = {int(room) for room in re.findall(
            r'\{"[^"]+", 2u, \d+u, \d+u, (\d+)u\}', rows)}
        self.assertEqual(len(rooms), 60)
        self.assertEqual(set(range(72)) - rooms,
                         {15, 18, 26, 29, 31, 32, 54, 58, 64, 65, 68, 71})
        self.assertEqual(len(re.findall(r'\{"[^"]+", (?:9|10|11|13)u,', rows)), 4)
        self.assertIn('"NO ENTRY POINT"', self.practice)
        self.assertIn('"ROOM CLEAR REQUESTED"', self.practice)


if __name__ == "__main__":
    unittest.main()
