"""Native tank planning and authenticated clean-JP elemental pickup behavior."""
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ElementPlanTests(unittest.TestCase):
    def test_native_plan_and_refusals(self):
        compiler = ROOT / "toolchain/clang.exe"
        if not compiler.exists():
            self.skipTest("Bundled Windows clang unavailable")
        with tempfile.TemporaryDirectory(prefix="lm-element-") as folder:
            exe = Path(folder) / "test.exe"
            r = subprocess.run([str(compiler), "--target=x86_64-pc-windows-msvc",
                "-fuse-ld=lld", "-nostdlib", "-fno-stack-protector",
                "-Wl,/entry:main,/subsystem:console", "-I", str(ROOT / "include"),
                str(ROOT / "scripts/lm_element_harness.c"), "-o", str(exe)],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_runtime_does_not_grant_flags_or_replace_action(self):
        src = (ROOT / "lm_diag/src/lm_elements.cpp").read_text()
        self.assertIn("LMState::readyForActionNow()", src)
        self.assertIn("LMWarp::active()", src)
        self.assertIn("word(0x803C7CACu) != 0u", src)
        self.assertIn("word(player) != 0x8034EE50u", src)
        self.assertIn("word(player + 0x1180u)", src)
        self.assertNotIn("getFlag", src)
        self.assertNotIn("0x80065320", src)
        self.assertNotIn("NeedMedal", src)
        self.assertNotIn("0x80065358", src)
        self.assertNotIn("0x80065398", src)
        self.assertEqual(src.count("*reinterpret_cast<volatile f32 *>"), 1)
        self.assertEqual(src.count("*reinterpret_cast<volatile u32 *>"), 1)
        self.assertIn("static_cast<f32>(plan.fuel)", src)

    def test_tank_plan_has_no_story_flag_or_inventory_dependency(self):
        plan = (ROOT / "include/susamune/lm_element_plan.h").read_text()
        signature = plan.split("static inline int LmPlanElement(", 1)[1].split(") {", 1)[0]
        self.assertNotIn("medals", signature)
        self.assertNotIn("flags", signature)

    def test_wait_reasons_preserve_guard_order_and_only_weapon_path_blames_weapon(self):
        source = (ROOT / "lm_diag/src/lm_elements.cpp").read_text()
        body = source.split('Result apply(', 1)[1]
        ordered = (
            'if (LMWarp::active()) return Result::Warping;',
            'if (!LMState::readyForActionNow()) return Result::NotReady;',
            'if (word(0x803C7CACu) != 0u) return Result::Event;',
            'word(player) != 0x8034EE50u',
            'static_cast<Result>(result)',
            '*reinterpret_cast<volatile f32 *>')
        positions = [body.index(part) for part in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('if (*reinterpret_cast<volatile const s16 *>(player + 0xFCu) <= 0)\n'
                      '        return Result::PlayerInactive;', body)
        self.assertNotIn('return Result::Busy;', body)
        self.assertIn('word(player + 0x1180u)', body)
        header = (ROOT / "lm_diag/include/lm_elements.hxx").read_text()
        self.assertIn('Applied = 0, Busy = 1, Invalid = 3', header)
        self.assertIn('case Result::Busy: return "WAIT: SUCTION / SPRAY ACTIVE";', source)
        for reason, text in (
                ('Warping', 'WAIT FOR ROOM TRANSITION'),
                ('NotReady', 'WAIT FOR GAME / STREAMING'),
                ('Event', 'WAIT FOR EVENT / CUTSCENE'),
                ('PlayerInactive', 'WAIT FOR LUIGI TO RECOVER')):
            self.assertIn(f'case Result::{reason}: return "{text}";', source)
            self.assertNotIn('SUCTION', text)
            self.assertNotIn('SPRAY', text)
        self.assertNotIn('WAIT / RELEASE SUCTION AND SPRAY', source)

class RetailElementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from dolreader.dol import DolFile
        path = ROOT / "build-lm-diag/clean_glmj_main.dol"
        if not path.exists(): raise unittest.SkipTest("Clean JP DOL unavailable")
        if hashlib.sha1(path.read_bytes()).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise AssertionError("Wrong Japanese retail DOL")
        with path.open("rb") as f: cls.dol = DolFile(f)

    def data(self, addr, size):
        self.dol.seek(addr)
        return self.dol.read(size)

    def words(self, values):
        for addr, expected in values.items():
            with self.subTest(addr=hex(addr)):
                self.assertEqual(int.from_bytes(self.data(addr, 4), "big"), expected)

    def test_pickup_type_mapping_and_capacity(self):
        # Registry names authenticate that 0x72/0x73/0x74 are fire/ice/water.
        for actor, label in ((0x72, b"elfire\0"), (0x73, b"elice\0"), (0x74, b"elwater\0")):
            ptr = int.from_bytes(self.data(0x803306D0 + actor * 0x1C + 0x18, 4), "big")
            self.assertEqual(self.data(ptr, len(label)), label)
        self.words({0x800AE01C: 0x2C000074, 0x800AE028: 0x2C000072,
                    0x800AE044: 0x38800002, 0x800AE04C: 0x38800003,
                    0x800AE054: 0x38800004, 0x800AE05C: 0x38800001,
                    0x800AE068: 0x806DACCC, 0x8049B7AC: 100,
                    0x800AE088: 0xD01F1188, 0x800AE08C: 0x909F1184})

    def test_native_depletion_and_hud_consume_same_pair(self):
        self.words({0x800AF548: 0xD01F1188, 0x800AF54C: 0x38000001,
                    0x800AF554: 0x901F1184, 0x800B6D64: 0x801E1184,
                    0x800B6DA0: 0xC01E1188, 0x800B6EB4: 0x801E1184,
                    0x800B6EB8: 0x901F0058, 0x800B1F7C: 0x3BE00001,
                    0x800B1F14: 0x3BE00000, 0x800B2114: 0x93FE1180})

    def test_old_medal_proxy_is_first_element_event_completion(self):
        for actor, label in ((0x75, b"elffst\0"), (0x76, b"elifst\0"), (0x77, b"elwfst\0")):
            ptr = int.from_bytes(self.data(0x803306D0 + actor * 0x1C + 0x18, 4), "big")
            self.assertEqual(self.data(ptr, len(label)), label)
        self.words({0x800D0F4C: 0x2C030075, 0x800D0F64: 0x3860002B,
                    0x800D0F6C: 0x3860002D, 0x800D0F74: 0x3860002C})

if __name__ == "__main__": unittest.main()
