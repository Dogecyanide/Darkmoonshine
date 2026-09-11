"""Native door-state predicate and authenticated Japanese retail field proof."""

import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DoorStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-door-")
        output = Path(cls.temp.name) / ("door.dll" if os.name == "nt" else "door.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_door_state_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        subprocess.run(command, check=True, capture_output=True, env=env)
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.busy.argtypes = [ctypes.c_uint] * 2
        cls.lib.busy.restype = ctypes.c_int
        cls.lib.validRange.argtypes = [ctypes.c_uint] * 4
        cls.lib.validRange.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def test_only_native_door_state_is_busy(self):
        for mode in range(4):
            for state in range(0x40):
                self.assertEqual(self.lib.busy(mode, state), int(mode == 2 and state == 0x20))

    def test_door_commands_are_not_stored_state_ids(self):
        for command in (0x0C, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19):
            self.assertEqual(self.lib.busy(2, command), 0)
        self.assertEqual(self.lib.busy(1, 0x20), 0)
        self.assertEqual(self.lib.busy(2, 0x20), 1)

    def test_range_guards_include_last_word_without_overflow(self):
        start, end = 0x80BE4560, 0x817FB140
        self.assertEqual(self.lib.validRange(end - 0x318, 0x318, start, end), 1)
        for address, size in ((end - 0x314, 0x318), (0, 4), (0xFFFFFFFF, 4),
                              (0xCC000000, 4), (0x90000000, 4),
                              (start + 1, 4), (start, 0xFFFFFFFF), (start, 0)):
            self.assertEqual(self.lib.validRange(address, size, start, end), 0)
        self.assertEqual(self.lib.validRange(start, 4, 0, 0xFFFFFFFF), 0)

    def test_real_capture_player_chain_is_stable_not_door_busy(self):
        path = ROOT / "build-lm-emu/diagnostic-capture-0.3.30/mem1.bin"
        if not path.exists():
            self.skipTest("Optional real MEM1 fixture unavailable")
        raw = path.read_bytes()
        word = lambda address: struct.unpack_from(">I", raw, address - 0x80000000)[0]
        heap = word(0x804A0B98)
        start, end = word(heap + 0x30), word(heap + 0x34)
        mission = word(0x804A17C8)
        self.assertEqual(self.lib.validRange(mission, 12, start, end), 1)
        manager = word(mission + 8)
        self.assertEqual(self.lib.validRange(manager, 0xE0C, start, end), 1)
        index = word(manager + 0xE08)
        self.assertLess(index, 128)
        player = word(0x803C8490 + index * 4)
        self.assertEqual(self.lib.validRange(player, 0x7E8, start, end), 1)
        self.assertEqual(word(player), 0x8034EE50)
        owner = word(player + 0x7E4)
        self.assertEqual(self.lib.validRange(owner, 0x318, start, end), 1)
        self.assertEqual(word(owner + 0x44), player)
        self.assertEqual(self.lib.busy(word(owner + 0x310), word(owner + 0x314)), 0)


class DoorRetailProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
        if not path.exists():
            raise unittest.SkipTest("Clean GLMJ01 DOL unavailable")
        cls.raw = path.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise AssertionError("Wrong retail DOL")
        cls.sections = []
        for count, offbase, addrbase, sizebase in ((7, 0, 0x48, 0x90), (11, 0x1C, 0x64, 0xAC)):
            for i in range(count):
                values = [struct.unpack_from(">I", cls.raw, base + i * 4)[0]
                          for base in (offbase, addrbase, sizebase)]
                if values[2]:
                    cls.sections.append(values)

    def word(self, address):
        for off, start, size in self.sections:
            if start <= address and address + 4 <= start + size:
                return struct.unpack_from(">I", self.raw, off + address - start)[0]
        self.fail(f"Unmapped address {address:08X}")

    def check(self, values):
        for address, expected in values.items():
            self.assertEqual(self.word(address), expected, f"Retail site {address:08X}")

    def test_player_getter_chain_and_actual_door_predicate(self):
        self.check({0x800E7E8C: 0x80AD0CE8, 0x800E7E90: 0x80650008,
                    0x800E3D9C: 0x1C04001C, 0x800E3DA8: 0x80630E08,
                    0x80067EC8: 0x5463103A, 0x80067ECC: 0x38048490,
                    0x80067ED4: 0x80630000, 0x800AD8B8: 0x807F07E4,
                    0x800AD8BC: 0x80030310, 0x800AD8C0: 0x2C000002,
                    0x800AD8C8: 0x80030314, 0x800AD8CC: 0x2C000020})

    def test_exact_player_vtable_has_zero_getter_pointer_bias(self):
        self.check({0x800ABF50: 0x90610008, 0x800ABF54: 0x80610008,
                    0x800ABF5C: 0x83A10008, 0x800ABF60: 0x3C608035,
                    0x800ABF64: 0x3803EE50, 0x800ABF68: 0x901D0000,
                    0x8034EE50: 0x8049B844, 0x8034EE54: 0,
                    0x8049B844: 0x8049B824, 0x8049C9A8: 0x8049C980,
                    0x8049B824: 0x506C6179, 0x8049B828: 0x65720000,
                    0x8049C980: 0x506C6179, 0x8049C984: 0x65720000,
                    0x801F5928: 0x80040004, 0x801F5930: 0x7C630214,
                    0x801F5984: 0x2C050000, 0x801F5988: 0x40820008,
                    0x801F598C: 0x480001BC})

    def test_door_command_dispatches_to_state_20_with_substate_one(self):
        self.check({0x80346BE8 + 0x0C * 4: 0x80072F98,
                    0x80072F98: 0x38000001, 0x80072F9C: 0x90010018,
                    0x80072FAC: 0x38800020, 0x80073DD0: 0x38000002,
                    0x80073DE8: 0x90030310, 0x80073DF4: 0x93E30314,
                    0x80073F0C: 0x80050000, 0x80073F10: 0x901E031C})

    def test_completed_actions_clear_state_but_intermediate_door_reenters(self):
        self.check({0x80074310: 0x38A00001, 0x80074314: 0x38800000,
                    0x80074318: 0x90BF0310, 0x80074324: 0x909F0314,
                    0x80346DA4 + (0x20 - 4) * 4: 0x80074548,
                    0x8007458C: 0x38800020, 0x800745C4: 0x38800020,
                    0x800745F8: 0x38800020, 0x80072C10: 0x38000001,
                    0x80072C14: 0x901D0310, 0x80072C20: 0x901D0314})


class DoorIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()

    def test_bounded_lookup_checks_before_each_dependent_read(self):
        body = self.source.split("bool doorTransitionReady(", 1)[1].split(
            "bool buildIdentity(", 1)[0]
        order = ["inside(identity.missionMode, 0xCu)",
                 "readWord(identity.missionMode + 8u)",
                 "inside(manager, 0xE0Cu)", "readWord(manager + 0xE08u)",
                 "index >= count", "readWord(kRoomActorTableStart + index",
                 "inside(player, LM_DOOR_CONTROLLER_OFFSET + sizeof(u32))",
                 "readWord(player)", "readWord(player + LM_DOOR_CONTROLLER_OFFSET)",
                 "inside(controller, 0x320u)",
                 "readWord(controller + LM_DOOR_MODE_OFFSET)",
                 "readWord(controller + 0x31Cu)"]
        previous = -1
        for token in order:
            current = body.index(token)
            self.assertGreater(current, previous, token)
            previous = current
        self.assertIn("readWord(player) != kPlayerVtable", body)
        self.assertIn("constexpr u32 kPlayerVtable = 0x8034EE50u;", self.source)
        self.assertNotIn("reinterpret_cast", body)

    def test_live_checks_run_again_frozen_before_copy(self):
        build = self.source.split("bool buildIdentity(", 1)[1].split(
            "if (identity->mainLoopMode", 1)[0]
        self.assertLess(build.index("roomActorCount > kRoomActorCapacity"),
                        build.index("doorTransitionReady(*identity, report)"))
        for function, write in (("void saveState()", "header->magic = 0u;"),
                                ("void loadState()", "copyWords(reinterpret_cast<void *>(live.heapStart)")):
            # Select the production save/load's closest preceding freeze.
            position = self.source.index(write, self.source.index(function))
            freeze = self.source.rfind("const FreezeState freeze = freezeBegin();", 0, position)
            self.assertGreaterEqual(freeze, 0)
            guard = self.source.find("buildIdentity(&live, true)", freeze, position)
            self.assertGreater(guard, freeze)
            self.assertIn("freezeEnd(freeze)", self.source[guard:position])


if __name__ == "__main__":
    unittest.main()
