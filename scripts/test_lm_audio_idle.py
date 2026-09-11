"""Native JP audio drain contracts and bounded C postcondition tests."""

import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from test_lm_hud_state import AuthenticatedRetailTests

ROOT = Path(__file__).resolve().parents[1]


class AudioIdleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-audio-idle-")
        output = Path(cls.temp.name) / ("audio.dll" if os.name == "nt" else "audio.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror", "-I",
                   str(ROOT / "include"), str(ROOT / "scripts/lm_audio_idle_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        built = subprocess.run(command, capture_output=True, env=env)
        if built.returncode:
            raise RuntimeError(built.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]
        cls.lib.run.restype = ctypes.c_int
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(24 * 1024 * 1024)
        self.basic, self.heap = 0x803E3CF8, 0x805384C0
        self.data, self.sound = 0x8070FD80, 0x8074FF80
        self.se, self.seq, self.stream = 0x8074D260, 0x80779960, 0x8077BFE0
        for address, value in {
            0x804A0B94: self.heap, self.heap: 0x8038886C,
            self.heap + 0x30: 0x80538550, self.heap + 0x34: 0x80BE44C0,
            0x804A03A8: self.basic, 0x804A1DD0: self.basic,
            self.basic + 8: 0x80383FB0, self.basic: self.data,
            self.basic + 0x64: self.sound, self.sound: 2 << 24,
            self.sound + 8: 0x80000800, self.sound + 0x30: self.basic + 0x64,
            0x804A042C: 5, 0x804A0444: 3, 0x804A045C: 1,
            self.data + 0x1E8: self.se, self.data + 0x180: self.seq,
            self.data + 0x184: self.stream, self.data + 0x214: self.sound,
            self.seq + 2 * 0x4C + 0x44: self.sound,
        }.items():
            self.put(address, value)

    def put(self, address, value):
        struct.pack_into(">I", self.image, address - 0x80000000, value)

    def get(self, address):
        return struct.unpack_from(">I", self.image, address - 0x80000000)[0]

    def run_case(self, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        result = self.lib.run(data, len(data), fail)
        self.assertLess(self.lib.metric(0), 100)
        self.assertEqual(self.lib.metric(1), 0, "followed an invalid pointer")
        return result

    def refuses(self, address, values):
        old = self.get(address)
        for value in values:
            with self.subTest(address=hex(address), value=hex(value)):
                self.put(address, value)
                self.assertEqual(self.run_case(), 0)
        self.put(address, old)

    def test_normalized_audio_is_read_only(self):
        before = hashlib.sha256(self.image).digest()
        self.assertEqual(self.run_case(), 1)
        self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (0, 0))
        self.assertEqual(hashlib.sha256(self.image).digest(), before)

    def test_each_live_gameplay_owner_is_rejected(self):
        for address in [self.se + i * 12 + 4 for i in range(5)] + [
                self.seq + 0x44, self.seq + 0x4C + 0x44,
                self.stream + 0x10, self.data + 0x220,
                self.sound + 0x28, self.sound + 0x2C]:
            self.refuses(address, [0x81001234, 0xFFFFFFFF])

    def test_bootstrap_must_be_the_only_reciprocal_sequence(self):
        self.refuses(self.sound + 8, [0, 0x80000801])
        self.refuses(self.sound + 0x30, [0, 0x81000000])
        self.refuses(self.sound, [3 << 24, 255 << 24])
        self.refuses(self.data + 0x214, [0, self.sound + 0x40])
        self.refuses(self.seq + 2 * 0x4C + 0x44, [0, self.sound + 0x40])

    def test_bounded_counts(self):
        for address in (0x804A042C, 0x804A0444, 0x804A045C):
            self.refuses(address, [0, 17, 0x40000000, 0xFFFFFFFF])

    def test_pointer_bounds_checked_before_dereference(self):
        for address in (0x804A0B94, self.basic, self.basic + 0x64,
                        self.data + 0x180, self.data + 0x184, self.data + 0x1E8):
            self.refuses(address, [0, 3, 0x7FFFFFFC, 0x81800000, 0xFFFFFFFC])
        for address in (self.basic, self.basic + 0x64, self.data + 0x180,
                        self.data + 0x184, self.data + 0x1E8):
            self.refuses(address, [0x81000000, 0x80400000, 0x80BE44B0])

    def test_bad_sys_bounds_and_service_identity(self):
        self.refuses(self.heap, [0, 0x8038886D])
        self.refuses(self.heap + 0x30, [0, self.heap, 0x80BE44C0])
        self.refuses(self.heap + 0x34, [0, 0xFFFFFFFF, 0x81800004])
        for address in (0x804A03A8, 0x804A1DD0, self.basic + 8):
            self.refuses(address, [0, 0xFFFFFFFF])

    def test_live_free_lists_and_dead_handle_fields_are_not_followed(self):
        for address in (self.data + 0x210, self.data + 0x218,
                        self.data + 0x21C):
            self.put(address, 0xFFFFFFFF)
        for i in range(5):
            self.put(self.se + 12 * i, 0xFFFFFFFF)
            self.put(self.se + 12 * i + 8, 0xFFFFFFFF)
        self.assertEqual(self.run_case(), 1)

    def test_read_failure_and_null_callback(self):
        self.assertEqual(self.run_case(self.data + 0x1E8), 0)
        self.assertEqual(self.lib.metric(2), self.data + 0x1E8)
        self.assertEqual(self.lib.nullReader(), 0)

    def test_old_non_quiescent_mem1_fixtures_are_rejected(self):
        paths = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in (
            "diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]
        found = False
        for path in paths:
            if not path.exists():
                continue
            found = True
            self.image = bytearray(path.read_bytes())
            with self.subTest(fixture=path.parent.name):
                self.assertEqual(self.run_case(), 0)
        if not found:
            self.skipTest("Private pre-drain MEM1 fixtures unavailable")


class AudioNativeProofTests(AuthenticatedRetailTests):
    def test_scene_change_drains_all_three_native_owner_kinds(self):
        self.words({
            0x8018D510: 0x806301E8, 0x8018D514: 0x7C03002E,
            0x8018D520: 0x83A3002C, 0x8018D544: 0x3B9C000C,
            0x8018D54C: 0x800DF94C, 0x8018D570: 0x80630180,
            0x8018D580: 0x809E0064, 0x8018D584: 0x88040000,
            0x8018D5C0: 0x80630184, 0x8018D5F0: 0x901E0064,
        })
        self.call(0x8018D530, 0x8018C9EC)
        self.call(0x8018D594, 0x80194F2C)
        self.call(0x8018D5D4, 0x80194F2C)
        self.call(0x8018D62C, 0x8018C81C)

    def test_native_release_clears_registered_game_handle_slots(self):
        self.words({
            0x8018CAB4: 0x80A40030, 0x8018CABC: 0x90050000,
            0x8018CB88: 0x80630030, 0x8018CB94: 0x90030000,
            0x8018F1D0: 0x807D0030, 0x8018F1E0: 0x90030000,
            0x8018D0F8: 0x90650038, 0x8018D100: 0x90650030,
            0x8018D104: 0x80040004, 0x8018D134: 0x8005002C,
            0x8018D138: 0x90060000, 0x8018CBB4: 0x3884021C,
            0x8018F340: 0x38840210,
        })
        self.call(0x8018CB2C, 0x8018F040)
        self.call(0x8018CBB8, 0x8018D0F4)
        self.call(0x8018F210, 0x8018D0F4)
        self.call(0x8018F344, 0x8018D0F4)


if __name__ == "__main__":
    unittest.main()
