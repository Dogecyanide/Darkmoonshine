"""Run the portable Moonshine Creation input/undo regression harness."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CreationTests(unittest.TestCase):
    def run_editor(self,case):
        compiler = ROOT / 'toolchain/clang++.exe'
        if not compiler.exists():
            self.skipTest('Bundled Windows clang is required')
        with tempfile.TemporaryDirectory(prefix='lm-creation-') as directory:
            exe = Path(directory) / 'creation.exe'
            subprocess.run([str(compiler), '--target=x86_64-pc-windows-msvc',
                            '-fuse-ld=lld', '-nostdlib', '-fno-stack-protector',
                            f'-DLM_CREATION_CASE={case}',
                            '-Wl,/entry:main,/subsystem:console',
                            str(ROOT / 'scripts/test_lm_creation.cpp'), '-o', str(exe)],
                           check=True, capture_output=True, text=True)
            self.assertEqual(subprocess.run([str(exe)]).returncode, 0,
                             'Nonzero status names the failed C++ CHECK line')

    def test_editor(self): self.run_editor(0)
    def test_precise_taps_and_short_holds(self): self.run_editor(1)
    def test_cross_screen_in_under_three_seconds_at_30_hz(self): self.run_editor(2)
    def test_direction_reversal_resets_acceleration(self): self.run_editor(3)
    def test_axes_and_colour_repeats_are_independent(self): self.run_editor(4)
    def test_bounds_and_opposing_directions(self): self.run_editor(5)
    def test_confirmations_and_new_editor_reset_acceleration(self): self.run_editor(6)
    def test_disconnect_and_release_stop_immediately(self): self.run_editor(7)
    def test_acceleration_thresholds_and_saturated_counter(self): self.run_editor(8)
    def test_scale_repeat_is_not_accelerated(self): self.run_editor(9)


if __name__ == '__main__':
    unittest.main()
