"""Japanese packed player-room decoding, compiled native and consumer contracts."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PlayerRoomNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-room-id-")
        output = Path(cls.temp.name) / ("room.dll" if os.name == "nt" else "room.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"), str(ROOT / "scripts/lm_room_id_harness.c"),
                   "-o", str(output)]
        if os.name != "nt": command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.player_room.argtypes = [ctypes.c_uint]
        cls.lib.player_room.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def test_actual_anteroom_storage_and_parlor_words(self):
        for packed, expected in ((0x2A010427, 39), (0x0E00040E, 14), (0x24010323, 35)):
            with self.subTest(packed=hex(packed)):
                self.assertEqual(self.lib.player_room(packed), expected)

    def test_all_room_bytes_are_unsigned(self):
        for room in range(256):
            self.assertEqual(self.lib.player_room(room), room)
            self.assertEqual(self.lib.player_room(0x2A010400 | room), room)

    def test_upper_fields_do_not_change_the_room(self):
        rng = random.Random(0x474C4D4A)
        for _ in range(10000):
            packed = rng.getrandbits(32)
            self.assertEqual(self.lib.player_room(packed), -1 if packed == 0xFFFFFFFF else packed & 255)

    def test_only_proven_full_word_sentinel_is_unset(self):
        self.assertEqual(self.lib.player_room(0xFFFFFFFF), -1)
        self.assertEqual(self.lib.player_room(0xFFFFFFFE), 254)
        self.assertEqual(self.lib.player_room(0x000000FF), 255)
        self.assertEqual(self.lib.player_room(0x2A0104FF), 255)


class PlayerRoomRetailAndConsumerContracts(unittest.TestCase):
    def test_verified_japanese_extract_instruction_masks_low_byte(self):
        import hashlib
        from dolreader.dol import DolFile
        clean = ROOT / "build-lm-diag/clean_glmj_main.dol"
        if not clean.exists():
            self.skipTest("Maintainer clean JP DOL is not available")
        self.assertEqual(hashlib.sha1(clean.read_bytes()).hexdigest(),
                         "722005ea9c1eab54b114f814734d8f327e5614ee")
        with clean.open("rb") as stream:
            dol = DolFile(stream)
            dol.seek(0x800DB98C)
            self.assertEqual(dol.read(8).hex(), "800400b45404063e")
        load, extract = 0x800400B4, 0x5404063E
        self.assertEqual((load >> 26, (load >> 16) & 31, load & 65535), (32, 4, 0xB4))
        self.assertEqual((extract >> 26, (extract >> 21) & 31,
                          (extract >> 16) & 31, (extract >> 11) & 31,
                          (extract >> 6) & 31, (extract >> 1) & 31),
                         (21, 0, 4, 0, 24, 31))
        mask = sum(1 << (31 - bit) for bit in range(24, 32))
        self.assertEqual(mask, 0xFF)

    def test_warp_and_metadata_use_the_shared_decoder(self):
        warp = (ROOT / "lm_diag/src/lm_warp.cpp").read_text(encoding="utf-8")
        tools = (ROOT / "lm_diag/src/lm_tools.cpp").read_text(encoding="utf-8")
        self.assertIn("sRoom = static_cast<u32>(LmPlayerRoomId(packedRoom));", warp)
        self.assertIn("if (sRoom == kNoRoom) { sStable = 0u; return; }", warp)
        self.assertIn("SUSAMUNE_LM_WARP_SETTLING, sDestination, packedRoom", warp)
        self.assertIn("sRoom != destination.room", warp)
        self.assertIn("SUSAMUNE_LM_WARP_ARRIVED, sDestination, sRoom", warp)
        self.assertIn("LmPlayerRoomId(word(p + 0xB4u))", tools)


if __name__ == "__main__":
    unittest.main()
