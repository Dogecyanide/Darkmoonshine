"""Retained-owner profiles: actual C capture/match, corruption and JP proof."""

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


class PersistentProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-persistent-profile-")
        output = Path(cls.temp.name) / ("profile.dll" if os.name == "nt" else "profile.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror", "-I",
                   str(ROOT / "include"), str(ROOT / "scripts/lm_persistent_profile_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        built = subprocess.run(command, capture_output=True, env=env)
        if built.returncode:
            raise RuntimeError(built.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.capture.argtypes = [ctypes.c_void_p] + [ctypes.c_uint] * 6
        cls.lib.capture.restype = ctypes.c_int
        for name in ("word", "metric", "valid"):
            getattr(cls.lib, name).argtypes = [ctypes.c_uint]
            getattr(cls.lib, name).restype = ctypes.c_uint
        cls.lib.mutate.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_int]

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(24 * 1024 * 1024)
        self.root, self.system = 0x80530000, 0x80530100
        self.mode, self.fifo_obj = 0x8039858C, 0x80496084
        root_node = self.root + 0x90
        for address, value in {
            self.root + 0x30: root_node, self.root + 0x34: 0x817FB140,
            self.root + 0x7C: root_node, self.root + 0x80: root_node,
            root_node: 0x484D0000, root_node + 4: 0x6B0000,
            self.system: 0x8038886C,
            self.system + 0x30: 0x80531000, self.system + 0x34: 0x80BE44C0,
        }.items():
            self.put(address, value)
        node, nodes = 0x80531000, []
        for size in (0x98, 0x80000, 0x96000, 0x96000, 0xC0000):
            nodes.append((node, size))
            node = (node + 16 + size + 31) & ~31
        self.pad, self.fifo, self.xfb0, self.xfb1, self.audio = [p + 16 for p, _ in nodes]
        for i, (node, size) in enumerate(nodes):
            for j, value in enumerate((0x484D0001, size,
                                      nodes[i - 1][0] if i else 0,
                                      nodes[i + 1][0] if i + 1 < len(nodes) else 0)):
                self.put(node + 4 * j, value)
        self.put(self.system + 0x7C, nodes[0][0])
        self.put(self.system + 0x80, nodes[-1][0])
        for address, value in {
            0x804A0BA0: self.fifo, 0x804A0BA4: self.fifo_obj,
            0x804A0BBC: self.xfb0, 0x804A0BC0: self.xfb1,
            0x804A0BCC: self.xfb0, 0x804A0BD0: self.xfb1,
            0x804A0BD4: self.mode, 0x804A0BF8: self.pad,
            self.fifo_obj: self.fifo, self.fifo_obj + 4: self.fifo + 0x80000 - 4,
            self.fifo_obj + 8: 0x80000,
            self.pad: 0x8038925C, self.pad + 4: self.system,
            0x803E3CF8: self.audio,
            self.audio + 0x180: self.audio + 0x1000,
            self.audio + 0x184: self.audio + 0x2000,
            self.audio + 0x1E8: self.audio + 0x3000,
            0x804A042C: 5, 0x804A0444: 3, 0x804A045C: 1,
            0x803E3D04: 0x80398780, 0x803E3D08: 0, 0x803E3D0C: 0x8039890C,
        }.items():
            self.put(address, value)
        for i in range(5):
            self.put(0x803C8428 + i * 20, 0xF00000 + i * 0x10000)
            self.put(0x803C842C + i * 20, 0x10000)

    def put(self, address, value):
        struct.pack_into(">I", self.image, address - 0x80000000, value)

    def get(self, address):
        return struct.unpack_from(">I", self.image, address - 0x80000000)[0]

    def capture(self, config=1, generation=17, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        result = self.lib.capture(data, len(data), self.root, self.system,
                                  config, generation, fail)
        self.assertLess(self.lib.metric(0), 400)
        self.assertEqual(self.lib.metric(1), 0)
        return result

    def test_capture_is_bounded_read_only_and_stable(self):
        before = hashlib.sha256(self.image).digest()
        self.assertEqual(self.capture(), 1)
        self.assertEqual(self.lib.valid(17), 1)
        self.lib.keep()
        self.assertEqual(self.capture(), 1)
        self.assertEqual(self.lib.matches(), 1)
        self.assertEqual(hashlib.sha256(self.image).digest(), before)
        self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (0, 0))

    def test_generation_config_and_payload_are_bound(self):
        self.assertEqual(self.capture(), 1)
        self.assertEqual(self.lib.valid(18), 0)
        self.lib.keep()
        self.assertEqual(self.capture(config=2), 1)
        self.assertEqual(self.lib.matches(), 0)
        self.assertEqual(self.lib.metric(4), 4)
        for word in (0, 1, 2, 3, 7, 20, 100, 359):
            self.assertEqual(self.capture(), 1)
            self.lib.mutate(word, self.lib.word(word) ^ 1, 0)
            self.assertEqual(self.lib.valid(17), 0)

    def test_retained_allocation_change_refuses_match(self):
        self.assertEqual(self.capture(), 1)
        self.lib.keep()
        self.put(self.pad - 16, 0x484D0005)
        self.assertEqual(self.capture(), 1)
        self.assertEqual(self.lib.matches(), 0)
        self.assertGreater(self.lib.metric(4), 70)

    def test_native_input_idle_policy(self):
        for address in (self.pad + 0x68, self.pad + 0x78, self.pad + 0x7C):
            self.put(address, 1)
            self.assertEqual(self.capture(), 0)
            self.assertTrue(all(self.lib.word(i) == 0 for i in range(360)))
            self.put(address, 0)
        self.put(self.pad + 0x6C, 0xDEADBEEF)
        self.assertEqual(self.capture(), 1, "inactive rumble does not read its stale pattern")

    def test_inflight_aram_refuses_but_completed_command_is_not_dereferenced(self):
        for i in range(5):
            base = 0x803C8428 + 20 * i
            self.put(base + 8, 0x81000000)
            self.assertEqual(self.capture(), 0)
            self.put(base + 8, 0)
            self.put(base + 16, 0xDEADBEEF)
        self.assertEqual(self.capture(), 1)

    def test_dynamic_flip_fifo_cursors_and_buttons_do_not_change_profile(self):
        self.assertEqual(self.capture(), 1)
        self.lib.keep()
        for address, value in ((0x804A0BC4, self.xfb1), (0x804A0BC8, self.xfb0),
                               (self.fifo_obj + 0x14, self.fifo + 0x2000),
                               (self.pad + 0x18, 0x106), (self.pad + 0x74, 0xFF)):
            self.put(address, value)
        self.assertEqual(self.capture(), 1)
        self.assertEqual(self.lib.matches(), 1)

    def test_audio_camera_must_target_captured_fixed_renderer(self):
        for address in (0x803E3D04, 0x803E3D08, 0x803E3D0C):
            old = self.get(address)
            for value in (0x81000000, 0x8074D100, 0xFFFFFFFF):
                self.put(address, value)
                self.assertEqual(self.capture(), 0)
                self.assertEqual(self.lib.metric(2), address)
            self.put(address, old)

    def test_invalid_pointer_links_fail_before_foreign_reads(self):
        for address in (self.system + 0x7C, self.pad - 4, 0x804A0BF8,
                        0x804A0BD4, 0x804A0BA4):
            old = self.get(address)
            for bad in (3, 0x7FFFFFFC, 0x81800000, 0xFFFFFFFC):
                self.put(address, bad)
                self.assertEqual(self.capture(), 0)
            self.put(address, old)
        self.put(self.pad - 4, self.pad - 16)
        self.assertEqual(self.capture(), 0, "cycle must fail bounded traversal")

    def test_read_failure_clears_candidate(self):
        self.assertEqual(self.capture(fail=0x804A0BF8), 0)
        self.assertEqual(self.lib.metric(2), 0x804A0BF8)
        self.assertTrue(all(self.lib.word(i) == 0 for i in range(360)))

    def test_actual_old_mem1_profiles_match_across_room_changes(self):
        paths = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in (
            "diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]
        if not all(path.exists() for path in paths):
            self.skipTest("Private MEM1 fixtures unavailable")
        self.root, self.system = 0x80538420, 0x805384C0
        for i, path in enumerate(paths):
            self.image = bytearray(path.read_bytes())
            with self.subTest(fixture=path.parent.name):
                self.assertEqual(self.capture(), 1,
                    f"fault {self.lib.metric(2):08X} value {self.lib.metric(3):08X}")
                self.assertEqual(self.lib.word(7), 39)
                if not i:
                    self.lib.keep()
                else:
                    self.assertEqual(self.lib.matches(), 1)


class PersistentNativeProofTests(AuthenticatedRetailTests):
    def test_pad_is_retained_sys_owner_with_word_length_and_optional_callbacks(self):
        self.words({0x80005854: 0x38600098, 0x80005870: 0x93ED0118,
                    0x801D1ED0: 0x901E0000, 0x801D1F0C: 0xB01E0074,
                    0x801D1F2C: 0x909E0078, 0x801D1F30: 0x909E007C,
                    0x801D2A3C: 0x801F0004, 0x801D2A40: 0x28000000,
                    0x801D2A44: 0x41820158, 0x801D2AE0: 0x807F0008,
                    0x800ACBB0: 0x901E0794})
        self.call(0x8000586C, 0x801D1E9C)

    def test_aram_command_is_destroyed_but_generic_descriptor_pointer_is_stale(self):
        self.words({0x8006615C: 0x907E0008, 0x80066184: 0x907E0010,
                    0x800661C0: 0x80630010, 0x800661F8: 0x901E0008,
                    0x80066220: 0x807E0010, 0x80066224: 0x38800001,
                    0x8006622C: 0x80010024,
                    0x800662D0: 0x901E0008, 0x800662F8: 0x807E0010,
                    0x8011FF84: 0x93FC0000})
        self.call(0x80066180, 0x801CBE40)
        self.call(0x800661C4, 0x801CC138)
        self.call(0x80066228, 0x801CCD90)
        self.call(0x80066300, 0x801CCD90)

    def test_fifo_and_framebuffer_boot_geometry(self):
        self.words({0x80005ED4: 0x3C800008, 0x80005EEC: 0x906D00C0,
                    0x80005EFC: 0x906D00C4, 0x80007390: 0x3BA36000,
                    0x800073AC: 0x906D00DC, 0x800073EC: 0x906D00E0,
                    0x80007428: 0x900D00EC, 0x80007430: 0x90040004,
                    0x8000743C: 0x900D00F4,
                    0x801ED5B0: 0x3805FFFC, 0x801ED5C0: 0x7C1F0214,
                    0x801ED5D0: 0x93E30000, 0x801ED5D4: 0x90030004,
                    0x801ED5DC: 0x90A30008})
        self.call(0x80005EF8, 0x801EC52C)
        self.call(0x801EC5BC, 0x801ED5A8)

    def test_audio_camera_bind_is_fixed_renderer_not_game_heap(self):
        self.words({0x80009378: 0x3C80803A, 0x80009384: 0x3BE48780,
                    0x800093AC: 0x387F0000, 0x800093B0: 0x389F018C,
                    0x80186834: 0x38C40000, 0x80186840: 0x7C040378,
                    0x80186848: 0x38A00000, 0x8018684C: 0x38E00000,
                    0x80186850: 0x806DF8C8,
                    0x8018B3C0: 0x9083000C, 0x8018B3C4: 0x90A30010,
                    0x8018B3C8: 0x90C30014})
        self.call(0x800093B4, 0x80186830)
        self.call(0x80186854, 0x8018B3B8)


if __name__ == "__main__":
    unittest.main()
