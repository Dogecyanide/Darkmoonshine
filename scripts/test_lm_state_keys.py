"""Production ARM persistent-key/transport tests; all key material is synthetic."""
import ctypes
import struct
import unittest

try:
    from scripts import test_lm_state_storage as kernel_tests
except ModuleNotFoundError:
    import test_lm_state_storage as kernel_tests


class LmStateKeyWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        kernel_tests.LmStateKernelWorkerTests.setUpClass.__func__(cls)
        cls.lib.set_seed.argtypes = [ctypes.POINTER(ctypes.c_uint)]
        cls.lib.key_words_equal.argtypes = [ctypes.POINTER(ctypes.c_uint)]
        cls.lib.request_v2.argtypes = [ctypes.c_uint] * 3
        cls.lib.add_named_file.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_uint]
        cls.lib.named_file_size.argtypes = [ctypes.c_char_p]
        cls.lib.named_file_bytes.argtypes = [ctypes.c_char_p]
        cls.lib.named_file_bytes.restype = ctypes.c_void_p
        cls.lib.remove_named_file.argtypes = [ctypes.c_char_p]
        cls.lib.set_config.argtypes = [ctypes.c_uint, ctypes.c_uint]
        for name in ("key_id", "key_config", "key_status"):
            getattr(cls.lib, name).restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        kernel_tests.LmStateKernelWorkerTests.tearDownClass.__func__(cls)

    setUp = kernel_tests.LmStateKernelWorkerTests.setUp
    drain = kernel_tests.LmStateKernelWorkerTests.drain
    archive = kernel_tests.LmStateKernelWorkerTests.archive

    def seed(self, marker=1):
        result = (ctypes.c_uint * 4)(marker, 0x22334455, 0x33445566, 0x44556677)
        self.lib.set_seed(result)
        return result

    def create(self):
        self.seed()
        self.lib.request(5, 0, 0)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.lib.key_status(), 2)

    def named(self, name):
        size = self.lib.named_file_size(name)
        self.assertGreaterEqual(size, 0)
        return ctypes.string_at(self.lib.named_file_bytes(name), size)

    def replace(self, name, data):
        self.lib.add_named_file(name, bytes(data), len(data))

    def export_v2(self, identifier=1, size=1000):
        self.lib.request_v2(1, identifier, size)
        self.drain()
        self.assertEqual(self.lib.status(), 0)
        return self.named(f"archive_{identifier:08}.lms".encode())

    def import_id(self, identifier=1):
        self.lib.request(2, identifier, 0)
        self.drain()
        return self.lib.status()

    def test_init_missing_is_read_only_and_key_creation_publishes_two_equal_durable_copies(self):
        self.assertEqual(self.lib.key_status(), 1)
        self.assertEqual(self.lib.named_file_size(b"archive_key0.bin"), -1)
        before = ctypes.string_at(self.lib.snapshot(), 4096)
        self.create()
        self.assertNotEqual(self.lib.key_id(), 0)
        self.assertNotEqual(self.lib.key_config(), 0)
        self.assertEqual(self.named(b"archive_key0.bin"), self.named(b"archive_key1.bin"))
        identity = self.lib.key_id()
        self.lib.restart_worker()
        self.assertEqual((self.lib.key_status(), self.lib.key_id()), (2, identity))
        self.assertEqual(self.lib.key_words_equal(self.seed()), 1)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 4096), before)

    def test_key_is_immutable_and_new_seed_does_not_rotate_existing_key(self):
        self.create(); before = self.named(b"archive_key0.bin")
        writes = self.lib.operation_calls(3)
        self.seed(999); self.lib.request(5, 0, 0); self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.lib.operation_calls(3), writes)
        self.assertEqual(self.named(b"archive_key0.bin"), before)

    def test_zero_seed_reserved_words_or_wrong_request_shape_never_create_key(self):
        self.lib.set_seed((ctypes.c_uint * 4)())
        self.lib.request(5, 0, 0); self.drain()
        self.assertEqual(self.lib.status(), 3)
        self.seed()
        ctypes.c_uint.from_address(self.lib.mailbox() + 848).value = 1
        self.lib.request(5, 0, 0); self.drain()
        self.assertEqual(self.lib.status(), 3)
        for identifier, size in ((1, 0), (0, 32)):
            self.seed(); self.lib.request(5, identifier, size); self.drain()
            self.assertEqual(self.lib.status(), 3)
        self.assertEqual(self.lib.named_file_size(b"archive_key0.bin"), -1)

    def test_corrupt_truncated_or_conflicting_durable_keys_are_never_replaced(self):
        self.create(); good = self.named(b"archive_key0.bin")
        for damaged in (b"bad", good[:-1], good + b"x", bytes(64)):
            self.replace(b"archive_key1.bin", damaged)
            self.lib.restart_worker()
            self.assertEqual(self.lib.key_status(), 3)
            self.seed(777); self.lib.request(5, 0, 0); self.drain()
            self.assertEqual(self.lib.status(), 9)
            self.assertEqual(self.named(b"archive_key0.bin"), good)
            self.assertEqual(self.named(b"archive_key1.bin"), damaged)
        self.lib.reset_worker(); self.seed(88); self.lib.request(5, 0, 0); self.drain()
        other = self.named(b"archive_key0.bin")
        self.replace(b"archive_key1.bin", good)
        self.lib.restart_worker()
        self.assertEqual(self.lib.key_status(), 3)
        self.assertEqual(self.named(b"archive_key0.bin"), other)

    def test_one_valid_missing_copy_is_recoverable_but_unpublished_temp_is_ignored(self):
        self.create(); identity = self.lib.key_id()
        self.lib.remove_named_file(b"archive_key1.bin")
        self.replace(b"archive_key1.tmp", b"interrupted garbage")
        self.lib.restart_worker()
        self.assertEqual((self.lib.key_status(), self.lib.key_id()), (2, identity))
        self.lib.remove_named_file(b"archive_key0.bin")
        self.lib.restart_worker()
        self.assertEqual(self.lib.key_status(), 1)

    def test_creation_faults_never_publish_a_nondurable_key_or_lose_a_durable_copy(self):
        failures = [(1, n) for n in range(1, 7)] + [(2, 1), (2, 2)]
        failures += [(op, n) for op in (3, 4, 6) for n in (1, 2)]
        failures += [(5, n) for n in range(1, 5)]
        for operation, at in failures:
            with self.subTest(operation=operation, at=at):
                self.lib.reset_worker(); self.seed(); self.lib.fault(operation, at)
                self.lib.request(5, 0, 0); self.drain()
                self.assertEqual(self.lib.status(), 9)
                present = [self.lib.named_file_size(f"archive_key{i}.bin".encode()) == 64 for i in (0, 1)]
                if self.lib.key_status() == 2:
                    self.assertTrue(any(present))
                self.lib.restart_worker()
                self.assertEqual(self.lib.key_status(), 2 if any(present) else 1)

    def test_configuration_changes_do_not_rotate_key_but_refuse_old_config_archive(self):
        self.create(); key = self.lib.key_id(); config = self.lib.key_config()
        self.export_v2()
        before = self.named(b"archive_key0.bin")
        self.lib.set_config(1, 0x10002); self.lib.restart_worker()
        self.assertEqual((self.lib.key_status(), self.lib.key_id()), (2, key))
        self.assertNotEqual(self.lib.key_config(), config)
        self.assertEqual(self.import_id(), 10)
        self.assertEqual(self.named(b"archive_key0.bin"), before)

    def test_runtime_fields_change_config_but_ui_only_flags_do_not(self):
        self.create(); original = self.lib.key_config(); key = self.lib.key_id()
        for field in range(1, 13):
            self.lib.set_config(field, 0x57); self.lib.restart_worker()
            self.assertNotEqual(self.lib.key_config(), original)
            self.assertEqual(self.lib.key_id(), key)
            self.lib.set_config(field, {3: 4, 4: 0x474C4D4A, 11: 8}.get(field, 0))
        for bit in (4, 7, 9, 11, 12, 17, 19, 20):
            self.lib.set_config(0, 1 << bit); self.lib.restart_worker()
            self.assertEqual(self.lib.key_config(), original)

    def test_cheats_debugger_and_debugwait_disable_persistent_but_not_legacy_io(self):
        self.create(); before = self.named(b"archive_key0.bin"); self.export_v2()
        for flag in (1, 2, 4):
            self.lib.set_config(0, flag); self.lib.restart_worker()
            self.assertEqual(self.lib.key_status(), 5)
            self.assertEqual(self.import_id(), 9)
            self.seed(); self.lib.request(5, 0, 0); self.drain()
            self.assertEqual(self.lib.status(), 9)
            self.lib.request(1, 10 + flag, 1000); self.drain()
            self.assertEqual(self.lib.status(), 0)
            self.assertEqual(self.named(b"archive_key0.bin"), before)

    def test_v2_cross_process_transport_preserves_fresh_receipt_and_catalog_profile(self):
        self.create(); data = self.export_v2()
        header = struct.unpack_from("=16I", data)
        self.assertEqual((header[1], header[7], header[14], header[15]),
                         (2, self.lib.key_id(), 0x4C4D5031, self.lib.key_config()))
        self.lib.restart_worker(); self.lib.request(2, 1, 0); self.lib.cancel_session(); self.drain()
        self.assertEqual(self.lib.status(), 0)
        self.assertEqual(self.lib.transferred(), 1000)
        self.lib.request(3, 0, 0); self.drain()
        self.assertEqual(self.lib.catalog_flags(0), 3)
        self.assertEqual(self.lib.catalog_config(0), self.lib.key_config())

    def test_v2_wrong_key_profile_or_config_is_rejected_before_payload(self):
        self.create(); original = self.export_v2()
        for offset, value, status in ((7 * 4, 123, 9), (14 * 4, 0, 3), (15 * 4, 123, 10)):
            data = bytearray(original); struct.pack_into("=I", data, offset, value)
            self.replace(b"archive_00000001.lms", data)
            ctypes.memset(self.lib.snapshot(), 0xAD, 2000)
            self.assertEqual(self.import_id(), status)
            self.assertEqual(ctypes.string_at(self.lib.snapshot(), 2000), b"\xAD" * 2000)

    def test_v2_missing_key_and_inflight_process_reset_fail_closed(self):
        self.create(); self.export_v2(size=50000)
        self.lib.request(2, 1, 0)
        for _ in range(4): self.lib.step()
        before = ctypes.string_at(self.lib.snapshot(), 50000)
        self.lib.cancel_session(); self.drain()
        self.assertEqual(self.lib.status(), 7)
        self.assertEqual(ctypes.string_at(self.lib.snapshot(), 50000), before)
        self.lib.remove_named_file(b"archive_key0.bin"); self.lib.remove_named_file(b"archive_key1.bin")
        self.lib.restart_worker()
        self.assertEqual(self.import_id(), 9)

    def test_v1_keeps_old_process_session_restriction(self):
        data = self.archive(b"x" * 1000)
        self.lib.add_import(1, data, len(data))
        self.lib.request(2, 1, 0); self.lib.cancel_session(); self.drain()
        self.assertEqual(self.lib.status(), 6)
        self.assertEqual(self.lib.transferred(), 0)


if __name__ == "__main__":
    unittest.main()
