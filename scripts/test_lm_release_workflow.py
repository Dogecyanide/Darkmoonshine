"""Release workflow contracts, including its executable ZIP verification body."""
from contextlib import contextmanager, redirect_stdout
import io
import json
import os
from pathlib import Path
import re
import struct
import tempfile
import textwrap
import unittest
from unittest.mock import patch
import warnings
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / '.github/workflows/release-launchers.yml'
VERSION = 'V1.0.0 Frozen in Time'
ZIP_PATH = 'build-lm-diag/DarkMoonshine-1.0.0-Frozen-in-Time.zip'


@contextmanager
def working_directory(path):
    previous = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)


class ReleaseWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.workflow = WORKFLOW.read_text(encoding='utf-8')
        match = re.search(r"^          @'\n(.*?)^          '@ \| venv/Scripts/python.exe -$",
                          self.workflow, re.M | re.S)
        self.assertIsNotNone(match, 'Executable release validator must remain in workflow')
        self.verify = compile(textwrap.dedent(match[1]), str(WORKFLOW), 'exec')

    def test_published_release_only_and_safe_tag_arguments(self):
        self.assertIn('on:\n  release:\n    types: [published]', self.workflow)
        self.assertNotIn('workflow_dispatch', self.workflow)
        self.assertNotIn('gh release create', self.workflow)
        self.assertIn('RELEASE_TAG: ${{ github.event.release.tag_name }}', self.workflow)
        self.assertIn('GH_REPO: ${{ github.repository }}', self.workflow)
        self.assertIn('gh release upload "$env:RELEASE_TAG" "$env:RELEASE_ZIP"', self.workflow)
        for line in self.workflow.splitlines():
            if 'run:' in line:
                self.assertNotIn('${{', line)

    def test_full_mod_presets_no_retail_iso_dependencies(self):
        presets = json.loads((ROOT / 'CMakePresets.json').read_text())
        configure = next(p for p in presets['configurePresets'] if p['name'] == 'diagnostic_console')
        build = next(p for p in presets['buildPresets'] if p['name'] == 'diagnostic')
        self.assertEqual(configure['cacheVariables']['LM_BOOTSTRAP'], 'ON')
        self.assertEqual(configure['cacheVariables']['LM_DIAGNOSTIC'], 'ON')
        self.assertNotEqual(configure['cacheVariables'].get('LM_EMULATOR'), 'ON')
        self.assertEqual(configure['binaryDir'], '${sourceDir}/build-lm-diag')
        self.assertEqual(build['configurePreset'], 'diagnostic_console')
        self.assertIn('launcher', build['targets'])
        self.assertIn('cmake --preset diagnostic_console', self.workflow)
        self.assertIn('cmake --build --preset diagnostic', self.workflow)
        self.assertNotIn('cmake --preset release_console', self.workflow)
        self.assertNotRegex(self.workflow, r'(?i)(\.iso\b|clean_glmj|fake-vmem|dolphin|\.lms\b)')
        self.assertIn('lfs: true', self.workflow)

    def test_final_paths_notes_and_licenses_are_explicit(self):
        self.assertIn(f'RELEASE_VERSION: {VERSION}', self.workflow)
        self.assertIn(f'RELEASE_ZIP: {ZIP_PATH}', self.workflow)
        for argument in ("'--mod-bins', 'build-lm-diag/mod_lmj.bin'",
                         "'--source', 'sd'",
                         "'--boot-dol', 'build-lm-diag/launcher/shared/boot.dol'",
                         "'--test-log', 'doc/lm-testing-current.md'",
                         "'--changelog', 'doc/darkmoonshine-1.0.0.md'"):
            self.assertIn(argument, self.workflow)
        self.assertLess(self.workflow.index('Verify exact release contents'),
                        self.workflow.index('Upload launcher to release'))

    @staticmethod
    def fixture(root):
        app = 'moonshine_luigis_mansion'
        paths = {
            f'{app}/boot.dol': 'build-lm-diag/launcher/shared/boot.dol',
            f'{app}/icon.png': 'launcher/icon.png',
            f'{app}/mod_lmj.bin': 'build-lm-diag/mod_lmj.bin',
            f'{app}/TESTING.md': 'doc/lm-testing-current.md',
            f'{app}/CHANGELOG.md': 'doc/darkmoonshine-1.0.0.md',
            f'{app}/licenses/miniz.txt': 'lm_diag/vendor/miniz/LICENSE',
            f'{app}/licenses/lz4.txt': 'lm_diag/vendor/lz4/LICENSE',
            'Darkmoonshine_Theme/background.png': 'launcher/Darkmoonshine_Theme/background.png',
            'Darkmoonshine_Theme/bgm.mp3': 'launcher/Darkmoonshine_Theme/bgm.mp3',
        }
        entries = {name: source.encode() for name, source in paths.items()}
        mod = struct.pack('>8I', 0x534D4F44, 2, 0x474C4D4A, 0x804B8400, 4, 1, 0x82000, 52)
        mod += b'CODE' + struct.pack('>3I', 0x80003100, 0, 1)
        mod += struct.pack('>I', zlib.crc32(mod))
        entries[f'{app}/mod_lmj.bin'] = mod
        for name, source in paths.items():
            path = root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(entries[name])
        entries[f'{app}/meta.xml'] = (
            f'<app><name>DarkMoonshine</name><author>Dogecyanide, Nintendont Team</author>'
            f'<version>{VERSION}</version></app>').encode()
        return entries

    def execute(self, changes=None, duplicate=False, bad_source_mod=False):
        with tempfile.TemporaryDirectory(prefix='lm-release-workflow-') as directory:
            root = Path(directory)
            entries = self.fixture(root)
            if changes:
                for key, value in changes.items():
                    if value is None:
                        del entries[key]
                    else:
                        entries[key] = value
            if bad_source_mod:
                name = 'moonshine_luigis_mansion/mod_lmj.bin'
                entries[name] = entries[name][:-1] + bytes([entries[name][-1] ^ 1])
                (root / 'build-lm-diag/mod_lmj.bin').write_bytes(entries[name])
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(root / ZIP_PATH, 'w') as package:
                    for key, value in entries.items():
                        package.writestr(key, value)
                    if duplicate:
                        package.writestr('moonshine_luigis_mansion/boot.dol', b'extra')
            with working_directory(root), patch.dict(os.environ, {'RELEASE_ZIP': ZIP_PATH,
                    'RELEASE_VERSION': VERSION}), redirect_stdout(io.StringIO()):
                exec(self.verify, {})

    def test_validator_accepts_complete_matching_release(self):
        self.execute()

    def test_validator_rejects_bootstrap_missing_license_or_theme(self):
        for name in ('moonshine_luigis_mansion/mod_lmj.bin',
                     'moonshine_luigis_mansion/licenses/miniz.txt',
                     'moonshine_luigis_mansion/licenses/lz4.txt',
                     'moonshine_luigis_mansion/CHANGELOG.md',
                     'Darkmoonshine_Theme/background.png', 'Darkmoonshine_Theme/bgm.mp3'):
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                self.execute({name: None})

    def test_validator_rejects_unexpected_user_data_duplicate_or_wrong_build(self):
        for changes in ({'lm_states/private.key': b'private'},
                        {'moonshine_luigis_mansion/boot.dol': b'wrong'},
                        {'moonshine_luigis_mansion/meta.xml': (
                            f'<app><name>DarkMoonshine</name><author>Wrong</author>'
                            f'<version>{VERSION}</version></app>').encode()},
                        {'moonshine_luigis_mansion/meta.xml': b'<app><name>DarkMoonshine</name><version>RC4</version></app>'}):
            with self.assertRaises(RuntimeError):
                self.execute(changes)
        with self.assertRaises(RuntimeError):
            self.execute(duplicate=True)
        with self.assertRaises(RuntimeError):
            self.execute(bad_source_mod=True)


if __name__ == '__main__':
    unittest.main()
