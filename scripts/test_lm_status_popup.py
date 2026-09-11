"""Heapless transient notices and the completed-presenter paint boundary."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class StatusPopupTests(unittest.TestCase):
    def test_native_host_lifetime_repeat_transitions_and_expiry(self):
        compiler = ROOT / "toolchain/clang++.exe"
        if not compiler.exists(): self.skipTest("Bundled compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="lm-notice-") as directory:
            exe = Path(directory) / "notice.exe"
            result = subprocess.run([str(compiler), "--target=x86_64-pc-windows-msvc",
                "-fuse-ld=lld", "-nostdlib", "-fno-stack-protector",
                "-Wl,/entry:main,/subsystem:console", "-I", str(ROOT / "include"),
                str(ROOT / "scripts/test_lm_status_popup.cpp"), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_only_current_complete_presenter_can_paint_before_bulk_copy(self):
        source = (ROOT / "lm_diag/src/lm_diag.cpp").read_text()
        wrapper = source.split('extern "C" void diagnosticChangeFrameBuffer()', 1)[1].split(
            'extern "C" void diagnosticFrameBegin()', 1)[0]
        self.assertLess(wrapper.index("sPopupSurfaceReady = false"), wrapper.index("kLMChangeFrameBufferAddr"))
        self.assertEqual(wrapper.count("sPopupSurfaceReady = false"), 2)
        self.assertLess(wrapper.index("LMState::tick"), wrapper.rindex("sPopupSurfaceReady = false"))
        self.assertEqual(wrapper.count("LMNotice::tick()"), 1)
        copy = source.split('extern "C" void diagnosticCopyDisp(', 1)[1]
        self.assertLess(copy.index("kGXDrawDoneAddr"), copy.index("sPopupSurfaceReady = true"))
        self.assertLess(copy.index("if (validXfb)"), copy.index("sPopupXfb = cachedXfb"))
        # Menus and text overlays need this even when the popup is hidden.
        ready = copy.split("if (directPrintReady)", 1)[1]
        self.assertLess(ready.index("kDirectPrintChangeFrameBufferAddr"), ready.index("LMPractice::draw"))
        self.assertLess(ready.index("kDirectPrintChangeFrameBufferAddr"), ready.index("LMTools::draw"))
        self.assertIn("!LMPractice::isOpen() && readByte(kLMDoubleBufferAddr) == 1u", ready)
        self.assertIn("const u32 kLMDoubleBufferAddr = 0x804A0BBAu;", source)
        self.assertNotIn("writeWord(kLMDoubleBufferAddr", source)
        present = source.split("void LMNotice::present()", 1)[1].split("extern", 1)[0]
        self.assertIn("if (!sPopupSurfaceReady) return", present)
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        tick = state.split("void tick(bool allowRequests)", 1)[1].split("Status status()", 1)[0]
        self.assertIn("LMNotice::present();\n        saveState();", tick)
        self.assertIn("LMNotice::present();\n        loadState();", tick)

    def test_compact_inset_no_permanent_panel_and_diagnostics_preserved(self):
        source = (ROOT / "lm_diag/src/lm_diag.cpp").read_text()
        self.assertIn("kPopupLeft = 12u, kPopupTop = 16u", source)
        self.assertIn("kPopupWidth = 56u, kPopupHeight = 11u", source)
        self.assertIn("if (!*message || LMPractice::isOpen()) return", source)
        self.assertIn("kPopupHeight * 2u * kXfbRowBytes", source)
        self.assertLessEqual((16 + 11) * 2 * 640 * 2, 640 * 480 * 2)
        for token in ("LM STATE X", "drawPanel", "drawRawHeartbeat", "showModel"):
            self.assertNotIn(token, source)
        for token in ("sampleFloorAndCanary();", "sampleHeapChecks(*system, *game);",
                      "LMCrash::init();", "LMState::presenterAfterDrawDone();",
                      "LMCrash::note(0x130u", "LMCrash::note(0x131u",
                      "if (!systemOk || !gameOk) __builtin_trap();"):
            self.assertIn(token, source)
        self.assertLess(source.index("LMCrash::init();"), source.index("LMCrash::note(0x130u"))
        self.assertLess(source.index("LMCrash::init();"), source.index("LMCrash::note(0x131u"))
        self.assertIn("!sFloorFaultReported", source)
        self.assertIn("!sCanaryFaultReported", source)

    def test_events_not_passive_busy_gate_keep_same_attempts_visible(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        gate = source.split("bool readyForAction()", 1)[1].split("bool requestSave()", 1)[0]
        self.assertNotIn("LMNotice::", gate)
        for token in ("LmStatusPopupGameplayEvent(LMPractice::isOpen(), edge)",
                      "LmStatusPopupGameplayEvent(LMPractice::isOpen(), pendingHotkey && !hotkey)",
                      "LM_POPUP_SAVED", "LM_POPUP_LOADED", "LM_POPUP_REJECTED"):
            self.assertIn(token, source)
        warp = (ROOT / "lm_diag/src/lm_warp.cpp").read_text()
        self.assertIn('sStatus = "WARP: ARRIVED";\n    LMNotice::show(LM_POPUP_LOADED);', warp)
        notice = (ROOT / "lm_diag/src/lm_notice.cpp").read_text()
        self.assertNotIn("LMState::status", notice)
        self.assertNotIn("new ", notice)


if __name__ == "__main__": unittest.main()
