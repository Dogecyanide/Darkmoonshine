"""Fault-inject the production ARM display-name worker, not a metadata model."""
from __future__ import annotations

import ctypes
import struct
import unittest
import zlib

try:
    from scripts import test_lm_state_storage as kernel_tests
except ModuleNotFoundError:
    import test_lm_state_storage as kernel_tests


class LmStateNameWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        kernel_tests.LmStateKernelWorkerTests.setUpClass.__func__(cls)
        cls.lib.set_name.argtypes = [ctypes.c_void_p]
        cls.lib.file_bytes.argtypes = [ctypes.c_uint]
        cls.lib.file_bytes.restype = ctypes.c_void_p
        cls.lib.add_named_file.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_uint]
        cls.lib.named_file_size.argtypes = [ctypes.c_char_p]
        cls.lib.named_file_bytes.argtypes = [ctypes.c_char_p]
        cls.lib.named_file_bytes.restype = ctypes.c_void_p
        cls.lib.remove_named_file.argtypes = [ctypes.c_char_p]
        cls.lib.catalog_name.argtypes = [ctypes.c_uint]
        cls.lib.catalog_name.restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        kernel_tests.LmStateKernelWorkerTests.tearDownClass.__func__(cls)

    setUp = kernel_tests.LmStateKernelWorkerTests.setUp
    drain = kernel_tests.LmStateKernelWorkerTests.drain
    archive = kernel_tests.LmStateKernelWorkerTests.archive

    def name(self, text: bytes):
        self.assertLessEqual(len(text), 31)
        self.lib.set_name(text.ljust(32, b"\0"))

    def export(self, name=b"Parlor", file_id=1):
        self.name(name)
        self.lib.request(1, file_id, 1000)
        self.drain()
        self.assertEqual(self.lib.status(), 0)

    def rename(self, text: bytes, file_id=1):
        self.name(text)
        self.lib.request(4, file_id, 0)
        self.drain()

    def catalog(self, after=0):
        self.lib.request(3, after, 0)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        return {self.lib.catalog_id(i): self.lib.catalog_name(i)
                for i in range(self.lib.catalog_count())}

    def bytes(self, basename: bytes):
        size = self.lib.named_file_size(basename)
        self.assertGreaterEqual(size, 0)
        return ctypes.string_at(self.lib.named_file_bytes(basename), size)

    def replace(self, basename: bytes, data: bytes):
        self.lib.add_named_file(basename, bytes(data), len(data))

    @staticmethod
    def checksum(data: bytes):
        result = bytearray(data)
        result[48:52] = bytes(4)
        result[48:52] = struct.pack("=I", zlib.crc32(result))
        return bytes(result)

    def test_export_name_persists_after_worker_restart_with_bound_header(self):
        self.export(b"Pearl dupe - 1F")
        snapshot = ctypes.string_at(self.lib.snapshot(), 4096)
        archive = self.bytes(b"archive_00000001.lms")
        record = self.bytes(b"archive_00000001.name0")
        self.assertEqual(len(record), 64)
        self.assertEqual(record, self.checksum(record))
        self.assertEqual(struct.unpack_from("=4I", record), (0x4C4D534E, 1, 1, 1))
        self.assertEqual(struct.unpack_from("=I", record, 52)[0], zlib.crc32(archive[:64]))
        self.lib.restart_worker()
        self.assertEqual(self.catalog(), {1: b"Pearl dupe - 1F"})
        self.assertEqual(self.bytes(b"archive_00000001.lms"), archive)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 4096), snapshot)

    def test_repeated_rename_alternates_generations_and_empty_name_clears(self):
        self.export()
        original = self.bytes(b"archive_00000001.lms")
        for generation, name in enumerate((b"A" * 31, b"Third", b"", b"Final"), 2):
            self.rename(name)
            self.assertEqual(self.lib.status(), 0)
            self.lib.restart_worker()
            self.assertEqual(self.catalog(), {1: name})
            record = self.bytes(f"archive_00000001.name{(generation - 1) % 2}".encode())
            self.assertEqual(struct.unpack_from("=I", record, 12)[0], generation)
            self.assertEqual(self.bytes(b"archive_00000001.lms"), original)

    def test_invalid_ascii_termination_or_padding_rejected_before_any_file_write(self):
        self.export()
        before = self.bytes(b"archive_00000001.lms")
        for value in (b"A" * 32, b"Bad\n" + bytes(28), b"\x80" + bytes(31),
                      b"\x7f" + bytes(31), b"A\0B" + bytes(29)):
            for command, file_id in ((4, 1), (1, 2)):
                self.lib.set_name(value)
                writes = self.lib.operation_calls(3)
                self.lib.request(command, file_id, 1000 if command == 1 else 0)
                self.drain()
                self.assertEqual(self.lib.status(), 3)
                self.assertEqual(self.lib.operation_calls(3), writes)
                self.assertEqual(self.lib.file_size(2, 0), -1)
                self.assertEqual(self.bytes(b"archive_00000001.lms"), before)
        self.assertEqual(self.catalog(), {1: b"Parlor"})

    def test_request_name_is_frozen_when_request_is_accepted(self):
        self.name(b"Original")
        self.lib.request(1, 1, 1000)
        self.lib.step()
        self.name(b"Later")
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.catalog(), {1: b"Original"})

    def test_names_are_display_metadata_not_paths(self):
        text = b"../hello: \\ * ? < > | \""
        self.export(text)
        self.assertEqual(self.catalog(), {1: text})
        self.assertEqual(self.lib.file_size(1, 0), 1064)

    def test_corrupt_truncated_and_wrong_bound_latest_copy_fall_back(self):
        self.export(b"Old")
        self.rename(b"New")
        original = self.bytes(b"archive_00000001.name1")
        cases = [original[:23], original + b"x"]
        corrupt = bytearray(original); corrupt[17] ^= 1; cases.append(bytes(corrupt))
        for offset in (0, 4, 8, 12, 52, 56):
            corrupt = bytearray(original)
            corrupt[offset:offset + 4] = struct.pack("=I", 0 if offset == 12 else 0xABCDEFFF)
            cases.append(self.checksum(corrupt))
        corrupt = bytearray(original); corrupt[16:48] = b"X" * 32
        cases.append(self.checksum(corrupt))
        for data in cases:
            with self.subTest(data=data):
                self.replace(b"archive_00000001.name1", data)
                self.lib.restart_worker()
                self.assertEqual(self.catalog(), {1: b"Old"})
        self.replace(b"archive_00000001.name0", b"torn")
        self.assertEqual(self.catalog(), {1: b""})

    def test_rename_io_failures_retain_previous_valid_label_and_archive_bytes(self):
        failures = [(1, n) for n in (1, 2, 3, 4)] + [(2, n) for n in (1, 2, 3)]
        failures += [(3, 1), (4, 1), (5, 1), (5, 2), (5, 3), (5, 4), (6, 1), (10, 1)]
        for operation, relative in failures:
            with self.subTest(operation=operation, relative=relative):
                self.lib.reset_worker()
                self.export(b"First")
                self.rename(b"Keep me")
                self.assertEqual(self.lib.status(), 0)
                original = self.bytes(b"archive_00000001.lms")
                self.lib.fault(operation, self.lib.operation_calls(operation) + relative)
                self.rename(b"Failed replacement")
                self.assertEqual(self.lib.status(), 2)
                self.lib.restart_worker()
                self.assertEqual(self.catalog(), {1: b"Keep me"})
                self.assertEqual(self.bytes(b"archive_00000001.lms"), original)

    def test_export_naming_failure_keeps_committed_archive_and_reports_partial_success(self):
        for operation, at in ((1, 4), (3, 3), (4, 2), (5, 2), (6, 2), (10, 1)):
            with self.subTest(operation=operation):
                self.lib.reset_worker()
                self.name(b"Unpublished")
                self.lib.fault(operation, at)
                self.lib.request(1, 3, 1000)
                self.drain()
                self.assertEqual(self.lib.status(), 8)
                self.assertEqual(self.lib.result_id(), 3)
                self.assertEqual(self.lib.transferred(), 1000)
                self.assertEqual(self.lib.file_size(3, 0), 1064)
                self.lib.restart_worker()
                self.assertEqual(self.catalog(), {3: b""})
                self.lib.request(2, 3, 0)
                self.drain()
                self.assertEqual(self.lib.status(), 0)

    def test_missing_archive_or_malformed_archive_cannot_be_renamed(self):
        self.rename(b"Missing", 4)
        self.assertEqual(self.lib.status(), 5)
        self.lib.add_import(4, b"broken", 6)
        self.rename(b"Bad", 4)
        self.assertEqual(self.lib.status(), 3)
        self.assertEqual(self.lib.named_file_size(b"archive_00000004.name0"), -1)

    def test_old_session_is_nameable_but_still_cannot_import(self):
        data = self.archive(b"a" * 1000, session=123)
        self.lib.add_import(5, data, len(data))
        before = ctypes.string_at(self.lib.snapshot(), 4096)
        self.rename(b"Old run", 5)
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.catalog(), {5: b"Old run"})
        self.lib.request(2, 5, 0)
        self.drain()
        self.assertEqual(self.lib.status(), 6)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 4096), before)
        self.assertEqual(self.bytes(b"archive_00000005.lms"), data)

    def test_header_replacement_cannot_inherit_prior_name(self):
        self.export(b"Old file")
        original = self.bytes(b"archive_00000001.lms")
        replacement = bytearray(original)
        replacement[44:48] = struct.pack("=I", 2345)
        self.replace(b"archive_00000001.lms", replacement)
        self.assertEqual(self.catalog(), {1: b""})
        self.rename(b"Replacement")
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.catalog(), {1: b"Replacement"})

    def test_orphan_name_reserves_id_even_when_next_blank_label_write_fails(self):
        self.export(b"Old deleted state")
        orphan = self.bytes(b"archive_00000001.name0")
        self.assertEqual(self.lib.remove_named_file(b"archive_00000001.lms"), 0)
        self.name(b"")
        self.lib.request(1, 1, 1000)
        # A new archive skips the orphan ID rather than inheriting its metadata.
        ctypes.c_uint.from_address(self.lib.mailbox() + 64 + 44).value = 999
        self.lib.fault(3, self.lib.operation_calls(3) + 3)
        self.drain()
        self.assertEqual(self.lib.status(), 8)
        self.assertEqual(self.lib.file_size(1, 0), -1)
        self.assertEqual(self.lib.file_size(2, 0), 1064)
        self.assertEqual(self.bytes(b"archive_00000001.name0"), orphan)
        self.lib.restart_worker()
        self.assertEqual(self.catalog(), {2: b""})

    def test_catalog_metadata_read_error_falls_back_to_unnamed(self):
        self.export()
        self.lib.fault(2, self.lib.operation_calls(2) + 2)
        self.assertEqual(self.catalog(), {1: b""})
        self.lib.restart_worker()
        self.assertEqual(self.catalog(), {1: b"Parlor"})

    def test_catalog_ignores_malformed_filenames_and_incomplete_metadata(self):
        self.export()
        archive = self.bytes(b"archive_00000001.lms")
        for basename in (b"archive_00000002.lms.tmp", b"archive_00000002.LMS",
                         b"archive_0000002.lms", b"archive_000000002.lms",
                         b"archive_00000000.lms", b"archive_00000002.name0",
                         b"archive_00000002.name1.tmp", b"archive_0000000x.lms"):
            self.replace(basename, archive)
        self.assertEqual(self.catalog(), {1: b"Parlor"})

    def test_generation_exhaustion_refuses_instead_of_overwriting_latest_label(self):
        self.export()
        data = bytearray(self.bytes(b"archive_00000001.name0"))
        data[12:16] = struct.pack("=I", 0xFFFFFFFF)
        self.replace(b"archive_00000001.name0", self.checksum(data))
        self.rename(b"No wrap")
        self.assertEqual(self.lib.status(), 2)
        self.assertEqual(self.catalog(), {1: b"Parlor"})


if __name__ == "__main__":
    unittest.main()
