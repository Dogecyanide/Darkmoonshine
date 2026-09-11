"""Execute the queued warp publication/rollback code with native test callbacks."""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WarpQueueNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-warp-queue-")
        output = Path(cls.temp.name) / ("queue.dll" if os.name == "nt" else "queue.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_warp_transition_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_ulong] * 5
        cls.lib.run.restype = ctypes.c_int
        for name in ("global_value", "flag_value", "seen_value", "event_value", "event_plan"):
            fn = getattr(cls.lib, name)
            fn.argtypes = [ctypes.c_ulong]
            fn.restype = ctypes.c_ulong
        cls.lib.plan_value.argtypes = [ctypes.c_ulong] * 3
        cls.lib.plan_value.restype = ctypes.c_ulong

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def test_exact_flag_edits_for_all_boss_destinations(self):
        for map_id, ids in ((10, [39, 46, 222]), (13, [67, 68, 222]),
                            (11, [81, 82, 222]), (9, [66, 222])):
            self.assertEqual(self.lib.plan_value(map_id, 0, 0), len(ids))
            for i, value in enumerate(ids):
                self.assertEqual(self.lib.plan_value(map_id, 0, i + 1), value)
                self.assertEqual(self.lib.plan_value(map_id, 0, i + 4), 0)

    def test_mansion_special_flags_match_existing_warp_behavior(self):
        for point, expected in ((0, None), (55, 34), (56, 46), (18, None)):
            self.assertEqual(self.lib.plan_value(2, point, 0), int(expected is not None))
            if expected is not None:
                self.assertEqual(self.lib.plan_value(2, point, 1), expected)
                self.assertEqual(self.lib.plan_value(2, point, 4), 1)

    def test_boss_intro_play_count_is_scoped_and_rolls_back(self):
        for map_id, event in ((9, 75), (10, 64), (11, 72), (13, 66), (2, None)):
            self.assertEqual(self.lib.event_plan(map_id), event if event else 0xFFFFFFFF)
            for accepted in (0, 1):
                self.assertEqual(self.lib.run(map_id, 0, accepted, 0, 0), accepted)
                for index in range(109):
                    expected = 0 if accepted and index == event else index + 1
                    self.assertEqual(self.lib.event_value(index), expected)

    def test_queue_observes_fully_published_scene2_request(self):
        self.assertEqual(self.lib.run(2, 18, 1, 0, 0), 1)
        self.assertEqual([self.lib.seen_value(i) for i in range(3)], [2, 2, 240])
        self.assertEqual([self.lib.global_value(i) for i in range(3)], [2, 240, 1])
        self.assertEqual(self.lib.seen_value(4), 1)
        self.assertEqual(self.lib.run(9, 0, 1, 0xFFFF, 0), 1)
        self.assertEqual([self.lib.seen_value(i) for i in range(4)], [2, 9, 0, 0])

    def test_refused_send_restores_all_globals_and_all_256_flags(self):
        for map_id, point in ((2, 18), (2, 55), (2, 56), (9, 0),
                              (10, 0), (11, 0), (13, 0)):
            for bits in (0, 0xFFFF, 0xCA35):
                with self.subTest(map=map_id, point=point, bits=bits):
                    self.assertEqual(self.lib.run(map_id, point, 0, bits, 0), 0)
                    self.assertEqual([self.lib.global_value(i) for i in range(3)], [13, 55, 7])
                    self.assertEqual([self.lib.flag_value(i) for i in range(256)],
                                     [(bits >> (i % 16)) & 1 for i in range(256)])
                    self.assertEqual(self.lib.seen_value(4), 1)

    def test_success_changes_only_the_planned_flag_bits(self):
        for map_id, point in ((2, 55), (2, 56), (9, 0), (10, 0), (11, 0), (13, 0)):
            for bits in (0, 0xFFFF, 0xCA35):
                expected = [(bits >> (i % 16)) & 1 for i in range(256)]
                for i in range(self.lib.plan_value(map_id, point, 0)):
                    expected[self.lib.plan_value(map_id, point, i + 1)] = self.lib.plan_value(map_id, point, i + 4)
                self.assertEqual(self.lib.run(map_id, point, 1, bits, 0), 1)
                self.assertEqual([self.lib.flag_value(i) for i in range(256)], expected)

    def test_oversized_flag_plan_is_rejected_before_side_effects(self):
        self.assertEqual(self.lib.run(9, 0, 1, 0xFFFF, 4), 0)
        self.assertEqual([self.lib.global_value(i) for i in range(3)], [13, 55, 7])
        self.assertEqual(self.lib.seen_value(4), 0)
        self.assertTrue(all(self.lib.flag_value(i) == 1 for i in range(256)))

    def test_runtime_uses_queue_not_direct_scene_poke(self):
        source = (ROOT / "lm_diag/src/lm_warp.cpp").read_text()
        self.assertNotIn("write(kScene,", source)
        self.assertIn("kQueueScene = 0x8000B20Cu", source)
        self.assertEqual(source.count("LMState::readyForActionNow()"), 3)
        reload_request = source.split("bool requestRoomReload(", 1)[1].split("void tick()", 1)[0]
        self.assertLess(reload_request.index("!LMState::readyForActionNow()"),
                        reload_request.index("if (!request(i, booSafe))"))
        self.assertIn("if (active()) { LMNotice::show(LM_POPUP_BUSY); return false; }",
                      reload_request)
        self.assertIn("sPhase == Phase::Queued || sPhase == Phase::Loading", source)
        self.assertIn("sPhase = Phase::Queued", source)
        stalled = source.split("if (sAge > kTimeoutFrames", 1)[1].split("if (!sPrepared", 1)[0]
        self.assertNotIn("fail(", stalled)
        self.assertNotIn("Phase::Idle", stalled)
        self.assertNotIn("write(", stalled)

    def test_lifecycle_phases_are_bounded_and_instrumented(self):
        source = (ROOT / "lm_diag/src/lm_warp.cpp").read_text()
        for phase in ("REQUEST", "ACCEPT", "DISPATCH", "APPEARANCE", "PREPARED",
                      "SETTLING", "ARRIVED", "REJECT"):
            self.assertIn("SUSAMUNE_LM_WARP_" + phase, source)
        self.assertIn("!sStallReported", source)


if __name__ == "__main__":
    unittest.main()
