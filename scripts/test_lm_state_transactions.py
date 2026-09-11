"""Execute the real storage client, deflate/auth and ARM worker together.

Only PPC instructions/cache calls and the surrounding game snapshot fixture are
host shims. All request, receipt, rollback and archive/catalog code is production.
This does not model Wii cache coherency or certify a gameplay state safe to load.
Persistent profiles below are synthetic wire fixtures, not native-owner proof.
"""
from __future__ import annotations

import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name == "nt", "Low-address Windows transaction harness")
class LmStateTransactionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("g++") or "C:/msys64/mingw64/bin/g++.exe"
        if not Path(compiler).exists():
            raise unittest.SkipTest("Native MinGW C++ compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-transactions-")
        folder = Path(cls.temp.name)
        source = (ROOT / "lm_diag/src/lm_state_storage.inc").read_text()
        substitutions = {
            'asm volatile("sync" ::: "memory");': '(void)0;',
            'asm volatile("mftbu %0\\n\\tmftb %1" : "=r"(high), "=r"(low));': 'high = 1; low = 2;',
            'asm volatile("mftb %0" : "=r"(later));': 'later = 3;',
        }
        for old, new in substitutions.items():
            if old not in source:
                raise RuntimeError(f"PPC host-shim anchor changed: {old}")
            source = source.replace(old, new)
        (folder / "lm_storage_under_test.inc").write_text(source)
        output = folder / "client.dll"
        command = [compiler, "-shared", "-O2", "-std=c++17", "-static-libgcc", "-static-libstdc++",
                   "-I", str(ROOT / "include"), "-I", str(ROOT / "launcher/fatfs"),
                   "-I", str(folder), str(ROOT / "scripts/lm_state_client_harness.cpp"),
                   str(ROOT / "lm_diag/src/lm_state_deflate.cpp"), "-o", str(output)]
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            cls.temp.cleanup()
            raise RuntimeError(result.stderr.decode(errors="replace")[-5000:])
        cls.dll_directory = os.add_dll_directory(str(Path(compiler).parent))
        cls.lib = ctypes.CDLL(str(output))
        for name in ("client_text", "client_catalog_text", "client_catalog_entry_text", "client_catalog_name"):
            getattr(cls.lib, name).restype = ctypes.c_char_p
        cls.lib.client_start_named.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_char_p]
        cls.lib.client_file_crc.restype = ctypes.c_uint
        cls.lib.client_add_file.argtypes = [ctypes.c_char_p]
        cls.lib.client_slot_crc.restype = ctypes.c_uint
        cls.lib.client_payload_crc.restype = ctypes.c_uint
        cls.lib.client_named_size.argtypes = [ctypes.c_char_p]
        cls.lib.client_named_bytes.argtypes = [ctypes.c_char_p]
        cls.lib.client_named_bytes.restype = ctypes.c_void_p
        cls.lib.client_put_named.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        cls.lib.client_remove_named.argtypes = [ctypes.c_char_p]
        for name in ("client_key_id", "client_config_id", "client_profile_crc"):
            getattr(cls.lib, name).restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        cls.lib.client_shutdown()
        from _ctypes import FreeLibrary
        FreeLibrary(cls.lib._handle)
        cls.dll_directory.close()
        cls.temp.cleanup()

    def setUp(self):
        self.assertEqual(self.lib.client_reset(), 1)

    def drain(self):
        for _ in range(4096):
            if not self.lib.client_pending():
                return
            self.lib.client_step()
        self.fail("Storage transaction did not terminate")

    def run_command(self, command, ident):
        self.assertEqual(self.lib.client_start(command, ident), 1)
        self.drain()
        return self.lib.client_text().decode()

    def export(self, ident=1, marker=11):
        self.lib.client_save(marker)
        self.assertIn("EXPORTED", self.run_command(1, ident))

    def assert_old_kept(self, text, marker=22):
        self.assertIn("STATE KEPT", text)
        self.assertLess(len(text), 48)
        self.assertNotIn("READY - PRESS LOAD", text)
        self.assertEqual(self.lib.client_marker(), marker)
        self.assertEqual(self.lib.client_core_size(), 50000)
        self.assertGreater(self.lib.client_size(), 50000)

    def test_success_only_after_authenticated_transfer(self):
        self.export()
        self.lib.client_save(22)
        self.assertEqual(self.lib.client_start(2, 1), 1)
        self.assertIn(b"WORKING", self.lib.client_text())
        self.drain()
        self.assertEqual(self.lib.client_marker(), 11)
        self.assertEqual(self.lib.client_text(), b"IMPORT 00000001: READY - PRESS LOAD")

    def test_missing_file_preserves_old_state_and_requested_identity(self):
        self.export()
        self.lib.client_save(22)
        text = self.run_command(2, 99)
        self.assertIn("IMPORT 00000099: NOT FOUND", text)
        self.assert_old_kept(text)
        self.assertEqual(self.lib.client_archive(), 1)

    def test_missing_file_with_empty_slot_stays_empty(self):
        self.export()
        self.lib.client_empty()
        text = self.run_command(2, 99)
        self.assertIn("NOT FOUND (SLOT EMPTY)", text)
        self.assertEqual(self.lib.client_size(), 0)

    def test_old_session_rejected_before_any_payload_transfer(self):
        self.export()
        self.lib.client_save(22)
        self.lib.client_session()
        text = self.run_command(2, 1)
        self.assertIn("DIFFERENT SESSION", text)
        self.assert_old_kept(text)
        self.assertEqual(self.lib.client_io_calls(2), 1)  # Header only.

    def test_corruption_auth_and_build_failures_roll_back(self):
        for offset, expected in ((24, "DIFFERENT BUILD"), (48, "AUTH ERROR"), (364, "CRC ERROR")):
            with self.subTest(offset=offset):
                self.setUp()
                self.export()
                self.lib.client_save(22)
                self.lib.client_corrupt_file(1, offset)
                text = self.run_command(2, 1)
                self.assertIn(expected, text)
                self.assert_old_kept(text)

    def test_partial_payload_read_rolls_back(self):
        self.export()
        self.lib.client_save(22)
        self.lib.client_fault(2, 3)
        text = self.run_command(2, 1)
        self.assertIn("FILE ERROR", text)
        self.assert_old_kept(text)

    def test_busy_attempt_replaces_stale_success_text(self):
        self.export()
        self.run_command(2, 1)
        self.lib.client_busy(1)
        self.assertEqual(self.lib.client_start(2, 99), 0)
        self.assertEqual(self.lib.client_text(), b"IMPORT 00000099: BUSY - NOT STARTED")

    def test_mismatched_receipts_never_report_success(self):
        for field in range(7):
            with self.subTest(field=field):
                self.setUp()
                self.export()
                self.lib.client_save(22)
                self.assertEqual(self.lib.client_start(2, 1), 1)
                self.lib.client_finish_worker()
                self.lib.client_bad_receipt(field)
                text = self.lib.client_text().decode()
                self.assertIn("BAD RESPONSE", text)
                self.assert_old_kept(text)

    def test_catalog_sort_pages_strict_names_and_snapshot_unchanged(self):
        for ident in (40, 3, 15, 2, 30, 19, 11, 42, 5, 28):
            self.export(ident)
        for name in ("archive_00000000.lms", "archive_00000001.tmp", "archive_1.lms",
                     "archive_00000006.LMS", "archive_00000007.lms.bak", "arCHive_00000008.lms"):
            self.lib.client_add_file(("/lm_states/" + name).encode())
        self.lib.client_save(22)
        old_crc = self.lib.client_slot_crc()
        old_text = self.lib.client_text()
        self.run_command(3, 0)
        ids = [self.lib.client_catalog_id(i) for i in range(self.lib.client_catalog_count())]
        self.assertEqual(ids, [2, 3, 5, 11, 15, 19, 28, 30])
        self.assertEqual(self.lib.client_catalog_more(), 1)
        self.assertEqual(self.lib.client_catalog_next(), 30)
        self.assertEqual(self.lib.client_catalog_bytes(0),
                         self.lib.client_size() + 64 + self.lib.client_trailer_size())
        self.assertEqual(self.lib.client_catalog_compatible(0), 1)
        self.assertEqual(self.lib.client_catalog_entry_text(0), b"SAME SESSION")
        self.run_command(3, 30)
        self.assertEqual([self.lib.client_catalog_id(i) for i in range(self.lib.client_catalog_count())], [40, 42])
        self.assertEqual(self.lib.client_catalog_more(), 0)
        self.assertEqual(self.lib.client_slot_crc(), old_crc)
        self.assertEqual(self.lib.client_text(), old_text)
        self.assertEqual(self.lib.client_directory_open(), 0)

    def test_catalog_marks_bad_and_old_session_files(self):
        self.export()
        self.lib.client_add_file(b"/lm_states/archive_00000002.lms")
        self.lib.client_session()
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_entry_text(0), b"DIFFERENT SESSION")
        self.assertEqual(self.lib.client_catalog_entry_text(1), b"INVALID FILE")
        self.assertEqual(self.lib.client_catalog_compatible(0), 0)
        self.assertEqual(self.lib.client_catalog_compatible(1), 0)

    def test_catalog_sliced_and_session_abort_closes_directory(self):
        for ident in range(35):
            self.lib.client_add_file(f"/lm_states/ignore_{ident:08d}.tmp".encode())
        self.assertEqual(self.lib.client_start(3, 0), 1)
        for _ in range(3):
            self.lib.client_step()
        self.assertEqual(self.lib.client_directory_open(), 1)
        self.assertEqual(self.lib.client_io_calls(8), 16)
        self.lib.client_session()
        self.drain()
        self.assertEqual(self.lib.client_directory_open(), 0)
        self.assertIn(self.lib.client_catalog_text(), (b"CANCELLED", b"BAD RESPONSE"))

    def test_catalog_io_failure_keeps_import_error(self):
        self.lib.client_save(22)
        self.run_command(2, 99)
        text = self.lib.client_text()
        self.lib.client_fault(8, 1)
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_text(), b"FILE ERROR")
        self.assertEqual(self.lib.client_directory_open(), 0)
        self.assertEqual(self.lib.client_text(), text)

    def test_malformed_catalog_response_has_no_selectable_rows(self):
        for field in range(11):
            with self.subTest(field=field):
                self.setUp()
                self.export()
                self.assertEqual(self.lib.client_start(3, 0), 1)
                self.lib.client_finish_worker()
                self.lib.client_bad_catalog(field)
                self.assertEqual(self.lib.client_catalog_text(), b"BAD RESPONSE")
                self.assertEqual(self.lib.client_catalog_count(), 0)

    def test_catalog_open_read_and_close_failures_release_handles(self):
        for operation in (1, 2, 5, 7, 8, 9):
            with self.subTest(operation=operation):
                self.setUp()
                self.export()
                self.lib.client_fault(operation, self.lib.client_io_calls(operation) + 1)
                self.run_command(3, 0)
                self.assertEqual(self.lib.client_catalog_text(), b"FILE ERROR")
                self.assertEqual(self.lib.client_catalog_count(), 0)
                self.assertEqual(self.lib.client_directory_open(), 0)

    def test_valid_archive_into_empty_slot_and_empty_catalog(self):
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_count(), 0)
        self.assertEqual(self.lib.client_catalog_text(), b"NO ARCHIVES FOUND")
        self.export()
        self.lib.client_empty()
        self.assertIn("READY - PRESS LOAD", self.run_command(2, 1))
        self.assertEqual(self.lib.client_marker(), 11)

    def test_single_slot_profile(self):
        self.assertEqual(self.lib.client_slots(), 1)
        self.assertEqual(self.lib.client_switch(0), 1)
        self.assertEqual(self.lib.client_switch(1), 0)

    def test_different_core_and_companion_sizes_keep_trailer_at_stored_extent(self):
        for old_core, new_core in ((50000, 200000), (200000, 50000)):
            with self.subTest(old=old_core, new=new_core):
                self.setUp()
                self.assertEqual(self.lib.client_save_profile(11, old_core, 200000, 0), 1)
                expected_crc = self.lib.client_payload_crc()
                expected_size = self.lib.client_size()
                self.assertIn("EXPORTED", self.run_command(1, 1))
                self.assertEqual(self.lib.client_save_profile(22, new_core, 400000, 0), 1)
                self.assertIn("READY - PRESS LOAD", self.run_command(2, 1))
                self.assertEqual(self.lib.client_core_size(), old_core)
                self.assertEqual(self.lib.client_size(), expected_size)
                self.assertEqual(self.lib.client_payload_crc(), expected_crc)

    def test_malformed_authenticated_companion_preserves_whole_old_slot(self):
        for field in range(7):
            with self.subTest(field=field):
                self.setUp()
                self.export()
                self.lib.client_bad_companion(1, field)
                self.lib.client_save(22)
                expected_crc = self.lib.client_payload_crc()
                self.assert_old_kept(self.run_command(2, 1))
                self.assertEqual(self.lib.client_payload_crc(), expected_crc)

    def test_authenticated_core_extent_cannot_point_outside_candidate(self):
        for size in (0, 256, 60000, 0xFFFFFFF0, self.lib.client_limit() - 32):
            with self.subTest(size=size):
                self.setUp()
                self.export()
                self.lib.client_bad_core_size(1, size)
                self.lib.client_save(22)
                self.assert_old_kept(self.run_command(2, 1))

    def test_outer_overflow_header_is_rejected_before_payload_write(self):
        for raw, payload in ((0xFFFFFFF0, 0), (self.lib.client_limit(), self.lib.client_limit() + 16)):
            with self.subTest(raw=raw):
                self.setUp()
                self.export()
                self.lib.client_bad_archive_extent(1, raw, payload)
                self.lib.client_save(22)
                self.assert_old_kept(self.run_command(2, 1))
                self.assertEqual(self.lib.client_io_calls(2), 1)

    def test_two_segment_backup_survives_larger_partial_import(self):
        self.assertEqual(self.lib.client_save_profile(11, 300000, 50000, 0), 1)
        self.assertIn("EXPORTED", self.run_command(1, 1))
        self.lib.client_save(22)
        self.lib.client_pool_size(64)
        expected_crc = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_start(2, 1), 1)
        self.assertGreater(self.lib.client_backup_size(), 64)
        self.lib.client_fault(2, 4)
        self.drain()
        self.assert_old_kept(self.lib.client_text().decode())
        self.assertEqual(self.lib.client_payload_crc(), expected_crc)

    def test_quick_rollback_restores_old_slot_after_corrupt_sd_payload(self):
        self.export()
        self.lib.client_save(22)
        expected = self.lib.client_payload_crc()
        quick = self.lib.client_codec_size(1)
        self.lib.client_corrupt_file(1, 364)
        self.assertEqual(self.lib.client_start(2, 1), 1)
        self.assertEqual(self.lib.client_backup_codec(), 0x4C5A3431)
        self.assertEqual(self.lib.client_backup_size(), quick)
        self.assertEqual(bytes(self.lib.client_backup_byte(i) for i in range(4)), b"LML4")
        self.assertEqual(self.lib.client_backup_valid(), 1)
        self.drain()
        self.assert_old_kept(self.lib.client_text().decode())
        self.assertIn(b"CRC ERROR", self.lib.client_text())
        self.assertEqual(self.lib.client_payload_crc(), expected)
        self.assertEqual(self.lib.client_raw_valid(), 1)

    def test_dense_rollback_fallback_exact_capacity_restores_old_slot(self):
        self.export()
        # Nibble entropy benefits strongly from Huffman coding, so quick output
        # is larger than dense. This is synthetic, not private gameplay data.
        self.assertEqual(self.lib.client_save_nibble_profile(22, 0x800000, 0x12345678), 1)
        old_size = self.lib.client_size()
        expected = self.lib.client_payload_crc()
        quick = self.lib.client_codec_size(1)
        dense = self.lib.client_codec_size(0)
        staging = self.lib.client_staging_capacity()
        self.assertGreater(quick, dense)
        self.assertGreater(dense, staging)
        self.lib.client_pool_size(dense - staging)
        self.lib.client_corrupt_file(1, 364)
        self.assertEqual(self.lib.client_start(2, 1), 1)
        self.assertEqual(self.lib.client_backup_codec(), 0x44464C31)
        self.assertEqual(self.lib.client_backup_size(), dense)
        self.assertNotEqual(bytes(self.lib.client_backup_byte(i) for i in range(4)), b"LML4")
        self.assertEqual(self.lib.client_backup_valid(), 1)
        self.drain()
        self.assertIn(b"CRC ERROR; STATE KEPT", self.lib.client_text())
        self.assertEqual(self.lib.client_payload_crc(), expected)
        self.assertEqual(self.lib.client_size(), old_size)
        self.assertEqual(self.lib.client_raw_valid(), 1)
        # Neither codec fits one byte below the exact dense requirement.
        self.lib.client_pool_size(dense - staging - 1)
        opens = self.lib.client_io_calls(1)
        self.assertEqual(self.lib.client_start(2, 1), 0)
        self.assertIn(b"FULL NEED", self.lib.client_text())
        self.assertEqual(self.lib.client_pending(), 0)
        self.assertEqual(self.lib.client_io_calls(1), opens)
        self.assertEqual(self.lib.client_payload_crc(), expected)

    def test_rollback_rejects_unknown_codec_without_altering_original_slot(self):
        self.export()
        self.lib.client_save(22)
        expected = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_start(2, 99), 1)
        codec = self.lib.client_backup_codec()
        self.assertEqual(codec, 0x4C5A3431)
        for invalid in (0, 1, 0xFFFFFFFF):
            self.lib.client_backup_set_codec(invalid)
            self.assertEqual(self.lib.client_backup_valid(), 0)
            self.assertEqual(self.lib.client_payload_crc(), expected)
        self.lib.client_backup_set_codec(codec)
        self.assertEqual(self.lib.client_backup_valid(), 1)
        self.drain()
        self.assert_old_kept(self.lib.client_text().decode())
        self.assertEqual(self.lib.client_payload_crc(), expected)

    def test_capacity_failure_never_starts_arm_or_changes_old_state(self):
        self.assertEqual(self.lib.client_save_profile(22, 0x410000, 4096, 0x1234), 1)
        self.lib.client_pool_size(0)
        expected_crc = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_start(2, 99), 0)
        self.assertIn(b"FULL NEED", self.lib.client_text())
        self.assertEqual(self.lib.client_pending(), 0)
        self.assertEqual(self.lib.client_io_calls(1), 0)
        self.assertEqual(self.lib.client_payload_crc(), expected_crc)

    def test_candidate_at_payload_ceiling_has_disjoint_small_slot_rollback(self):
        self.lib.client_save(11)
        companion = self.lib.client_size() - self.lib.client_core_size()
        core = self.lib.client_limit() - companion - self.lib.client_trailer_size()
        self.assertEqual(self.lib.client_save_profile(11, core, 4096, 0), 1)
        self.assertEqual(self.lib.client_size() + self.lib.client_trailer_size(), self.lib.client_limit())
        self.assertIn("EXPORTED", self.run_command(1, 1))
        self.lib.client_save(22)
        self.lib.client_pool_size(64)
        expected_crc = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_start(2, 1), 1)
        self.lib.client_finish_worker()
        self.lib.client_bad_receipt(0)
        self.assert_old_kept(self.lib.client_text().decode())
        self.assertEqual(self.lib.client_payload_crc(), expected_crc)

    def test_named_export_catalog_rename_and_import_preserve_snapshot(self):
        self.lib.client_save(11)
        self.assertEqual(self.lib.client_start_named(1, 1, b"Parlor practice"), 1)
        self.drain()
        self.assertEqual(self.lib.client_text(), b"EXPORT 00000001: EXPORTED: THIS BOOT ONLY")
        original_file = self.lib.client_file_crc(1)
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_name(0), b"Parlor practice")
        self.lib.client_save(22)
        slot_crc = self.lib.client_slot_crc()
        self.assertEqual(self.lib.client_start_named(4, 1, b"Anteroom 100%"), 1)
        self.drain()
        self.assertEqual(self.lib.client_text(), b"RENAME 00000001: RENAMED")
        self.assertEqual(self.lib.client_catalog_name(0), b"Anteroom 100%")
        self.assertEqual(self.lib.client_slot_crc(), slot_crc)
        self.assertEqual(self.lib.client_file_crc(1), original_file)
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_name(0), b"Anteroom 100%")
        self.assertIn("READY - PRESS LOAD", self.run_command(2, 1))
        self.assertEqual(self.lib.client_marker(), 11)

    def test_old_session_name_can_change_but_import_still_refuses(self):
        self.export()
        self.lib.client_session()
        self.assertEqual(self.lib.client_start_named(4, 1, b"Before reboot"), 1)
        self.drain()
        self.assertIn(b"RENAMED", self.lib.client_text())
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_name(0), b"Before reboot")
        self.assertEqual(self.lib.client_catalog_compatible(0), 0)
        self.lib.client_save(22)
        self.assert_old_kept(self.run_command(2, 1))

    def test_clearing_name_and_renaming_with_no_resident_slot(self):
        self.export()
        self.lib.client_empty()
        self.assertEqual(self.lib.client_start_named(4, 1, b"A" * 31), 1)
        self.drain()
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_name(0), b"A" * 31)
        self.assertEqual(self.lib.client_start_named(4, 1, b""), 1)
        self.drain()
        self.assertEqual(self.lib.client_catalog_name(0), b"")
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_name(0), b"")
        self.assertEqual(self.lib.client_size(), 0)

    def test_bad_names_or_missing_ids_never_change_resident_state(self):
        self.export()
        self.lib.client_save(22)
        before = self.lib.client_slot_crc()
        for name in (b"A" * 32, b"Line\nBreak", b"\x7f", b"\xff"):
            self.assertEqual(self.lib.client_start_named(4, 1, name), 0)
            self.assertIn(b"INVALID NAME", self.lib.client_text())
            self.assertEqual(self.lib.client_pending(), 0)
        for ident in (0, 100000000):
            self.assertEqual(self.lib.client_start_named(4, ident, b"Valid"), 0)
            self.assertIn(b"INVALID ID", self.lib.client_text())
        self.assertEqual(self.lib.client_start_named(4, 99, b"Missing"), 1)
        self.drain()
        self.assertIn(b"NOT FOUND", self.lib.client_text())
        self.assertEqual(self.lib.client_slot_crc(), before)

    def test_export_name_failure_keeps_committed_id_and_state(self):
        self.lib.client_save(11)
        before = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_start_named(1, 17, b"Name failure"), 1)
        self.lib.client_finish_worker()
        self.lib.client_name_error_reply()
        self.assertEqual(self.lib.client_text(), b"EXPORT 00000017: THIS BOOT ONLY; NAME NOT SAVED")
        self.assertEqual(self.lib.client_archive(), 17)
        self.assertEqual(self.lib.client_payload_crc(), before)
        self.assertIn("READY - PRESS LOAD", self.run_command(2, 17))

    def test_rename_bad_receipt_never_updates_cached_name_or_reports_success(self):
        for field in range(7):
            with self.subTest(field=field):
                self.setUp()
                self.export()
                self.run_command(3, 0)
                before = self.lib.client_slot_crc()
                self.assertEqual(self.lib.client_start_named(4, 1, b"New name"), 1)
                self.lib.client_finish_worker()
                self.lib.client_bad_receipt(field)
                self.assertIn(b"BAD RESPONSE", self.lib.client_text())
                self.assertEqual(self.lib.client_catalog_name(0), b"")
                self.assertEqual(self.lib.client_slot_crc(), before)

    def prepare_persistent(self):
        self.assertEqual(self.lib.client_prepare_key(), 1)
        self.drain()
        self.assertEqual(self.lib.client_key_ready(), 1)
        self.assertNotEqual(self.lib.client_key_id(), 0)
        self.assertNotEqual(self.lib.client_config_id(), 0)
        self.lib.client_enable_persistent(1)

    def named_file(self, name):
        size = self.lib.client_named_size(name)
        self.assertGreater(size, 0)
        return ctypes.string_at(self.lib.client_named_bytes(name), size)

    def test_key_creation_runs_real_client_worker_handshake_without_touching_slot(self):
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        self.assertEqual(self.lib.client_prepare_key(), 1)
        self.assertEqual(self.lib.client_pending(), 1)
        self.assertIn(b"PREPARING SD KEY", self.lib.client_text())
        self.assertEqual(self.lib.client_start(1, 1), 0)
        self.drain()
        self.assertEqual(self.lib.client_text(), b"SD KEY READY")
        self.assertEqual(self.lib.client_payload_crc(), before)
        self.assertEqual(self.named_file(b"archive_key0.bin"), self.named_file(b"archive_key1.bin"))
        self.assertEqual(self.lib.client_named_size(b"archive_00000001.lms"), -1)

    def test_portable_named_export_survives_two_fresh_boots_and_reconstructs_raw_slot(self):
        self.prepare_persistent()
        self.lib.client_save(11)
        original_crc = self.lib.client_payload_crc()
        original_profile = self.lib.client_profile_crc()
        original_size = self.lib.client_size()
        self.assertEqual(self.lib.client_start_named(1, 1, b"Parlor 100%"), 1)
        self.drain()
        self.assertEqual(self.lib.client_text(), b"EXPORT 00000001: EXPORTED: REBOOT READY")
        self.assertEqual(self.lib.client_file_version(1), 2)
        key = self.named_file(b"archive_key0.bin")
        key_id = self.lib.client_key_id()
        for _ in range(2):
            session = self.lib.client_process_session()
            self.lib.client_reboot()
            self.assertNotEqual(self.lib.client_process_session(), session)
            self.assertEqual(self.lib.client_size(), 0)
            self.assertEqual(self.lib.client_key_ready(), 0)
            self.assertEqual(self.lib.client_profile_crc(), 0)
            self.prepare_persistent()
            self.assertEqual(self.lib.client_key_id(), key_id)
            self.assertEqual(self.named_file(b"archive_key0.bin"), key)
            self.assertEqual(self.lib.client_io_calls(3), 0)  # No key rotation.
            self.run_command(3, 0)
            self.assertEqual(self.lib.client_catalog_name(0), b"Parlor 100%")
            self.assertEqual(self.lib.client_catalog_entry_text(0), b"REBOOT ARCHIVE")
            self.assertEqual(self.lib.client_catalog_compatible(0), 1)
            self.assertIn("READY - PRESS LOAD", self.run_command(2, 1))
            self.assertEqual(self.lib.client_marker(), 11)
            self.assertEqual(self.lib.client_size(), original_size)
            self.assertEqual(self.lib.client_payload_crc(), original_crc)
            self.assertEqual(self.lib.client_profile_crc(), original_profile)
            self.assertEqual(self.lib.client_raw_valid(), 1)
            self.assertEqual(self.lib.client_persistent_loaded(), 1)

    def test_ready_key_does_not_upgrade_zero_profile_legacy_archive(self):
        self.prepare_persistent()
        self.lib.client_enable_persistent(0)
        self.export()
        self.assertEqual(self.lib.client_file_version(1), 1)
        self.assertIn(b"THIS BOOT ONLY", self.lib.client_text())
        self.lib.client_reboot()
        self.prepare_persistent()
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        self.assert_old_kept(self.run_command(2, 1))
        self.assertIn(b"DIFFERENT SESSION", self.lib.client_text())
        self.assertEqual(self.lib.client_payload_crc(), before)

    def test_v2_header_binding_covers_every_non_tag_byte(self):
        self.prepare_persistent()
        self.export()
        for offset in range(64):
            with self.subTest(offset=offset):
                self.assertEqual(self.lib.client_header_binding_changes(1, offset),
                                 0 if 48 <= offset < 56 else 1)

    def test_every_v2_header_byte_mutation_is_rejected_and_rolls_back(self):
        self.prepare_persistent()
        self.export()
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        for offset in range(64):
            with self.subTest(offset=offset):
                self.lib.client_corrupt_file(1, offset)
                self.assert_old_kept(self.run_command(2, 1))
                self.assertEqual(self.lib.client_payload_crc(), before)
                self.assertEqual(self.lib.client_persistent_loaded(), 0)
                self.lib.client_corrupt_file(1, offset)

    def test_v2_generation_header_is_authenticated_before_profile_validation(self):
        self.prepare_persistent()
        self.export()
        self.lib.client_save(22)
        self.lib.client_corrupt_file(1, 44)
        text = self.run_command(2, 1)
        self.assertIn("AUTH ERROR", text)
        self.assert_old_kept(text)

    def test_v2_payload_corruption_rolls_back_full_profile(self):
        self.prepare_persistent()
        self.export()
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        profile = self.lib.client_profile_crc()
        self.lib.client_corrupt_file(1, 364)
        text = self.run_command(2, 1)
        self.assertIn("CRC ERROR", text)
        self.assert_old_kept(text)
        self.assertEqual(self.lib.client_payload_crc(), before)
        self.assertEqual(self.lib.client_profile_crc(), profile)

    def test_authenticated_invalid_reboot_profiles_never_replace_old_state(self):
        for field in range(9):
            with self.subTest(field=field):
                self.setUp()
                self.prepare_persistent()
                self.export()
                self.lib.client_bad_profile(1, field)
                self.lib.client_save(22)
                before = self.lib.client_payload_crc()
                text = self.run_command(2, 1)
                self.assertIn("BAD REBOOT PROFILE", text)
                self.assert_old_kept(text)
                self.assertEqual(self.lib.client_payload_crc(), before)
                self.assertEqual(self.lib.client_raw_valid(), 1)

    def test_invalid_profile_into_empty_slot_does_not_mark_it_reboot_loaded(self):
        self.prepare_persistent()
        self.export()
        self.lib.client_bad_profile(1, 7)
        self.lib.client_reboot()
        self.prepare_persistent()
        self.assertIn("BAD REBOOT PROFILE (SLOT EMPTY)", self.run_command(2, 1))
        self.assertEqual(self.lib.client_size(), 0)
        self.assertEqual(self.lib.client_persistent_loaded(), 0)

    def test_failed_import_preserves_previously_imported_portable_flag_and_profile(self):
        self.prepare_persistent()
        self.export(1, 11)
        self.export(2, 33)
        self.lib.client_bad_profile(2, 4)
        self.lib.client_reboot()
        self.prepare_persistent()
        self.run_command(2, 1)
        before = self.lib.client_payload_crc()
        profile = self.lib.client_profile_crc()
        self.assertEqual(self.lib.client_persistent_loaded(), 1)
        self.assert_old_kept(self.run_command(2, 2), marker=11)
        self.assertEqual(self.lib.client_payload_crc(), before)
        self.assertEqual(self.lib.client_profile_crc(), profile)
        self.assertEqual(self.lib.client_persistent_loaded(), 1)

    def test_wrong_runtime_configuration_is_catalogued_and_refused_before_payload(self):
        self.prepare_persistent()
        self.export()
        old_config = self.lib.client_config_id()
        self.lib.client_set_config(2, 1)
        self.lib.client_reboot()
        self.prepare_persistent()
        self.assertNotEqual(self.lib.client_config_id(), old_config)
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_compatible(0), 0)
        self.assertEqual(self.lib.client_catalog_entry_text(0), b"DIFFERENT SETUP")
        reads = self.lib.client_io_calls(2)
        self.assert_old_kept(self.run_command(2, 1))
        self.assertIn(b"DIFFERENT SETUP", self.lib.client_text())
        self.assertEqual(self.lib.client_io_calls(2) - reads, 1)
        self.assertEqual(self.lib.client_payload_crc(), before)

    def test_new_sd_key_cannot_authenticate_previous_archive(self):
        self.prepare_persistent()
        self.export()
        old_key = self.lib.client_key_id()
        for name in (b"archive_key0.bin", b"archive_key1.bin"):
            self.lib.client_remove_named(name)
        self.lib.client_reboot()
        self.prepare_persistent()
        self.assertNotEqual(self.lib.client_key_id(), old_key)
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        self.run_command(3, 0)
        self.assertEqual(self.lib.client_catalog_compatible(0), 0)
        self.assertEqual(self.lib.client_catalog_entry_text(0), b"DIFFERENT SD KEY")
        self.assert_old_kept(self.run_command(2, 1))
        self.assertEqual(self.lib.client_payload_crc(), before)

    def test_matching_key_id_with_wrong_secret_fails_authentication(self):
        self.prepare_persistent()
        self.export()
        self.lib.client_save(22)
        before = self.lib.client_payload_crc()
        self.lib.client_wrong_key_same_id()
        text = self.run_command(2, 1)
        self.assertIn("AUTH ERROR", text)
        self.assert_old_kept(text)
        self.assertEqual(self.lib.client_payload_crc(), before)

    def test_corrupt_durable_key_is_not_recreated_by_client_startup(self):
        self.prepare_persistent()
        self.export()
        good = self.named_file(b"archive_key0.bin")
        self.lib.client_put_named(b"archive_key1.bin", b"bad", 3)
        self.lib.client_reboot()
        self.assertEqual(self.lib.client_prepare_key(), 1)
        self.drain()
        self.assertEqual(self.lib.client_key_ready(), 0)
        self.assertEqual(self.named_file(b"archive_key0.bin"), good)
        self.assertEqual(self.named_file(b"archive_key1.bin"), b"bad")
        self.assertEqual(self.lib.client_io_calls(3), 0)
        self.lib.client_save(22)
        self.assert_old_kept(self.run_command(2, 1))
        self.assertIn(b"SD KEY ERROR", self.lib.client_text())

    def test_portable_name_failure_reports_created_archive_and_remains_importable(self):
        self.prepare_persistent()
        self.lib.client_save(11)
        self.assertEqual(self.lib.client_start_named(1, 17, b"Name failure"), 1)
        self.lib.client_finish_worker()
        self.lib.client_name_error_reply()
        self.assertEqual(self.lib.client_text(), b"EXPORT 00000017: REBOOT READY; NAME NOT SAVED")
        self.assertEqual(self.lib.client_file_version(17), 2)
        self.assertEqual(self.lib.client_archive(), 17)
        self.lib.client_reboot()
        self.prepare_persistent()
        self.assertIn("READY - PRESS LOAD", self.run_command(2, 17))
        self.assertEqual(self.lib.client_marker(), 11)


if __name__ == "__main__":
    unittest.main()
