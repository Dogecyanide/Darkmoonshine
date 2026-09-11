"""Permanent selected-file deletion through production ARM and PPC code."""
import ctypes
import os
import unittest

try:
    from scripts import test_lm_state_names as names
    from scripts import test_lm_state_transactions as transactions
except ModuleNotFoundError:
    import test_lm_state_names as names
    import test_lm_state_transactions as transactions


class DeleteKernelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        names.LmStateNameWorkerTests.setUpClass.__func__(cls)

    @classmethod
    def tearDownClass(cls):
        names.LmStateNameWorkerTests.tearDownClass.__func__(cls)

    setUp = names.LmStateNameWorkerTests.setUp
    drain = names.LmStateNameWorkerTests.drain
    archive = names.LmStateNameWorkerTests.archive
    catalog = names.LmStateNameWorkerTests.catalog
    name = names.LmStateNameWorkerTests.name
    export = names.LmStateNameWorkerTests.export
    rename = names.LmStateNameWorkerTests.rename
    bytes = names.LmStateNameWorkerTests.bytes
    replace = names.LmStateNameWorkerTests.replace

    def select(self):
        self.catalog()
        self.lib.request_delete(0)

    def test_delete_frees_archive_and_both_names_but_no_other_files(self):
        self.export()
        self.rename(b"Second name")
        self.replace(b"archive_key0.bin", b"key zero")
        self.replace(b"archive_key1.bin", b"key one")
        self.replace(b"archive_00000002.lms", b"unrelated state")
        self.replace(b"archive_00000002.name0", b"unrelated name")
        before = ctypes.string_at(self.lib.snapshot(), 2000)
        self.select()
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        for extension in (b"lms", b"name0", b"name1"):
            self.assertEqual(self.lib.named_file_size(b"archive_00000001." + extension), -1)
        self.assertEqual(self.bytes(b"archive_key0.bin"), b"key zero")
        self.assertEqual(self.bytes(b"archive_key1.bin"), b"key one")
        self.assertEqual(self.bytes(b"archive_00000002.lms"), b"unrelated state")
        self.assertEqual(self.bytes(b"archive_00000002.name0"), b"unrelated name")
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 2000), before)
        self.assertEqual(self.catalog(), {2: b""})

    def test_old_build_session_and_corrupt_files_can_be_deleted(self):
        cases = [b"", b"truncated", bytes(64),
                 self.archive(b"x" * 1000, snapshot_version=3, build=0x12345678, session=42)]
        for data in cases:
            with self.subTest(length=len(data)):
                self.lib.reset_worker()
                self.replace(b"archive_00000001.lms", data)
                self.select()
                self.drain()
                self.assertEqual(self.lib.status(), 0)
                self.assertEqual(self.lib.file_size(1, 0), -1)

    def test_changed_header_or_length_is_refused_without_unlink(self):
        for replacement in (b"other", b"longer content"):
            with self.subTest(replacement=replacement):
                self.lib.reset_worker()
                self.replace(b"archive_00000001.lms", b"state")
                self.select()
                self.replace(b"archive_00000001.lms", replacement)
                self.drain()
                self.assertEqual(self.lib.status(), 11)
                self.assertEqual(self.bytes(b"archive_00000001.lms"), replacement)
                self.assertEqual(self.lib.operation_calls(10), 0)

    def test_bad_or_stale_proof_is_refused_before_io(self):
        for index, value, expected in [(0, 0, 11), (1, 0, 11), (2, 1, 11),
                                       (3, 5, 3), (4, 1, 3), (7, 1, 3)]:
            with self.subTest(index=index):
                self.lib.reset_worker()
                self.export()
                self.select()
                opened = self.lib.operation_calls(1)
                self.lib.delete_proof_word(index, value)
                self.drain()
                self.assertEqual(self.lib.status(), expected)
                self.assertEqual(self.lib.operation_calls(1), opened)
                self.assertGreater(self.lib.file_size(1, 0), 0)

    def test_current_page_and_process_session_are_required(self):
        self.export()
        self.catalog()
        self.lib.request(6, 1, 0)
        self.drain()
        self.assertNotEqual(self.lib.status(), 0)
        self.select()
        self.lib.cancel_session()
        self.drain()
        self.assertEqual(self.lib.status(), 11)
        self.assertGreater(self.lib.file_size(1, 0), 0)

    def test_cancellation_after_validation_never_deletes(self):
        self.export()
        self.select()
        self.lib.step()
        self.lib.cancel_session()
        self.drain()
        self.assertEqual(self.lib.status(), 7)
        self.assertGreater(self.lib.file_size(1, 0), 0)

    def test_open_read_close_and_archive_unlink_failures_keep_every_file(self):
        for operation in (1, 2, 5, 10):
            with self.subTest(operation=operation):
                self.lib.reset_worker()
                self.export()
                self.rename(b"Other name")
                old = {suffix: self.bytes(b"archive_00000001." + suffix)
                       for suffix in (b"lms", b"name0", b"name1")}
                self.select()
                self.lib.fault(operation, self.lib.operation_calls(operation) + 1)
                self.drain()
                self.assertEqual(self.lib.status(), 2)
                for suffix, data in old.items():
                    self.assertEqual(self.bytes(b"archive_00000001." + suffix), data)

    def test_cleanup_failure_truthfully_reports_deleted_and_reserves_id(self):
        for sidecar in (0, 1):
            with self.subTest(sidecar=sidecar):
                self.lib.reset_worker()
                self.export()
                self.rename(b"Other name")
                self.select()
                self.lib.fault(10, self.lib.operation_calls(10) + 2 + sidecar)
                self.drain()
                self.assertEqual(self.lib.status(), 12)
                self.assertEqual(self.lib.file_size(1, 0), -1)
                remaining = f"archive_00000001.name{sidecar}".encode()
                self.assertEqual(self.lib.named_file_size(remaining), 64)
                self.lib.fault(0, 0)
                self.export(file_id=1)
                self.assertEqual(self.lib.result_id(), 2)
                self.assertEqual(self.lib.named_file_size(remaining), 64)

    def test_missing_archive_returns_not_found_and_keeps_names(self):
        self.export()
        self.select()
        self.lib.remove_named_file(b"archive_00000001.lms")
        self.drain()
        self.assertEqual(self.lib.status(), 5)
        self.assertEqual(self.lib.named_file_size(b"archive_00000001.name0"), 64)

    def test_completed_delete_consumes_catalog_authority(self):
        self.export()
        self.select()
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.replace(b"archive_00000001.lms", b"new occupant")
        self.lib.request_delete(0)
        self.drain()
        self.assertEqual(self.lib.status(), 11)
        self.assertEqual(self.bytes(b"archive_00000001.lms"), b"new occupant")

    def test_invalid_id_does_not_turn_into_a_path(self):
        self.export()
        for ident in (0, 100000000, 0xFFFFFFFF):
            self.lib.request(6, ident, 0)
            self.drain()
            self.assertEqual(self.lib.status(), 3)
        self.assertGreater(self.lib.file_size(1, 0), 0)


@unittest.skipUnless(os.name == "nt", "Low-address Windows transaction harness")
class DeleteClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        transactions.LmStateTransactionTests.setUpClass.__func__(cls)

    @classmethod
    def tearDownClass(cls):
        transactions.LmStateTransactionTests.tearDownClass.__func__(cls)

    setUp = transactions.LmStateTransactionTests.setUp
    drain = transactions.LmStateTransactionTests.drain
    run_command = transactions.LmStateTransactionTests.run_command
    export = transactions.LmStateTransactionTests.export

    def selected(self):
        self.run_command(3, 0)
        token = self.lib.client_delete_token(0)
        self.assertNotEqual(token, 0)
        return token

    def test_success_preserves_entire_resident_snapshot(self):
        self.export()
        self.lib.client_save(22)
        before = self.lib.client_slot_crc()
        token = self.selected()
        self.assertEqual(self.lib.client_delete(1, token), 1)
        self.assertIn(b"WORKING", self.lib.client_text())
        self.drain()
        self.assertEqual(self.lib.client_text(), b"DELETE 00000001: DELETED FROM SD")
        self.assertEqual(self.lib.client_slot_crc(), before)
        self.assertEqual(self.lib.client_marker(), 22)
        self.assertEqual(self.lib.client_named_size(b"archive_00000001.lms"), -1)
        self.assertEqual(self.lib.client_delete_token(0), 0)

    def test_stale_confirmation_or_wrong_id_is_rejected_before_request(self):
        self.export()
        old_token = self.selected()
        token = self.selected()
        self.assertNotEqual(token, old_token)
        for ident, proof in [(1, old_token), (2, token), (1, 0)]:
            self.assertEqual(self.lib.client_delete(ident, proof), 0)
            self.assertIn(b"REFRESH LIST FIRST", self.lib.client_text())
        self.assertGreater(self.lib.client_named_size(b"archive_00000001.lms"), 0)

    def test_save_request_and_storage_busy_refuse_delete(self):
        self.export()
        token = self.selected()
        self.lib.client_busy(1)
        self.assertEqual(self.lib.client_delete_token(0), 0)
        self.assertEqual(self.lib.client_delete(1, token), 0)
        self.lib.client_busy(0)
        self.assertEqual(self.lib.client_start(3, 0), 1)
        self.assertEqual(self.lib.client_delete(1, token), 0)
        self.drain()
        self.assertGreater(self.lib.client_named_size(b"archive_00000001.lms"), 0)

    def test_changed_file_and_io_failure_never_damage_resident_state(self):
        for changed in (False, True):
            with self.subTest(changed=changed):
                self.lib.client_reset()
                self.export()
                self.lib.client_save(22)
                before = self.lib.client_slot_crc()
                token = self.selected()
                if changed:
                    self.lib.client_corrupt_file(1, 0)
                else:
                    self.lib.client_fault(10, self.lib.client_io_calls(10) + 1)
                self.assertEqual(self.lib.client_delete(1, token), 1)
                self.drain()
                self.assertNotIn(b"DELETED", self.lib.client_text())
                self.assertEqual(self.lib.client_slot_crc(), before)
                self.assertGreater(self.lib.client_named_size(b"archive_00000001.lms"), 0)

    def test_cleanup_failure_cannot_be_misreported_as_archive_kept(self):
        self.export()
        token = self.selected()
        before = self.lib.client_slot_crc()
        self.lib.client_fault(10, self.lib.client_io_calls(10) + 2)
        self.assertEqual(self.lib.client_delete(1, token), 1)
        self.drain()
        self.assertIn(b"DELETED; NAME CLEANUP FAILED", self.lib.client_text())
        self.assertLessEqual(len(self.lib.client_text()), 48)
        self.assertNotIn(b"STATE KEPT", self.lib.client_text())
        self.assertEqual(self.lib.client_named_size(b"archive_00000001.lms"), -1)
        self.assertEqual(self.lib.client_slot_crc(), before)

    def test_wrong_receipt_never_claims_success(self):
        self.export()
        token = self.selected()
        before = self.lib.client_slot_crc()
        self.assertEqual(self.lib.client_delete(1, token), 1)
        self.lib.client_finish_worker()
        self.lib.client_bad_receipt(0)
        self.assertIn(b"BAD RESPONSE", self.lib.client_text())
        self.assertEqual(self.lib.client_slot_crc(), before)

    def test_old_build_file_can_be_deleted_despite_incompatible_import(self):
        self.export()
        self.lib.client_corrupt_file(1, 24)
        token = self.selected()
        self.assertEqual(self.lib.client_catalog_compatible(0), 0)
        self.assertEqual(self.lib.client_delete(1, token), 1)
        self.drain()
        self.assertIn(b"DELETED FROM SD", self.lib.client_text())

    def test_old_or_unavailable_launcher_cannot_start_delete(self):
        for version, available in [(5, 1), (0, 1), (7, 1), (6, 0)]:
            with self.subTest(version=version, available=available):
                self.lib.client_reset()
                self.export()
                token = self.selected()
                before = self.lib.client_slot_crc()
                unlinks = self.lib.client_io_calls(10)
                self.lib.client_storage_capability(version, available)
                self.assertEqual(self.lib.client_delete(1, token), 0)
                self.assertEqual(self.lib.client_text(), b"SD UNAVAILABLE")
                self.assertEqual(self.lib.client_io_calls(10), unlinks)
                self.assertEqual(self.lib.client_slot_crc(), before)
                self.assertGreater(self.lib.client_named_size(b"archive_00000001.lms"), 0)


if __name__ == "__main__":
    unittest.main()
