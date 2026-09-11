"""Execute the confirmation helper and check that UI submission uses its frozen identity."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ArchiveDeleteUITests(unittest.TestCase):
    def test_confirmation_and_frozen_identity(self):
        with tempfile.TemporaryDirectory(prefix='lm-delete-ui-') as folder:
            binary = Path(folder) / 'delete.exe'
            result = subprocess.run([
                str(ROOT / 'toolchain/clang++.exe'), '--target=x86_64-pc-windows-msvc',
                '-fuse-ld=lld', '-nostdlib', '-fno-stack-protector',
                '-Wl,/entry:main,/subsystem:console', '-I', str(ROOT / 'include'),
                str(ROOT / 'scripts/test_lm_archive_delete.cpp'), '-o', str(binary)
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(binary)]).returncode, 0)

    def test_ui_requires_fresh_confirm_and_preserves_result_during_refresh(self):
        source = (ROOT / 'lm_diag/src/lm_practice.cpp').read_text(encoding='utf-8')
        body = source.split('void archiveInput(', 1)[1].split('void switchPage(', 1)[0]
        self.assertIn('buttons & ~sPreviousButtons, sMenuPad.mCurError == 0u', body)
        self.assertIn('LMState::requestDelete(sDeletePrompt.id, sDeletePrompt.token)', body)
        self.assertLess(body.index('if (sDeletePending)'), body.index('if (edge(buttons, kButtonB))'))
        self.assertLess(body.index('sDeleteResult[i] = 0;'), body.index('browseArchives(0u)'))
        self.assertIn('if (LMState::storageBusy()) return;', body)
        self.assertIn('This cannot be undone.', source)
        self.assertIn('A: Delete permanently   B: Cancel', source)
        for text in ['PERMANENTLY DELETE SD STATE?', 'A: Delete permanently   B: Cancel']:
            self.assertLessEqual(35 + len(text) * 6, 295)

if __name__ == '__main__':
    unittest.main()
