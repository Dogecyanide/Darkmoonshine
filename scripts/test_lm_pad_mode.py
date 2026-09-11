"""Authenticated GLMJ01 wire-mode compatibility; no L/R remapping."""
import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]


def decode_trigger_reply(mode, word):
    # Retail PAD spec-5 switch, authenticated below. Other report fields and
    # digital button bits are outside this word's low 16 bits.
    if mode == 3:
        return (word >> 8) & 255, word & 255, 0, 0
    assert mode == 0
    return tuple(((word >> shift) & 15) << 4 for shift in (12, 8, 4, 0))


class PadModeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
        if not path.exists():
            raise unittest.SkipTest("Clean GLMJ01 DOL unavailable")
        cls.raw = path.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise AssertionError("Wrong retail DOL")
        cls.sections = [tuple(struct.unpack_from(">I", cls.raw, b + 4 * i)[0] for b in q)
                        for n, q in ((7, (0, 0x48, 0x90)), (11, (0x1C, 0x64, 0xAC)))
                        for i in range(n)]
        spec = importlib.util.spec_from_file_location("lm_pad_patches", ROOT / "lm_diag/patches.py")
        cls.patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.patch)

    def words(self, address, expected):
        off, start, size = next(s for s in self.sections
                                if s[1] <= address < s[1] + s[2])
        self.assertLessEqual(address + 4 * len(expected), start + size)
        self.assertEqual(struct.unpack_from(f">{len(expected)}I", self.raw, off + address - start),
                         expected, hex(address))

    def test_init_changes_both_mode_values_without_replacing_retail_calls(self):
        self.words(0x801D2070, (0x48012D31, 0x38000000, 0x900D158C,
                               0x38600000, 0x48013461, 0x48012559))
        edits = [p for p in self.patch.patches if 0x801D2060 <= p["lmj"] < 0x801D2098]
        self.assertEqual([(p["lmj"], p["expected"], p["val"], p["type"].name) for p in edits],
                         [(0x801D2074, 0x38000000, 0x38000003, "W32"),
                          (0x801D207C, 0x38600000, 0x38600003, "W32")])
        guards = {p["addr"]: p["expected"] for p in self.patch.checks}
        for address, word in ((0x801D2078, 0x900D158C), (0x801D2080, 0x48013461),
                              (0x801D2084, 0x48012559)):
            self.assertEqual(guards[address], word)
        # PADSetAnalogMode moves r3 into r31, shifts it 8 and stores the SDK
        # analog-mode global. Only this retail callsite requests a wire mode.
        self.words(0x801E54F0, (0x7C7F1B78,))
        self.words(0x801E54FC, (0x57E6402E,))
        self.words(0x801E5518, (0x90CDFF90,))
        calls = []
        for off, address, size in self.sections[:7]:
            for pos in range(0, size - 3, 4):
                word = struct.unpack_from(">I", self.raw, off + pos)[0]
                if word & 0xFC000003 == 0x48000001:
                    disp = word & 0x03FFFFFC
                    if disp & 0x02000000:
                        disp -= 0x04000000
                    if address + pos + disp == 0x801E54E0:
                        calls.append(address + pos)
        self.assertEqual(calls, [0x801D2080])

    def test_mode_decoders_and_shared_clamp_are_authenticated(self):
        self.words(0x801E5198, (0x80050004, 0x5400C636, 0x98040006,
                               0x80050004, 0x5400E636, 0x98040007))
        self.words(0x801E5294, (0x80C50004, 0x54C6C63E, 0x98C40006,
                               0x80A50004, 0x98A40007, 0x98040008, 0x98040009))
        self.words(0x801D20B8, (0x387D0018, 0x48011239))  # Same PADClamp call.
        self.words(0x804A0A60, (0x1EB40F48,))  # SDK triggers deadzone30/cap180.

    def test_unconverted_mode3_reply_reproduces_old_phob_symptom(self):
        # v0.29 Phob 2 ignores the requested report mode and returns full L,R
        # bytes. Retail mode0 splits them into L/R/A/B nibbles instead.
        self.assertEqual(decode_trigger_reply(0, 0x00FF0000 | (255 << 8)),
                         (240, 240, 0, 0))  # L alone also becomes R.
        self.assertEqual(decode_trigger_reply(0, 0x00FF0000 | 255),
                         (0, 0, 240, 240))  # R alone becomes analog A/B.
        for left in range(256):
            for right in range(256):
                self.assertEqual(decode_trigger_reply(3, (left << 8) | right),
                                 (left, right, 0, 0))

    def test_native_game_routes_vacuum_to_r_and_element_to_l(self):
        # CButton copies raw +6/+7 to its +E/+F, then JUT owns it at +18.
        self.words(0x801D25C4, (0x88040006, 0x9803000E, 0x88040007, 0x9803000F))
        # Native analog binding5 is JUT+26(L), binding6 JUT+27(R).
        self.words(0x803470DC, (0x8007F48C, 0x8007F4EC))
        self.words(0x8007F48C, (0x88630026,))
        self.words(0x8007F4EC, (0x88630027,))
        self.words(0x8007EA2C, (0x809E012C, 0x480003FD, 0xD03E0020))
        self.words(0x8007EA8C, (0x809E0144, 0x4800039D, 0xD03E002C))
        self.words(0x8007EE60, (0x806301B0, 0x480005DD))


if __name__ == "__main__":
    unittest.main()
