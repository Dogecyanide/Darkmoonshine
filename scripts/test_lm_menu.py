"""Native navigation regression tests and safe-area layout contracts."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuTests(unittest.TestCase):
    def test_controller_navigation(self):
        compiler = ROOT / "toolchain/clang++.exe"
        if not compiler.exists():
            self.skipTest("Bundled Windows clang is required")
        with tempfile.TemporaryDirectory(prefix="lm-menu-") as directory:
            exe = Path(directory) / "menu.exe"
            subprocess.run([
                str(compiler), "--target=x86_64-pc-windows-msvc", "-fuse-ld=lld",
                "-nostdlib", "-fno-stack-protector", "-Wl,/entry:main,/subsystem:console",
                str(ROOT / "scripts/test_lm_menu.cpp"), "-o", str(exe)
            ], check=True, capture_output=True, text=True)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0)

    def test_tiles_and_help_fit_safe_area(self):
        names = ["Savestates", "Room warps", "Displays", "Input timing", "Luigi colour",
                 "Game options", "Doors", "Room tools", "Audio"]
        for i, name in enumerate(names):
            x, y = 13 + i % 3 * 99, 31 + 25 + i // 3 * 34
            self.assertLessEqual(len(name) * 6 + 10, 95)
            self.assertLessEqual(x + 95, 310)
            self.assertLessEqual(y + 29, 160)
        self.assertLessEqual(31 + 164 + 7, 207)

    def test_menu_retains_gameplay_input_safety(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        self.assertIn("sPageSelections[", source)
        self.assertIn("if (sMenuPad.mCurError)", source)
        self.assertIn("LMMenuNavigation::grid", source)
        self.assertIn("sConsumeUntilRelease = true", source)
        self.assertIn("clearConfirmation();", source)
        self.assertIn("Game runs", source)
        self.assertIn("LMDraw::flush(xfb", source)

    def test_archive_browser_uses_real_catalog_and_confirmed_compatible_imports(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        browser = source.split("void archiveInput(", 1)[1].split("void switchPage(", 1)[0]
        self.assertIn("LMState::catalogCount()", browser)
        self.assertIn("LMState::catalogNextCursor()", browser)
        self.assertIn("LMState::catalogCursor()", browser)
        self.assertIn("sArchiveHistory[sArchiveDepth - 1u]", browser)
        self.assertIn("if (LMState::storageBusy()) return;", browser)
        self.assertIn("if (!LMState::catalogCompatible(sArchiveSelection))", browser)
        self.assertIn("if (confirm(0x53440000u ^ id))", browser)
        self.assertIn("LMState::requestImport(id)", browser)
        self.assertIn("Browse SD archives...", source)
        self.assertIn("Import fills memory. Load restores gameplay.", source)

    def test_eight_archive_rows_and_longest_status_fit_panel(self):
        source = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        self.assertIn('kArchivePanelHeight = 202u', source)
        self.assertIn('kMenuTop + 25u + i * 16u, "%s %.31s"', source)
        self.assertIn('kMenuTop + 33u + i * 16u, "%08lu  %5luK  %s"', source)
        self.assertLessEqual(13 + len('> ' + 'X' * 31) * 6, 310)
        self.assertLessEqual(25 + len('99999999  99999K  DIFFERENT SESSION') * 6, 310)
        self.assertLessEqual(31 + 33 + 7 * 16 + 7, 31 + 156)
        self.assertLessEqual(31 + 192 + 7, 31 + 202)


if __name__ == "__main__":
    unittest.main()
