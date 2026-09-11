"""Execute the production shared companion and miniz against private MEM1 data.

The actual .inc is included without rewriting it. Only addresses, outer snapshot
header, native cache flush, and big-endian MEM1 reads are host adapters. This is
not a test of GPU/audio timing or a claim that arbitrary gameplay loads are safe.
"""

import ctypes
import hashlib
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "build-lm-emu/diagnostic-capture-0.3.30/mem1.bin"
CORE = 0x20000


class SharedMailboxLayoutTests(unittest.TestCase):
    def test_adaptive_companion_requires_new_kind_and_snapshot_format(self):
        shared = (ROOT / "lm_diag/src/lm_state_shared.inc").read_text()
        state = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        self.assertRegex(shared, r"kSharedCompanionKind\s*=\s*2u")
        self.assertRegex(state, r"kSnapshotVersion\s*=\s*28u")
        self.assertIn("LmStateDeflateFast(", shared)
        self.assertIn("LmStateDeflate(", shared)
        self.assertLess(shared.index("LmStateDeflateFast("), shared.index("LmStateDeflate("))

    def test_both_codec_paths_use_the_complete_mailbox_reservation(self):
        for filename, prefix in (("lm_state_shared.inc", "kShared"),
                                 ("lm_state_storage.inc", "kSlot")):
            source = (ROOT / "lm_diag/src" / filename).read_text()
            workspace = "kSharedCodecWorkspace" if prefix == "kShared" else "kCodecWorkspace"
            with self.subTest(source=filename):
                self.assertRegex(source, rf"constexpr u32 {workspace}\s*=\s*"
                                 r"SUSAMUNE_LM_CACHE_PPC_BASE\s*\+\s*SUSAMUNE_LM_MAILBOX_SIZE\s*;")
                self.assertRegex(source, rf"constexpr u32 {prefix}StagingStart\s*=\s*"
                                 rf"{workspace}\s*\+\s*LM_STATE_DEFLATE_WORKSPACE\s*;")
                self.assertRegex(source, rf"constexpr u32 {prefix}StagingSize\s*=\s*"
                                 r"SUSAMUNE_LM_CACHE_SIZE\s*-\s*SUSAMUNE_LM_MAILBOX_SIZE\s*"
                                 r"-\s*LM_STATE_DEFLATE_WORKSPACE\s*;")
                self.assertIn("sizeof(LmStateStorageMailbox) <= SUSAMUNE_LM_MAILBOX_SIZE", source)

    def test_no_lm_codec_workspace_retains_the_old_literal_mailbox_offset(self):
        for path in (ROOT / "lm_diag/src").glob("lm_state*"):
            if path.suffix not in (".cpp", ".inc"):
                continue
            source = path.read_text()
            self.assertIsNone(re.search(r"SUSAMUNE_LM_CACHE_(?:PPC_BASE|SIZE)\s*[+-]\s*"
                                        r"(?:0x0*200[uUlL]*|512[uUlL]*)\b", source), path.name)


@unittest.skipUnless(os.name == "nt", "Low-address Windows companion harness")
class SharedCompanionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not FIXTURE.exists():
            raise unittest.SkipTest("Optional private MEM1 ownership/archive fixture unavailable")
        compiler = shutil.which("g++") or "C:/msys64/mingw64/bin/g++.exe"
        if not Path(compiler).exists():
            raise unittest.SkipTest("Native MinGW C++ compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-companion-")
        output = Path(cls.temp.name) / "companion.dll"
        command = [compiler, "-shared", "-O2", "-std=c++17", "-static-libgcc", "-static-libstdc++",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_shared_companion_harness.cpp"),
                   str(ROOT / "lm_diag/src/lm_state_deflate.cpp"), "-o", str(output)]
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            cls.temp.cleanup()
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.dll_directory = os.add_dll_directory(str(Path(compiler).parent))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.companion_reset.argtypes = [ctypes.c_void_p, ctypes.c_uint]
        for name in ("companion_stage", "companion_commit", "companion_make",
                     "companion_stored", "companion_metric", "companion_codec_size"):
            getattr(cls.lib, name).restype = ctypes.c_uint
        cls.fixture = FIXTURE.read_bytes()
        cls.fixture_buffer = ctypes.create_string_buffer(cls.fixture)

    @classmethod
    def tearDownClass(cls):
        cls.lib.companion_shutdown()
        from _ctypes import FreeLibrary
        FreeLibrary(cls.lib._handle)
        cls.dll_directory.close()
        cls.temp.cleanup()

    def setUp(self):
        self.assertEqual(self.lib.companion_reset(self.fixture_buffer, len(self.fixture)), 1)
        self.snapshot = self.lib.companion_metric(0)
        self.limit = self.lib.companion_metric(1)
        self.parent = self.lib.companion_metric(2)
        self.parent_size = self.lib.companion_metric(3)

    def word(self, address, value=None):
        slot = ctypes.c_uint.from_address(address)
        if value is not None:
            slot.value = value
        return slot.value

    def digest(self, address, size):
        return hashlib.sha256(ctypes.string_at(address, size)).digest()

    def sys_digest(self):
        return self.digest(self.lib.companion_metric(4), self.lib.companion_metric(5))

    def make(self):
        self.size = self.lib.companion_make(CORE, 17, 0x12345678)
        self.assertGreater(self.size, CORE + 64)
        self.descriptor = self.snapshot + CORE
        self.packed = self.word(self.descriptor + 16)
        self.payload = self.descriptor + 64
        self.assertEqual(self.lib.companion_stored(), self.size)
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_real_archive_round_trip_restores_only_parent_and_flushes_live_target(self):
        before = ctypes.string_at(self.parent, self.parent_size)
        self.make()
        metadata_end = 0x6E20
        ctypes.memset(self.parent + metadata_end, 0x37, self.parent_size - metadata_end)
        self.assertEqual(self.lib.companion_matches(), 1)
        self.assertEqual(self.lib.companion_restore(1), 1)
        self.assertEqual(ctypes.string_at(self.parent, self.parent_size), before)
        self.assertEqual((self.lib.companion_metric(10), self.lib.companion_metric(11),
                          self.lib.companion_metric(12)), (1, self.parent, self.parent_size))
        self.assertEqual(ctypes.string_at(0x80000000, len(self.fixture)), self.fixture)

    def test_real_codec_stage_decode_restore_preserve_every_mailbox_cache_line(self):
        base, reserved, workspace, actual, codec_size, cache_size = (
            self.lib.companion_metric(i) for i in range(16, 22))
        self.assertEqual(actual, 928)
        self.assertEqual(reserved, 0x400)
        self.assertLessEqual(actual, reserved)
        self.assertEqual(workspace, base + reserved)
        self.assertEqual(workspace % 32, 0)
        self.assertEqual(self.lib.companion_metric(6), workspace + codec_size)
        self.assertEqual(self.lib.companion_metric(6) + self.lib.companion_metric(7), base + cache_size)
        marker = bytes((i * 31 + 17) & 255 for i in range(reserved))
        ctypes.memmove(base, marker, len(marker))
        self.make()
        self.assertEqual(ctypes.string_at(base, reserved), marker)
        self.assertEqual(self.lib.companion_valid(self.size), 1)
        self.assertEqual(ctypes.string_at(base, reserved), marker)
        self.assertEqual(self.lib.companion_restore(1), 1)
        self.assertEqual(ctypes.string_at(base, reserved), marker)

    def test_stage_capacity_failure_preserves_every_old_snapshot_byte(self):
        self.make()
        old = self.digest(self.snapshot, self.limit)
        sys_before = self.sys_digest()
        for size in (self.limit - 64, self.limit - 80, self.limit, 0xFFFFFFFF):
            self.assertEqual(self.lib.companion_stage(size), 0)
            self.assertEqual(self.digest(self.snapshot, self.limit), old)
            self.assertEqual(self.sys_digest(), sys_before)
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_large_incompressible_stage_does_not_overrun_workspace_or_old_slot(self):
        self.make()
        old = self.digest(self.snapshot, self.limit)
        # Preserve validated metadata; randomized file data exceeds 3.68 MiB staging.
        payload = self.parent + 0x6E20
        random_data = hashlib.shake_256(b"synthetic companion capacity test").digest(
            self.parent_size - 0x6E20)
        ctypes.memmove(payload, random_data, len(random_data))
        sys_before = self.sys_digest()
        self.assertEqual(self.lib.companion_stage(CORE), 0)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        self.assertEqual(self.sys_digest(), sys_before)

    def test_exact_capacity_uses_dense_fallback_then_refuses_below_dense_limit(self):
        self.make()
        old = self.digest(self.snapshot, self.limit)
        before = self.sys_digest()
        quick = self.lib.companion_codec_size(1)
        dense = self.lib.companion_codec_size(0)
        self.assertEqual(self.packed, quick)
        self.assertEqual(ctypes.string_at(self.payload, 4), b"LML4")
        self.assertLess(dense, quick)
        quick_rounded = (quick + 31) & ~31
        dense_rounded = (dense + 31) & ~31
        exact_quick_core = self.limit - 64 - quick_rounded
        self.assertEqual(self.lib.companion_stage(exact_quick_core), quick)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        # One aligned unit below QUICK's requirement is not failure: use dense.
        self.assertEqual(self.lib.companion_stage(exact_quick_core + 32), dense)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        exact_dense_core = self.limit - 64 - dense_rounded
        self.assertEqual(self.lib.companion_stage(exact_dense_core + 32), 0)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        self.assertEqual(self.sys_digest(), before)
        # Misaligned core lengths must fail before either codec or commit.
        self.assertEqual(self.lib.companion_stage(exact_dense_core + 1), 0)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        self.assertEqual(self.lib.companion_make(exact_dense_core, 23, 0xDEADBEEF), self.limit)
        self.assertNotEqual(ctypes.string_at(self.snapshot + exact_dense_core + 64, 4), b"LML4")
        self.assertEqual(self.lib.companion_stored(), self.limit)
        self.assertEqual(self.lib.companion_valid(self.limit), 1)
        self.assertEqual(self.lib.companion_restore(1), 1)
        self.assertEqual(self.sys_digest(), before)

    def test_invalid_live_owner_refuses_stage_without_touching_old_snapshot(self):
        self.make()
        old = self.digest(self.snapshot, self.limit)
        ctypes.memset(0x804A12B0, 0, 4)
        before = self.sys_digest()
        self.assertEqual(self.lib.companion_stage(CORE), 0)
        self.assertNotEqual(self.lib.companion_metric(13), 0)
        self.assertEqual(self.digest(self.snapshot, self.limit), old)
        self.assertEqual(self.sys_digest(), before)

    def test_core_size_bounds_are_checked_before_descriptor_access(self):
        self.make()
        for value in (0, 0x17A20, CORE + 1, self.limit, 0xFFFFFFFF, 0xFFFFFFE0):
            self.word(self.snapshot, value)
            self.assertEqual(self.lib.companion_stored(), 0, hex(value))
            self.assertEqual(self.lib.companion_valid(self.size), 0)
        self.word(self.snapshot, CORE)
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_descriptor_bounds_generation_core_and_heap_binding(self):
        self.make()
        fields = {0: (0, 0xFFFFFFFF), 4: (0, 1, 3), 8: (0, 18), 12: (0, 0x12345679),
                  16: (0, self.limit, 0xFFFFFFFF), 32: (0, self.parent_size + 1),
                  36: (0, 0xFFFFFFFF), 40: (0, 0xFFFFFFFF), 44: (0, 0xFFFFFFFF)}
        before = self.sys_digest()
        for offset, values in fields.items():
            original = self.word(self.descriptor + offset)
            for value in values:
                with self.subTest(offset=offset, value=value):
                    self.word(self.descriptor + offset, value)
                    self.assertEqual(self.lib.companion_stored(), 0)
                    self.assertEqual(self.lib.companion_valid(self.size), 0)
            self.word(self.descriptor + offset, original)
        for offset in (4, 8, 12, 16, 20):
            original = self.word(self.snapshot + offset)
            self.word(self.snapshot + offset, original ^ 4)
            self.assertEqual(self.lib.companion_stored(), 0)
            self.word(self.snapshot + offset, original)
        self.assertEqual(self.sys_digest(), before)
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_checksum_and_zero_padding_are_mandatory(self):
        self.make()
        before = self.sys_digest()
        for offset in (20, 24, 48, 52, 56, 60, 64, 64 + self.packed - 1):
            byte = ctypes.c_ubyte.from_address(self.descriptor + offset)
            byte.value ^= 1
            self.assertEqual(self.lib.companion_valid(self.size), 0, offset)
            byte.value ^= 1
        padding_start = self.payload + self.packed
        self.assertLess(padding_start, self.snapshot + self.size)
        ctypes.c_ubyte.from_address(padding_start).value = 1
        self.assertEqual(self.lib.companion_valid(self.size), 0)
        ctypes.c_ubyte.from_address(padding_start).value = 0
        for size in (0, self.size - 1, self.size + 32):
            self.assertEqual(self.lib.companion_valid(size), 0)
        self.assertEqual(self.sys_digest(), before)
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_corrupt_stream_with_recomputed_wrapper_crc_never_writes_sys(self):
        self.make()
        ctypes.memset(self.payload, 0, 2)
        self.lib.companion_rechecksum()
        before = self.sys_digest()
        self.assertEqual(self.lib.companion_stored(), self.size)
        self.assertEqual(self.lib.companion_valid(self.size), 0)
        self.assertEqual(self.lib.companion_restore(1), 0)
        self.assertEqual(self.sys_digest(), before)
        self.assertEqual(self.lib.companion_metric(10), 0)

    def test_quick_trailer_adler_is_checked_even_when_wrapper_crc_is_recomputed(self):
        self.make()
        self.assertEqual(ctypes.string_at(self.payload, 4), b"LML4")
        before = self.sys_digest()
        last = ctypes.c_ubyte.from_address(self.payload + self.packed - 1)
        last.value ^= 0x80
        self.lib.companion_rechecksum()
        self.assertEqual(self.lib.companion_stored(), self.size)
        self.assertEqual(self.lib.companion_valid(self.size), 0)
        self.assertEqual(self.lib.companion_restore(1), 0)
        self.assertEqual(self.sys_digest(), before)
        self.assertEqual(self.lib.companion_metric(10), 0)

    def test_malformed_quick_block_header_never_reaches_a_live_destination(self):
        self.make()
        self.assertEqual(ctypes.string_at(self.payload, 8), b"LML4\0\x02\0\0")
        before = self.sys_digest()
        fields = {
            0: (0, 0x4C4D4C35),
            4: (0, 0x1FFFF, 0x40000, 0xFFFFFFFF),
            8: (0, 1, 0x1FFFF, 0x20001, 0xFFFFFFFF),
            12: (0, 0x20000, 0x7FFFFFFF, 0x80000001, 0xFFFFFFFF),
        }
        for offset, values in fields.items():
            original = ctypes.string_at(self.payload + offset, 4)
            for value in values:
                with self.subTest(offset=offset, value=value):
                    ctypes.memmove(self.payload + offset, struct.pack(">I", value), 4)
                    self.lib.companion_rechecksum()
                    self.assertEqual(self.lib.companion_stored(), self.size)
                    self.assertEqual(self.lib.companion_valid(self.size), 0)
                    self.assertEqual(self.lib.companion_restore(1), 0)
                    self.assertEqual(self.sys_digest(), before)
                    self.assertEqual(self.lib.companion_metric(10), 0)
            ctypes.memmove(self.payload + offset, original, 4)
        self.lib.companion_rechecksum()
        self.assertEqual(self.lib.companion_valid(self.size), 1)

    def test_quick_stream_does_not_accept_appended_bytes_as_padding(self):
        self.make()
        before = self.sys_digest()
        appended = self.packed + 1
        rounded = (appended + 31) & ~31
        ctypes.memset(self.payload + self.packed, 0, rounded - self.packed)
        self.word(self.descriptor + 16, appended)
        self.lib.companion_rechecksum()
        stored = CORE + 64 + rounded
        self.assertEqual(self.lib.companion_stored(), stored)
        self.assertEqual(self.lib.companion_valid(stored), 0)
        self.assertEqual(self.sys_digest(), before)

    def test_valid_zlib_with_wrong_output_size_fails_dry_decode(self):
        self.make()
        packed = zlib.compress(b"too short")
        ctypes.memmove(self.payload, packed, len(packed))
        rounded = (len(packed) + 31) & ~31
        ctypes.memset(self.payload + len(packed), 0, rounded - len(packed))
        self.word(self.descriptor + 16, len(packed))
        self.lib.companion_rechecksum()
        stored = CORE + 64 + rounded
        before = self.sys_digest()
        self.assertEqual(self.lib.companion_stored(), stored)
        self.assertEqual(self.lib.companion_valid(stored), 0)
        self.assertEqual(self.sys_digest(), before)

    def test_truncated_stream_with_recomputed_wrapper_crc_fails_dry_decode(self):
        self.make()
        shortened = self.packed - 1
        self.word(self.descriptor + 16, shortened)
        rounded = (shortened + 31) & ~31
        ctypes.memset(self.payload + shortened, 0, rounded - shortened)
        self.lib.companion_rechecksum()
        stored = CORE + 64 + rounded
        before = self.sys_digest()
        self.assertEqual(self.lib.companion_stored(), stored)
        self.assertEqual(self.lib.companion_valid(stored), 0)
        self.assertEqual(self.sys_digest(), before)

    def test_file_selected_target_is_refused_by_live_identity_matching(self):
        self.make()
        self.word(self.descriptor + 28, 0x81000000)
        self.lib.companion_rechecksum()
        before = self.sys_digest()
        self.assertEqual(self.lib.companion_valid(self.size), 1)
        self.assertEqual(self.lib.companion_matches(), 0)
        self.assertEqual(self.lib.companion_restore(1), 0)
        self.assertEqual(self.sys_digest(), before)
        self.assertEqual(self.lib.companion_metric(10), 0)

    def test_restore_helper_uses_trusted_live_base_not_saved_base(self):
        self.make()
        foreign = 0x81000000
        self.word(self.descriptor + 28, foreign)
        self.lib.companion_rechecksum()
        foreign_before = self.digest(foreign, self.parent_size)
        ctypes.memset(self.parent + 0x6E20, 0x19, self.parent_size - 0x6E20)
        # Isolated helper contract: skip equality, but still validate live owner.
        self.assertEqual(self.lib.companion_restore(0), 1)
        self.assertEqual(self.lib.companion_metric(11), self.parent)
        self.assertEqual(self.digest(foreign, self.parent_size), foreign_before)
        self.assertEqual(ctypes.string_at(0x80000000, len(self.fixture)), self.fixture)


if __name__ == "__main__":
    unittest.main()
