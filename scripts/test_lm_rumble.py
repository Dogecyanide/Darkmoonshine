"""Bounded motor reconciliation and authenticated GLMJ01/Nintendont paths."""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_lm_hud_state import AuthenticatedRetailTests

ROOT = Path(__file__).resolve().parents[1]
PAD, MANAGER, CONTROLLER = 0x80500100, 0x81000100, 0x81000200


@unittest.skipUnless(os.name == "nt", "Windows native harness")
class RumbleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-rumble-")
        obj, dll = (Path(cls.temp.name) / name for name in ("rumble.obj", "rumble.dll"))
        result = subprocess.run([str(ROOT / "toolchain/clang.exe"),
            "--target=x86_64-pc-windows-msvc", "-O2", "-ffreestanding", "-fno-stack-protector",
            "-I", str(ROOT / "include"), "-c", str(ROOT / "scripts/lm_rumble_harness.c"),
            "-o", str(obj)], capture_output=True, text=True)
        if result.returncode: raise RuntimeError(result.stderr)
        result = subprocess.run([str(ROOT / "toolchain/lld-link.exe"), "/dll", "/noentry",
            "/nodefaultlib", f"/out:{dll}", str(obj)], capture_output=True, text=True)
        if result.returncode: raise RuntimeError(result.stderr)
        cls.lib = ctypes.CDLL(str(dll))
        for name, count in (("run", 1), ("word", 1), ("poke", 2), ("metric", 1), ("future_event", 1)):
            getattr(cls.lib, name).argtypes = [ctypes.c_uint] * count
            getattr(cls.lib, name).restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(cls.lib._handle))
        cls.temp.cleanup()

    def setUp(self): self.lib.setup()
    def motors_stopped(self):
        self.assertEqual([self.lib.metric(i) for i in range(5)], [2, 2, 2, 2, 4])
        self.assertEqual(self.lib.word(0x804A2064), 0)
        self.assertEqual(self.lib.metric(7), 0)

    def test_quiet_saved_latches_still_stop_live_motor(self):
        self.lib.poke(CONTROLLER + 4, 0x1234)
        self.lib.run(1)
        self.motors_stopped()
        self.assertEqual(self.lib.metric(5), 4)
        self.assertEqual(self.lib.word(CONTROLLER + 4), 0x1234)

    def test_restored_active_patterns_cancel_and_future_rumble_works(self):
        self.lib.run(1)
        self.motors_stopped()
        for controller in range(CONTROLLER, CONTROLLER + 0x400, 0x100):
            self.assertEqual(self.lib.word(controller + 4) >> 16, 0)
            for i in range(4):
                self.assertEqual(self.lib.word(controller + 8 + i * 0x18) >> 24, 0)
                self.assertEqual(self.lib.word(controller + 0x68 + i * 0x1C) >> 24, 0)
        self.assertEqual([self.lib.word(PAD + 0x64 + i * 4) for i in range(4)], [0] * 4)
        self.lib.future_event(0)
        self.assertEqual(self.lib.metric(4), 4)
        self.lib.future_event(1)
        self.assertEqual(self.lib.metric(0), 1)
        self.lib.future_event(0)
        self.assertEqual(self.lib.metric(0), 2)

    def test_connection_enable_input_and_other_pad_data_remain_live(self):
        before = [self.lib.word(PAD + i) for i in range(0, 0x98, 4)]
        self.lib.run(1)
        after = [self.lib.word(PAD + i) for i in range(0, 0x98, 4)]
        self.assertEqual(before[:25], after[:25])
        self.assertEqual(before[29:], after[29:])
        self.assertEqual(self.lib.word(0x804A2068), 0xA0000000)

    def test_disabled_rumble_stays_disabled_but_hard_stop_still_sent(self):
        self.lib.poke(0x804A2068, 0)
        self.lib.run(1)
        self.lib.future_event(1)
        self.motors_stopped()
        self.assertEqual(self.lib.word(0x804A2068), 0)

    def test_disconnected_pad_keeps_connection_status_and_reconciles(self):
        self.lib.poke(PAD + 0x74, 0xFFFFFECA)
        self.lib.run(1)
        self.motors_stopped()
        self.assertEqual(self.lib.word(PAD + 0x74), 0xFFFFFECA)
        self.assertEqual(self.lib.metric(5), 4)

    def test_refused_load_makes_no_mutations_or_motor_calls(self):
        self.lib.run(0)
        self.assertEqual([self.lib.metric(i) for i in range(8)], [1, 1, 1, 1, 0, 0, 0, 0])
        self.assertEqual(self.lib.word(CONTROLLER + 4), 0x11111111)

    def test_invalid_manager_controller_or_overlap_never_resets_unknown_object(self):
        cases = [(0x804A1758, 0), (MANAGER - 16, 0), (MANAGER - 12, 0x40),
                 (MANAGER + 4, 0x81003F80), (CONTROLLER - 12, 0xD4),
                 (CONTROLLER, PAD + 4), (MANAGER + 8, CONTROLLER),
                 (MANAGER + 8, MANAGER), (MANAGER + 4, 0xFFFFFFFF)]
        for a, value in cases:
            with self.subTest(address=hex(a), value=hex(value)):
                self.lib.setup(); self.lib.poke(a, value); self.lib.run(1)
                self.motors_stopped()
                self.assertEqual(self.lib.metric(5), 0)

    def test_invalid_or_truncated_pad_never_writes_inside_it(self):
        for a, value in ((0x804A0BF8, 0x80500F80), (PAD - 12, 0x94),
                         (PAD, 0), (PAD + 4, 0), (PAD + 0x74, 0x0004FECA)):
            with self.subTest(address=hex(a)):
                self.lib.setup(); self.lib.poke(a, value); self.lib.run(1)
                self.motors_stopped()
                self.assertEqual(self.lib.metric(5), 0)
                self.assertEqual(self.lib.metric(6), 1)

    def test_success_only_integration_after_heap_proof(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertEqual(source.count("LMRumble::afterLoad("), 1)
        tail = source[source.index("if (!healthyAfter)"):]
        self.assertLess(tail.index("__builtin_trap()"), tail.index("LMRumble::afterLoad("))
        self.assertLess(tail.index("LMRumble::afterLoad("), tail.index("++sLoadRevision"))
        helper = (ROOT / "lm_diag/src/lm_rumble.cpp").read_text()
        self.assertNotIn("93003010", helper)
        self.assertNotIn("PADReset", helper)


class NativeRumbleTests(AuthenticatedRetailTests):
    def test_controller_allocation_and_single_live_pad_binding(self):
        self.words({0x80070EC8: 0x38600044, 0x80070EEC: 0x93ED0C78,
            0x800710CC: 0x386000D8, 0x800710E0: 0x38800000,
            0x800710EC: 0x979F0004, 0x800710F0: 0x2C1E0004,
            0x80071438: 0x800D0118, 0x80071450: 0x38000000,
            0x80071468: 0x807D0004, 0x8007146C: 0x80810028})
        self.call(0x80070ED8, 0x801C9308)
        self.call(0x800710D0, 0x801C9308)
        self.call(0x80071470, 0x800852CC)

    def test_native_reset_clears_both_latches_and_all_eight_wave_slots(self):
        self.words({0x800852CC: 0x90830000, 0x800852D8: 0x98030004,
            0x800852E4: 0x98030005, 0x800852F0: 0x98050000,
            0x80085308: 0x98040008, 0x80085320: 0x98040020,
            0x80085338: 0x98040038, 0x80085350: 0x98060000,
            0x8008535C: 0x98030068, 0x80085368: 0x98030084,
            0x80085374: 0x980300A0, 0x8008537C: 0x900300A8,
            0x80085380: 0x4E800020})
        for address in range(0x800852CC, 0x80085384, 4):
            self.assertNotEqual(self.word(address) & 0xFC000003, 0x48000001)

    def test_game_edge_latches_explain_missing_stop_and_allow_new_start(self):
        self.words({0x800856A0: 0x881E0004, 0x800856A8: 0x40820020,
            0x800856B8: 0xA8630074, 0x800856C4: 0x981E0004,
            0x80085720: 0x881E0005, 0x80085728: 0x41820024,
            0x80085744: 0x981E0005})
        self.call(0x800856BC, 0x801D28CC)
        self.call(0x8008573C, 0x801D298C)

    def test_native_jut_layout_and_hardware_command(self):
        self.words({0x80005854: 0x38600098, 0x80005870: 0x93ED0118,
            0x801CB490: 0x907E0004,
            0x801D1EC8: 0x3C608039, 0x801D1ECC: 0x3803925C,
            0x801D1ED0: 0x901E0000, 0x801D1EF0: 0x387E0064,
            0x801D1F0C: 0xB01E0074, 0x801D29CC: 0x38000000,
            0x801D29A8: 0x80AD1588, 0x801D29C4: 0x38800002,
            0x801D29D0: 0x386D1584, 0x801D29D4: 0x7C03F9AE,
            0x801E4D08: 0x3C808000,
            0x801E4D54: 0x281D0002, 0x801E4D64: 0x57A007BE,
            0x801E4D6C: 0x64840040})
        self.call(0x801D29C8, 0x801E4CE4)
        self.call(0x80005858, 0x801C9308)
        self.call(0x8000586C, 0x801D1E9C)
        self.call(0x801D1EC0, 0x801CB450)
        self.call(0x801CB48C, 0x801C8FC4)
        self.assertEqual(0x804A0AE0 + 0x1584, 0x804A2064)
        self.assertEqual(self.data(0x80389228, 16), bytes.fromhex(
            "80000000 40000000 20000000 10000000"))
        self.call(0x801E4D74, 0x801DA924)
        self.call(0x801E4D78, 0x801DA938)

    def test_nintendont_replaces_same_entry_with_bounded_motor_mailbox(self):
        patch = (ROOT / "launcher/kernel/Patch.c").read_text()
        section = patch.split("case FCODE_PADControlMotor:", 1)[1].split("case FCODE_PADIsBarrel:", 1)[0]
        self.assertIn("DisableSIPatch", section)
        self.assertIn("PADControlMotor_size", section)
        asm = (ROOT / "launcher/kernel/asm/PADControlMotor.S").read_text()
        self.assertIn("PadRumble@h", asm)
        self.assertRegex(asm, r"cmpwi\s+%r3,\s+3")
        self.assertRegex(asm, r"stwx\s+%r4,\s+%r3,\s+%r0")


if __name__ == "__main__": unittest.main()
