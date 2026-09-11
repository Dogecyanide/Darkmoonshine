"""Redistributed LM binaries must carry their compressor's licence."""
from pathlib import Path
import json
import re
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import xml.etree.ElementTree as ET

try:
    from scripts import package_launcher
except ModuleNotFoundError:
    import package_launcher

ROOT = Path(__file__).resolve().parents[1]
VERSION = "V1.0.0 Frozen in Time"


class LMPackageTests(unittest.TestCase):
    def test_root_theme_copy_uses_only_allowlisted_assets(self):
        with tempfile.TemporaryDirectory(prefix="lm-theme-package-") as directory:
            root = Path(directory)
            boot, mod, output = root / "boot.dol", root / "mod_lmj.bin", root / "test.zip"
            boot.write_bytes(b"boot")
            mod.write_bytes(b"mod")
            theme = root / "theme"
            theme.mkdir()
            (theme / "background.png").write_bytes(b"png")
            (theme / "bgm.mp3").write_bytes(b"mp3")
            (theme / "unrelated.key").write_bytes(b"do not distribute")
            with patch.object(package_launcher, "THEME_DIR", theme):
                self.assertEqual(package_launcher.main([
                    "--boot-dol", str(boot), "--out-zip", str(output),
                    "--mod-bins", str(mod)]), 0)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read("Darkmoonshine_Theme/background.png"), b"png")
                self.assertNotIn("Darkmoonshine_Theme/bgm.mp3", archive.namelist())
                self.assertFalse(any("unrelated" in name for name in archive.namelist()))
                self.assertIsNone(archive.testzip())

    def test_console_package_contains_miniz_notice(self):
        with tempfile.TemporaryDirectory(prefix="lm-package-") as directory:
            root = Path(directory)
            boot, mod, output = root / "boot.dol", root / "mod_lmj.bin", root / "test.zip"
            boot.write_bytes(b"test boot")
            mod.write_bytes(b"test mod")
            with patch.object(package_launcher, "render_meta", return_value="<app />"):
                self.assertEqual(package_launcher.main([
                    "--boot-dol", str(boot), "--out-zip", str(output),
                    "--mod-bins", str(mod)
                ]), 0)
            with zipfile.ZipFile(output) as archive:
                prefix = package_launcher.APP_NAME
                notice = archive.read(f"{prefix}/licenses/miniz.txt").decode()
                self.assertIn("Permission is hereby granted", notice)
                self.assertIn("Redistribution and use", archive.read(f"{prefix}/licenses/lz4.txt").decode())
                self.assertEqual(archive.read(f"{prefix}/mod_lmj.bin"), b"test mod")
                checklist = archive.read(f"{prefix}/TESTING.md").decode()
                self.assertEqual(checklist, (ROOT / "doc/lm-testing-current.md").read_text(encoding="utf-8"))
                self.assertEqual(checklist.splitlines()[0], "# DarkMoonshine — " + VERSION)
                self.assertEqual([int(number) for number in re.findall(r"(?m)^(\d+)\.\s", checklist)],
                                 list(range(1, 11)))
                self.assertIn("entire `lm_dumps` folder", checklist)
                self.assertIsNone(archive.testzip())

    def test_default_checklist_matches_release_version(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        for name in ("release_console", "diagnostic_console", "diagnostic_emulator"):
            preset = next(p for p in presets["configurePresets"] if p["name"] == name)
            self.assertEqual(preset["cacheVariables"]["LAUNCHER_VERSION"], VERSION)
        checklist = (ROOT / "doc/lm-testing-current.md").read_text(encoding="utf-8")
        self.assertEqual(checklist.splitlines()[0], "# DarkMoonshine — " + VERSION)

    def test_hbc_branding_uses_exact_release_and_author_strings(self):
        for regions in (["lmj"], []):
            with self.subTest(regions=regions):
                meta = ET.fromstring(package_launcher.render_meta("sd", regions))
                self.assertEqual(meta.findtext("name"), "DarkMoonshine")
                self.assertEqual(meta.findtext("author"), "Dogecyanide, Nintendont Team")
                self.assertEqual(meta.findtext("version"), VERSION)

    def test_non_lm_metadata_remains_unchanged(self):
        meta = ET.fromstring(package_launcher.render_meta("di", ["jp"], "upstream-test"))
        self.assertEqual(meta.findtext("name"), "Moonshine Luigi's Mansion")
        self.assertEqual(meta.findtext("author"), "Dogecyanide, panther03, Nintendont authors")
        self.assertEqual(meta.findtext("version"), "upstream-test")
        self.assertIn("Experimental: begin with same-room tests", meta.findtext("long_description"))

    def test_shared_branding_and_visible_labels_fit_existing_panels(self):
        branding = package_launcher.lm_branding()
        self.assertEqual(branding["VERSION"], VERSION)
        self.assertEqual(branding["NAME"], "DarkMoonshine")
        self.assertEqual(branding["AUTHORS"], "Dogecyanide, Nintendont Team")
        self.assertEqual(branding["PACKAGE_STEM"], "DarkMoonshine-1.0.0-Frozen-in-Time")
        for field in ("NAME", "VERSION", "AUTHORS"):
            self.assertLessEqual(25 + 10 * len(branding[field]), 455)
        self.assertLessEqual(13 + 6 * len(branding["VERSION"]), 314)
        menu = (ROOT / "launcher/loader/source/menu.c").read_text()
        for field in ("NAME", "VERSION", "AUTHORS"):
            self.assertIn("LM_BRANDING_" + field, menu)
        self.assertIn("SusaVersionGameID(gIni.version) == SUSAMUNE_MOD_GAME_ID_LMJ", menu)
        self.assertIn("Copyright (C) 2013  crediar", menu)
        self.assertIn("GNU General Public License", menu)
        practice = (ROOT / "lm_diag/src/lm_practice.cpp").read_text()
        self.assertIn('kMenuTop + 5u, "%s", LM_BRANDING_NAME', practice)
        self.assertIn('kMenuTop + 164u, "%s", LM_BRANDING_VERSION', practice)

    def test_rebranding_does_not_move_installed_app_or_user_files(self):
        self.assertEqual(package_launcher.APP_NAME, "moonshine_luigis_mansion")
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('"${CMAKE_BINARY_DIR}/moonshine_luigis_mansion_launcher.zip"', cmake)
        self.assertIn('"${CMAKE_BINARY_DIR}/${_lm_package_stem}.zip"', cmake)
        self.assertIn('file(STRINGS "${_lm_branding_header}" _lm_package_line', cmake)
        ini = (ROOT / "include/susamune/susamune_cfg.h").read_text()
        self.assertIn('"/moonshine_lm.ini"', ini)
        crash = (ROOT / "launcher/kernel/SusamuneCrash.c").read_text()
        for suffix in ("a.bin", "b.bin", "a.txt", "b.txt"):
            self.assertIn('"%s/luigis_mansion_crash_' + suffix + '"', crash)


if __name__ == "__main__":
    unittest.main()
