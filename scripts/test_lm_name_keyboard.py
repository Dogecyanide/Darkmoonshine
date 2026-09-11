"""Exercise the actual portable Moonshine naming keyboard and LM modal contracts."""
import ast
import ctypes
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
A, B, X, Y, Z, START, L, R = 0x100, 0x200, 0x400, 0x800, 0x10, 0x1000, 0x40, 0x20


class KeyboardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('g++') or shutil.which('clang++')
        fallback = Path('C:/msys64/mingw64/bin/g++.exe')
        if not compiler and fallback.exists(): compiler = str(fallback)
        if not compiler: raise unittest.SkipTest('Native C++ compiler unavailable')
        cls.temp = tempfile.TemporaryDirectory(prefix='lm-name-keyboard-')
        library = Path(cls.temp.name) / ('keyboard.dll' if os.name == 'nt' else 'keyboard.so')
        command = [compiler, '-shared', '-O2', '-std=c++17', '-Wall', '-Werror',
                   '-I', str(ROOT / 'include'), str(ROOT / 'scripts/lm_name_keyboard_harness.cpp'),
                   '-o', str(library)]
        if os.name != 'nt': command.insert(2, '-fPIC')
        env = os.environ.copy()
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
        subprocess.run(command, check=True, capture_output=True, env=env, timeout=60)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.reset.argtypes = [ctypes.c_char_p]
        cls.lib.step.argtypes = [ctypes.c_uint, ctypes.c_uint]
        cls.lib.draft_text.restype = ctypes.c_char_p
        cls.lib.metric.argtypes = [ctypes.c_uint]

    @classmethod
    def tearDownClass(cls):
        if os.name == 'nt':
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self): self.lib.reset(None)
    def press(self, mask, held=None): return self.lib.step(mask, mask if held is None else held)
    def text(self): return self.lib.draft_text()
    def tearDown(self): self.assertEqual(self.lib.metric(5), 1, 'Draft overwrote a guard')

    def test_bounded_prefill(self):
        self.lib.reset(b'x' * 80)
        self.assertEqual(self.text(), b'x' * 31)
        self.assertEqual(self.lib.metric(0), 31)
        self.assertLessEqual(self.lib.metric(6), 40)

    def test_prefill_replaces_nonprintable_bytes(self):
        self.lib.reset(b'A\x01\x7f\xff%Z')
        self.assertEqual(self.text(), b'A???%Z')

    def test_append_and_backspace(self):
        self.press(A); self.press(2); self.press(A); self.press(B)
        self.assertEqual(self.text(), b'a')
        self.press(B); self.press(B)
        self.assertEqual(self.text(), b'')

    def test_all_grid_wrapping(self):
        self.press(1); self.assertEqual(self.lib.metric(1), 31)
        self.press(2); self.assertEqual(self.lib.metric(1), 0)
        self.press(8); self.assertEqual(self.lib.metric(1), 24)
        self.press(4); self.assertEqual(self.lib.metric(1), 0)

    def test_exact_upstream_pages_and_controls(self):
        upstream = (ROOT / 'src/creation_extras.cpp').read_text()
        local = (ROOT / 'include/susamune/lm_name_keyboard.h').read_text()
        for original, copied in (('gCreationLettersLower','kLower'),
                                 ('gCreationLettersUpper','kUpper'),('gCreationSymbols','kSymbols')):
            pattern = lambda name: name + r'\[33\]\s*=\s*("(?:\\.|[^"\\])*");'
            self.assertEqual(ast.literal_eval(re.search(pattern(original), upstream).group(1)),
                             ast.literal_eval(re.search(pattern(copied), local).group(1)))
        self.press(Y); self.press(A); self.press(X); self.press(L); self.press(A)
        self.assertEqual(self.text(), b'A 0')
        self.press(R); self.press(A)
        self.assertEqual(self.text(), b'A 0A')

    def test_both_pages_are_printable_and_full_capacity_stops_typing(self):
        for page in range(2):
            self.lib.reset(None)
            if page: self.press(L)
            for _ in range(32):
                self.press(A); self.press(2)
            self.assertEqual(len(self.text()), 31)
            self.assertTrue(all(32 <= ch <= 126 for ch in self.text()))
            self.press(X); self.press(A)
            self.assertEqual(len(self.text()), 31)

    def test_start_requires_separate_confirmation(self):
        self.press(A)
        self.assertEqual(self.press(START | A), 0)
        self.assertEqual(self.text(), b'a')
        self.assertEqual(self.lib.metric(4), 1)
        self.assertEqual(self.press(A), 1)
        self.assertEqual(self.text(), b'a')

    def test_discard_chord_does_not_insert_space(self):
        self.lib.reset(b'original')
        self.assertEqual(self.press(X | START), 0)
        self.assertEqual(self.lib.metric(4), 2)
        self.assertEqual(self.text(), b'original')
        self.assertEqual(self.press(A), 2)

    def test_cancel_confirmation_preserves_draft_without_delete(self):
        self.lib.reset(b'keep me')
        for trigger in (START, START | X, Z):
            self.press(trigger)
            self.press(B)
            self.assertEqual(self.text(), b'keep me')
            self.assertEqual(self.lib.metric(4), 0)

    def test_clear_requires_confirmation_and_only_clears_draft(self):
        self.lib.reset(b'clear me')
        self.press(Z | A)
        self.assertEqual(self.text(), b'clear me')
        self.assertEqual(self.lib.metric(4), 3)
        self.assertEqual(self.press(A), 0)
        self.assertEqual(self.text(), b'')

    def test_navigation_and_typing_cannot_leak_through_confirmation(self):
        self.lib.reset(b'unchanged')
        self.press(START)
        self.press(2 | X | Y | L)
        self.assertEqual(self.text(), b'unchanged')
        self.assertEqual(self.lib.metric(1), 0)
        self.assertEqual(self.lib.metric(2), 0)
        self.assertEqual(self.lib.metric(3), 0)


class KeyboardIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'lm_diag/src/lm_practice.cpp').read_text()
        cls.input = cls.source.split('void nameKeyboardInput(', 1)[1].split('void drawNameKeyboard(', 1)[0]
        cls.begin = cls.source.split('void beginNameKeyboard(', 1)[1].split('void nameKeyboardInput(', 1)[0]
        cls.browser = cls.source.split('void archiveInput(', 1)[1].split('void switchPage(', 1)[0]

    def test_naming_is_modal_before_browser_and_menu_back(self):
        tick = self.source.split('void tick()', 1)[1].split('void draw(', 1)[0]
        self.assertLess(tick.index('if (sNameAction != NameAction::None)'),
                        tick.index('else if (sArchiveBrowser)'))
        self.assertLess(tick.index('nameKeyboardInput(buttons);'),
                        tick.index('else if (edge(buttons, kButtonB))'))
        self.assertIn('sPreviousButtons = buttons;', tick)
        self.assertIn('u16 pressed = buttons & ~sPreviousButtons;', self.input)
        self.assertIn('LMState::samplePad(statuses[0], !sOpen);', self.source)
        self.assertNotIn('sOpen = false', self.input + self.begin)
        filter_pad = self.source.split('void filterPadRead(', 1)[1].split('void tick()', 1)[0]
        for field in ('mButton', 'mStickX', 'mStickY', 'mTriggerLeft', 'mTriggerRight'):
            self.assertIn(f'pad->{field} = 0u;', filter_pad)

    def test_export_and_rename_require_confirmed_draft_not_entry(self):
        self.assertIn('beginNameKeyboard(NameAction::Export, 0u, nullptr)', self.source)
        self.assertNotIn('requestExport(', self.begin)
        self.assertNotIn('requestRename(', self.begin)
        self.assertIn('LMNameKeyboard::begin(sNameDraft, name)', self.begin)
        self.assertIn('sNameArchiveId = archiveId;', self.begin)
        commit = self.input.split('result == LMNameKeyboard::Commit', 1)[1]
        self.assertIn('LMState::requestRename(sNameArchiveId, sNameDraft.text)', commit)
        self.assertIn('LMState::requestExport(sNameDraft.text)', commit)
        self.assertEqual(self.source.count('LMState::requestExport('), 1)
        self.assertEqual(self.source.count('LMState::requestRename('), 1)

    def test_inflight_input_is_blocked_and_result_is_not_assumed_success(self):
        pending = self.input.split('if (sNamePending)', 1)[1].split('u16 pressed', 1)[0]
        self.assertTrue(pending.lstrip().startswith('{\n        if (LMState::storageBusy()) return;'))
        self.assertIn('sNameAction = NameAction::None;', pending)
        self.assertIn('showNotice(LMState::storageText());', pending)
        self.assertIn('if (renamed) browseArchives(LMState::catalogCursor());', pending)
        refusal = self.input.split('if (!sNamePending)', 1)[1]
        self.assertIn('showNotice(LMState::storageText());', refusal)
        self.assertNotIn('sNameAction = NameAction::None', refusal)
        for text in ('RENAMED', 'EXPORTED', 'strstr', 'strcmp'):
            self.assertNotIn(text, self.input)

    def test_discard_and_clear_cannot_replace_or_destroy_memory_state(self):
        for forbidden in ('requestImport(', 'requestSave(', 'requestLoad(', 'selectSlot(',
                          'discardState(', 'clearState(', 'memcpy(', 'memset('):
            self.assertNotIn(forbidden, self.begin + self.input)
        discard = self.input.split('result == LMNameKeyboard::Discard', 1)[1].split('else if', 1)[0]
        self.assertIn('sNameAction = NameAction::None;', discard)
        self.assertNotIn('LMState::', discard)
        self.assertIn('NAME CHANGES DISCARDED', discard)
        self.assertIn('sNameDraft.confirmation == LMNameKeyboard::NoPrompt', self.input)
        self.assertIn('!(buttons & kButtonStart)', self.input)

    def test_rename_uses_selected_actual_id_independent_of_import_compatibility(self):
        rename = self.browser.split('edge(buttons, kButtonX)', 1)[1].split('else if', 1)[0]
        self.assertIn('NameAction::Rename, LMState::catalogId(sArchiveSelection)', rename)
        self.assertIn('LMState::catalogName(sArchiveSelection)', rename)
        self.assertNotIn('catalogCompatible', rename)
        self.assertIn('if (LMState::storageBusy()) return;', self.browser)
        self.assertIn('edge(buttons, kButtonY)', self.browser)
        self.assertIn('if (!LMState::catalogCompatible(sArchiveSelection))', self.browser)
        self.assertIn('if (confirm(0x53440000u ^ id))', self.browser)

    def test_user_names_are_bounded_data_not_format_strings(self):
        self.assertIn('"%s_", sNameDraft.text', self.source)
        self.assertIn('"%s %.31s",', self.source)
        self.assertIn('"%s", one', self.source)
        self.assertIn('if (name && name[0])', self.source)
        self.assertIn('"%s Archive %08lu"', self.source)
        self.assertIn('"%08lu  %5luK  %s"', self.source)
        self.assertIn('"A: Import  X: Rename  Z: Delete  Y: Refresh"', self.source)

    def test_keyboard_tiles_labels_and_confirmation_fit_visible_panel(self):
        draw = self.source.split('void drawNameKeyboard(', 1)[1].split('void archiveInput(', 1)[0]
        self.assertIn('29u + (i % 8u) * 34u, y = 100u + (i / 8u) * 18u', draw)
        for index in range(32):
            x, y = 29 + (index % 8) * 34, 100 + (index // 8) * 18
            self.assertLessEqual(x + 24, 314)
            self.assertLessEqual(y + 16, 180)
        for x, y, text in (
            (21, 75, 'X' * 31 + '_'),
            (13, 180, 'D-pad: Select  A: Type  B: Delete  X: Space'),
            (13, 192, 'Y: Case   L/R: Page   Z: Clear'),
            (13, 204, 'START: Keep   X+START: Discard changes'),
            (13, 222, 'Empty name: show archive ID. Memory unchanged.'),
            (45, 146, 'Please wait - do not remove the SD.'),
            (45, 133, 'Memory state stays unchanged.'),
            (45, 152, 'A: Confirm    B: Go back'),
        ):
            self.assertLessEqual(x + len(text) * 6, 314)
            self.assertLessEqual(y + 7, 233)
        self.assertIn('LMDraw::flush(xfb, 640u, 480u, kMenuTop, kArchivePanelHeight);', draw)


if __name__ == '__main__': unittest.main()
