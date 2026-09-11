"""Native cache-ownership and bounds tests for the LM-only crash telemetry."""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CrashTelemetryNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-crash-telemetry-")
        output = Path(cls.temp.name) / ("telemetry.dll" if os.name == "nt" else "telemetry.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"), str(ROOT / "scripts/lm_crash_telemetry_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.telemetry_case.argtypes = [ctypes.c_uint]
        cls.lib.telemetry_case.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def case(self, index):
        self.assertEqual(self.lib.telemetry_case(index), 0, "native harness failure line")

    def test_old_kernel_and_old_ppc_fallback(self): self.case(0)
    def test_full_queue_preserves_all_unacknowledged_events(self): self.case(1)
    def test_repeated_ring_reuse_and_cache_line_ownership(self): self.case(2)
    def test_counter_wrap(self): self.case(3)
    def test_torn_controls_records_and_invalid_actions(self): self.case(4)
    def test_saturating_overflow_counter(self): self.case(5)
    def test_exact_pc_object_context_and_canaries(self): self.case(6)
    def test_null_cross_boundary_and_io_pointers_are_not_followed(self): self.case(7)
    def test_save_anchor_survives_immediate_post_load_trace_burst(self): self.case(8)
    def test_grain_source_and_copy_fault_evidence_survives_draw_trace(self): self.case(9)
    def test_validation_faults_do_not_collide_with_audio_trace(self): self.case(10)
    def test_camera_and_reboot_profile_diagnostics_are_durable(self): self.case(11)
    def test_unknown_volume_identity_survives_postload_trace(self): self.case(12)


if __name__ == "__main__":
    unittest.main()
