"""Executable timing tests and authenticated GLMJ01 observation anchors."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PracticeObservationTests(unittest.TestCase):
    def test_actual_timing_counter_implementation(self):
        compiler = ROOT / "toolchain/clang++.exe"
        if not compiler.exists():
            self.skipTest("Bundled Windows clang is required")
        with tempfile.TemporaryDirectory(prefix="lm-timing-") as directory:
            exe = Path(directory) / "timing.exe"
            result = subprocess.run([
                str(compiler), "--target=x86_64-pc-windows-msvc", "-fuse-ld=lld",
                "-nostdlib", "-fno-stack-protector", "-Wl,/entry:main,/subsystem:console",
                "-I", str(ROOT / "include"),
                str(ROOT / "scripts/test_lm_timing.cpp"), "-o", str(exe)
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_observation_anchors_match_retail_dol(self):
        path = ROOT / "build-lm-diag/clean_glmj_main.dol"
        if not path.exists():
            self.skipTest("Maintainer's clean DOL is not available")
        from dolreader.dol import DolFile
        with path.open("rb") as f:
            dol = DolFile(f)
        for address, expected in {
            0x801E2FC4: 0x806D16F8,  # VI getter -> r13 + 0x16f8
            0x801E2FC8: 0x4E800020,
            0x8000747C: 0x3C80803A,  # high half before SIGNED addi
            0x80007484: 0x38A48560,  # 803A0000 - 7AA0 = 80398560
            0x80007490: 0x80050004,  # retrace queue's expected interval
            0x80007494: 0x7C641850,
            0x80066A5C: 0xD0230044,  # xyz written by setPosition
            0x80066A64: 0xD05F0048,
            0x80066A68: 0xD07F004C,
            0x80066AB8: 0x907F00B4,  # resolved room ID
        }.items():
            dol.seek(address)
            self.assertEqual(int.from_bytes(dol.read(4), "big"), expected, hex(address))
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        self.assertIn("word(0x80398564u)", source)
        self.assertNotIn("word(0x803A8564u)", source)
        self.assertIn("Speed %lu.%02lu u/update (XZ)", source)

    def test_trainer_never_claims_engine_verified_success(self):
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        self.assertIn("REFERENCE, NOT TRICK RESULT", source)
        self.assertIn("not a trick-hit detector", source)
        self.assertIn("LMState::loadRevision()", source)
        self.assertIn("LMInputDisplay::draw(xfb, 640u, 480u, sPad)", source)
        input_source = (ROOT / "lm_diag/src/lm_input_display.cpp").read_text()
        self.assertIn("static_cast<s8>(raw.mStickX)", input_source)
        self.assertIn("0x7F800000u", source)

    def test_state_input_is_latched_before_menu_neutralization(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        capture = source.index("LMState::samplePad(statuses[0], !sOpen);")
        self.assertLess(capture, source.index("pad->mButton = 0u;", capture))
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertIn("LmStateSampleHotkeys(&sHotkeys, pad.mButton", state)
        self.assertIn("LmStateConsumeHotkey(&sHotkeys", state)
        self.assertNotIn("sPreviousButtons", state)

    def test_busy_reason_latches_the_failing_preflight(self):
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertEqual(state.count("if (!actionIdentity(&preflight))"), 2)
        preflight = state.split("bool actionIdentity(", 1)[1].split("void saveState()", 1)[0]
        self.assertIn("!buildIdentity(identity, true) || !ioIdle(true)", preflight)
        self.assertIn("Gate::Stability", preflight)
        self.assertIn("Gate::IdentityChanged", preflight)
        reject = state.split("void setReject(", 1)[1].split("void clearEpochMismatch", 1)[0]
        self.assertIn("sRejectedGate = sGate;", reject)
        self.assertIn("sRejectedGateValue = sGateValue;", reject)
        self.assertIn("sStatus == Status::Busy ? sRejectedGate : sGate", state)
        self.assertIn("sStatus == Status::Busy ? sRejectedGateValue : sGateValue", state)

    def test_immediate_action_gate_rechecks_current_identity_and_io(self):
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        immediate = state.split("bool readyForActionNow()", 1)[1].split("bool requestSave()", 1)[0]
        self.assertIn("if (!readyForAction()) return false;", immediate)
        self.assertIn("return actionIdentity(&live);", immediate)
        self.assertNotIn("++sStableFrames", immediate)

    def test_grain_source_is_checked_before_snapshot_or_game_writes(self):
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        save = state.split("void saveState()", 1)[1].split("void loadState()", 1)[0]
        self.assertLess(save.index("grainStateValid(nullptr, live"),
                        save.index("header->magic = 0u"))
        load = state.split("void loadState()", 1)[1].split('#include "lm_state_storage.inc"', 1)[0]
        self.assertLess(load.index("snapshotChecksum(header) != header->checksum"),
                        load.index("grainStateValid(header, preflight"))
        self.assertLess(load.index("grainStateValid(header, preflight"),
                        load.index("quiesceAudio(preflight)"))
        self.assertLess(load.index("restoreStaticRanges();"),
                        load.index("grainStateValid(nullptr, live"))
        self.assertLess(load.index("grainStateValid(nullptr, live"),
                        load.index("freezeEnd(freeze, true)"))
        post_copy = load.split("if (!grainStateValid(nullptr, live", 1)[1].split("traceLoadPhase(0x70u", 1)[0]
        self.assertIn("__builtin_trap();", post_copy)
        self.assertNotIn("return;", post_copy)
        reader = state.split("int grainReadWord(", 1)[1].split("bool grainStateValid", 1)[0]
        self.assertIn("if (!found) return 0;", reader)
        self.assertIn("sizeof(u32) > header->totalSize - offset", reader)


if __name__ == "__main__":
    unittest.main()
