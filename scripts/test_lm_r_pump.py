"""Integration contracts for the standalone raw-input R-pump display."""
from pathlib import Path
import hashlib
import os
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RPumpIntegrationTests(unittest.TestCase):
    def test_display_toggle_is_appended_and_off_by_default(self):
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        self.assertIn("bool sInputs, sLagVisible, sRPumpVisible;", source)
        self.assertIn("return timing ? 7u : 6u;", source)
        action = source.split("bool action(bool timing", 1)[1].split("if (!row)", 1)[0]
        self.assertIn("if (row == 4u) { sRPumpVisible = !sRPumpVisible; sRPump.reset(); }", action)
        self.assertIn("R-pump display  %s", source)
        self.assertIn("R = digital click OR raw analog 30+", source)
        self.assertIn("Held game frames; release not counted", source)

    def test_sampling_is_update_only_and_cancels_discontinuities(self):
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        tick = source.split("void tick(bool menuOpen)", 1)[1].split("void draw(", 1)[0]
        self.assertIn("sRPump.sample(LMTiming::rTriggerDown", tick)
        self.assertIn("sRPumpVisible && active && !sWasMenu && !changed &&", tick)
        self.assertIn("!loaded && p == sPlayer", tick)
        self.assertIn("p && !menuOpen && sPad.mCurError == 0u", tick)
        self.assertLess(tick.index("sRPump.sample("), tick.index("sPlayer = p;"))
        draw = source.split("void draw(", 1)[1].split("u32 rows(", 1)[0]
        self.assertNotIn("sRPump.sample", draw)
        self.assertIn("sRPump.sample(LMTiming::rTriggerDown(sPad),", tick)
        practice = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        self.assertLess(practice.index("LMTools::samplePad(statuses[0]);"),
                        practice.index("pad->mButton = 0u;"))

    def test_popup_is_independent_of_bottom_overlay_and_idle_is_hidden(self):
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        draw = source.split("void draw(", 1)[1].split("u32 rows(", 1)[0]
        popup = draw.split("if (sRPumpVisible && sRPump.visible())", 1)[1].split("u32 lines", 1)[0]
        self.assertIn("220, 109, 94, 21", popup)
        self.assertIn("sRPump.holding ? sRPump.hold : sRPump.last", popup)
        self.assertIn("LMDraw::flush(xfb, 640u, 480u, 109, 21);", popup)
        self.assertIn("capped ? 99999u : frames", popup)
        self.assertNotIn("sRPump", draw.split("u32 lines", 1)[1])
        self.assertNotIn('"READY"', popup)
        self.assertNotIn('"WAIT"', popup)

    def test_raw_trigger_readout_is_input_toggle_only(self):
        source = (ROOT / "lm_diag/src/lm_tools.cpp").read_text()
        draw = source.split("void draw(", 1)[1].split("u32 rows(", 1)[0]
        block = draw.split("if (sInputs) {", 1)[1].split("if (sRPumpVisible", 1)[0]
        self.assertIn('"L%03u R%03u %04X", sPad.mTriggerLeft,', block)
        self.assertIn("sPad.mTriggerRight, sPad.mButton", block)
        self.assertIn('"PAD disconnected"', block)
        self.assertIn("LMDraw::flush(xfb, 640u, 480u, 219, 9);", block)


class RPumpRetailPadProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
        if not path.exists():
            raise unittest.SkipTest("Clean GLMJ01 DOL unavailable")
        cls.raw = path.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise AssertionError("Wrong retail DOL")
        cls.sections = []
        for count, offsets in ((7, (0, 0x48, 0x90)), (11, (0x1C, 0x64, 0xAC))):
            for i in range(count):
                cls.sections.append(tuple(struct.unpack_from(">I", cls.raw, base + 4 * i)[0]
                                          for base in offsets))

    def words(self, address, expected):
        off, start, size = next(section for section in self.sections
                                if section[1] <= address < section[1] + section[2])
        self.assertLessEqual(address + len(expected) * 4, start + size)
        actual = struct.unpack_from(f">{len(expected)}I", self.raw, off + address - start)
        self.assertEqual(actual, expected, hex(address))

    def test_hook_forwards_the_unmodified_raw_status_before_jut_processing(self):
        self.words(0x801D20B0, (0x387D0018, 0x48012849))
        self.words(0x801E4C10, (0x3B39000C, 0x3AB5000C))
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        wrapper = source.split('extern "C" u32 diagnosticPadRead', 1)[1]
        self.assertLess(wrapper.index("reinterpret_cast<PadReadFn>(kPadReadAddress)(statuses)"),
                        wrapper.index("LMPractice::filterPadRead(statuses);"))
        capture = source.split("void filterPadRead(PADStatus *statuses)", 1)[1].split("void tick()", 1)[0]
        self.assertLess(capture.index("LMTools::samplePad(statuses[0]);"),
                        capture.index("pad->mTriggerRight = 0u;"))

    def test_retail_decoder_uses_distinct_l_r_bytes_and_button_masks(self):
        # Normal analog mode writes high trigger byte to L +6, low byte to R +7.
        self.words(0x801E5294, (0x80C50004, 0x54C6C63E, 0x98C40006,
                               0x80A50004, 0x98A40007))
        # Legacy-spec conversion independently derives L 0x40 and R 0x20.
        self.words(0x801E5084, (0x88040006, 0x280000AA, 0x41800010,
                               0xA0040000, 0x60000040, 0xB0040000))
        self.words(0x801E509C, (0x88040007, 0x280000AA, 0x41800010,
                               0xA0040000, 0x60000020, 0xB0040000))
        source = (ROOT / "launcher/loader/source/ppc/global.h").read_text()
        self.assertRegex(source, r"PAD_TRIGGER_R\s+0x0020")
        self.assertRegex(source, r"PAD_TRIGGER_L\s+0x0040")


if __name__ == "__main__":
    unittest.main()
