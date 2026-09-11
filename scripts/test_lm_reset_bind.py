"""Reset shortcut recording, debounce, reserved buttons and safe dispatch."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ResetBindTests(unittest.TestCase):
    def test_executable_masks_recording_release_debounce_and_cancel(self):
        compiler = ROOT / "toolchain/clang++.exe"
        if not compiler.exists(): self.skipTest("Bundled compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="lm-reset-bind-") as directory:
            exe = Path(directory) / "reset.exe"
            result = subprocess.run([str(compiler), "--target=x86_64-pc-windows-msvc",
                "-fuse-ld=lld", "-nostdlib", "-fno-stack-protector", "-O2",
                "-Wl,/entry:main,/subsystem:console", "-I", str(ROOT / "include"),
                str(ROOT / "scripts/test_lm_reset_bind.cpp"), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_pad_hook_only_queues_dispatch_uses_existing_safe_reset(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        pad = source.split("void filterPadRead(", 1)[1].split("void tick()", 1)[0]
        self.assertIn("sResetPending = true", pad)
        self.assertNotIn("resetCurrentRoom(", pad)
        self.assertIn("!sOpen && !sConsumeUntilRelease", pad)
        self.assertIn("statuses[0].mCurError == 0", pad)
        reset = source.split("void resetCurrentRoom(", 1)[1].split("\n}", 1)[0]
        self.assertIn("readyForActionNow()", reset)
        self.assertIn("noEventRunning()", reset)
        self.assertIn("menuReady()", reset)
        self.assertIn("!shortcut && !confirm(6u)", reset)
        self.assertIn("LMWarp::requestRoomReload(false, sBooSafe)", reset)
        tick = source.split("void tick()", 1)[1].split("void draw(", 1)[0]
        self.assertIn("sResetPending = false;\n            resetCurrentRoom(true);", tick)
        self.assertIn("sResetRecorder.sample(sMenuPad.mButton", tick)
        self.assertIn("sResetBind = 0u", tick)

    def test_persisted_binding_validated_and_recording_explained(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        self.assertIn("v[47]=sResetBind", source)
        self.assertIn("LMResetBind::valid(v[47])", source)
        self.assertIn("RECORD RESET ROOM COMBO", source)
        self.assertIn("Release all buttons first.", source)
        self.assertIn("L/R: full click. B alone: cancel.", source)
        self.assertIn("A records; Z disables. Default OFF.", source)


if __name__ == "__main__": unittest.main()
