"""Execute the ARM preference parser/writer against host files and I/O faults."""

import ctypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
KEYS = [
    "metadata", "inputs", "lag", "r_pump", "luigi_colour", "luigi_rgb",
    "timer_visible", "timer_label", "timer_x", "timer_y", "timer_scale",
    "timer_opacity", "timer_brightness", "timer_background_rgb",
    "timer_background_opacity", "timer_padding",
    *[f"timer_char_{i}_rgb" for i in range(9)],
    "streak_x", "streak_y", "streak_scale", "streak_opacity", "streak_brightness",
    "streak_background_rgb", "streak_background_opacity", "streak_padding",
    "streak_rgb", "timing_profile", "reference_1_delay", "reference_1_hold",
    "reference_1_tolerance", "reference_1_button", "reference_1_valid",
    "reference_2_delay", "reference_2_hold", "reference_2_tolerance",
    "reference_2_button", "reference_2_valid", "boo_safe",
    "timer_run_in_menus", "reset_room_bind",
]


class Block(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in (
        "magic", "version", "requestSeq", "checksum", "presentLo", "presentHi",
        "reserved0a", "reserved0b", "ackSeq", "status", "ready",
        "reserved1a", "reserved1b", "reserved1c", "reserved1d", "reserved1e",
    )] + [("values", ctypes.c_uint32 * 48)]


def ini(values, version=2):
    low = sum(1 << i for i in values if i < 32)
    high = sum(1 << (i - 32) for i in values if i >= 32)
    count = 46 if version == 1 else 48
    words = [values.get(i, 0xFFFFFFFF) & 0xFFFFFFFF for i in range(count)] + [0] * (48 - count)
    crc = zlib.crc32(struct.pack(">51I", version, low, high, *words))
    fields = ["[lm_preferences]", f"version = {version}"]
    fields += [f"{KEYS[i]} = 0x{words[i]:08X}" for i in values]
    return ("\r\n".join(fields) + f"\r\nchecksum = 0x{crc:08X}\r\n").encode()


class PreferencesKernelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.build = tempfile.TemporaryDirectory(prefix="lm-prefs-test-")
        output = Path(cls.build.name) / ("prefs.dll" if os.name == "nt" else "prefs.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"), "-I", str(ROOT / "scripts"),
                   str(ROOT / "scripts/lm_preferences_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        built = subprocess.run(command, capture_output=True, env=env)
        if built.returncode:
            raise RuntimeError(built.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.setup.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32]
        cls.lib.block.restype = ctypes.POINTER(Block)
        cls.lib.parse.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
        cls.lib.parse.restype = ctypes.c_uint32
        cls.lib.LmPreferencesPending.restype = ctypes.c_bool

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.build.cleanup()

    def setUp(self):
        self.files = tempfile.TemporaryDirectory(prefix="lm-prefs-")
        self.addCleanup(self.files.cleanup)
        self.path = Path(self.files.name) / "moonshine_lm.ini"
        self.backup = Path(str(self.path) + ".lm.bak")
        self.lib.setup(os.fsencode(self.path), 0x474C4D4A, 1)
        self.block = self.lib.block().contents

    def boot(self):
        self.lib.LmPreferencesInit()
        return self.block.status

    def request(self, values=None):
        values = {i: i * 100 + 1 for i in range(48)} if values is None else values
        self.block.magic = 0x4C4D5046
        self.block.version = 2
        self.block.presentLo = self.block.presentHi = 0
        for i in range(48):
            self.block.values[i] = values.get(i, 0xFFFFFFFF)
        for i in values:
            if i < 32:
                self.block.presentLo |= 1 << i
            else:
                self.block.presentHi |= 1 << (i - 32)
        self.lib.checksum()
        self.block.requestSeq += 1
        self.assertTrue(self.lib.LmPreferencesPending())

    def service(self):
        self.lib.LmPreferencesService()
        self.assertEqual(self.block.ackSeq, self.block.requestSeq)
        self.assertFalse(self.lib.LmPreferencesPending())
        self.assertEqual((self.lib.metric(1), self.lib.metric(2)), (32, 32))
        return self.block.status

    def test_missing_file_publishes_available_empty_defaults(self):
        self.assertEqual(self.boot(), 1)
        self.assertEqual(self.block.ready, 1)
        self.assertEqual(self.lib.valid(), 1)
        self.assertEqual(ctypes.sizeof(Block), 256)
        self.assertEqual(Block.ackSeq.offset, 32)
        self.assertEqual(Block.values.offset, 64)

    def test_complete_roundtrip_preserves_unknown_ini(self):
        prefix = b"; untouched\r\n[nintendont]\r\nunlock_read_speed = 1\r\n[other]\nx = yep\n"
        self.path.write_bytes(prefix + ini({0: 7}) + b"future_key = value\n")
        self.assertEqual(self.boot(), 0)
        self.request()
        self.assertEqual(self.service(), 0)
        result = self.path.read_bytes()
        self.assertTrue(result.startswith(prefix))
        self.assertIn(b"future_key = value\n", result)
        self.assertEqual(result.count(b"metadata = "), 1)
        self.assertEqual(self.boot(), 0)
        self.assertEqual(list(self.block.values), [i * 100 + 1 for i in range(48)])
        self.assertIn(b"version = 2\r\n", result)
        self.assertIn(b"timer_run_in_menus = 0x000011F9\r\n", result)
        self.assertIn(b"reset_room_bind = 0x0000125D\r\n", result)
        self.assertEqual(self.backup.read_bytes(), prefix + ini({0: 7}) + b"future_key = value\n")

    def test_partial_presence_and_signed_negative_one_are_distinct(self):
        self.path.write_bytes(ini({8: 0xFFFFFFFF, 33: 0x123456, 45: 1}))
        self.assertEqual(self.boot(), 0)
        self.assertEqual(self.block.presentLo, 1 << 8)
        self.assertEqual(self.block.presentHi, (1 << 1) | (1 << 13))
        self.request({8: 0xFFFFFFFF, 33: 0x123456, 45: 1})
        self.assertEqual(self.service(), 0)
        self.assertEqual(self.boot(), 0)
        self.assertEqual(self.block.values[8], 0xFFFFFFFF)

    def test_repeated_saves_keep_one_section_and_stable_bytes(self):
        prefix = b"[nintendont]\nuntouched = value\n"
        suffix = b"future_key = retain me\n[other]\nlast = no newline"
        self.path.write_bytes(prefix + ini({0: 1}) + suffix)
        self.boot()
        expected = None
        for _ in range(10):
            self.request()
            self.assertEqual(self.service(), 0)
            result = self.path.read_bytes()
            self.assertEqual(result.count(b"[lm_preferences]"), 1)
            self.assertTrue(result.startswith(prefix))
            self.assertTrue(result.endswith(suffix))
            if expected is None:
                expected = result
            self.assertEqual(result, expected)
            self.assertEqual(self.boot(), 0)

    def test_unterminated_own_header_and_missing_section(self):
        for original in [b"[lm_preferences]", b"[nintendont]\nx = no newline"]:
            with self.subTest(original=original):
                self.path.write_bytes(original)
                self.boot()
                self.request()
                self.assertEqual(self.service(), 0)
                self.assertEqual(self.path.read_bytes().count(b"[lm_preferences]"), 1)
                self.assertEqual(self.boot(), 0)

    def test_decimal_signed_and_comments(self):
        data = ini({8: 0xFFFFFFFF, 9: 123}).replace(b"0xFFFFFFFF", b"-1 ; signed")
        data = data.replace(b"0x0000007B", b"123 # decimal")
        self.assertEqual(self.lib.parse(data, len(data)), 0)

    def test_invalid_ini_rejected(self):
        good = ini({0: 1})
        for data in [good.replace(b"version = 2", b"version = 3"),
                     good + b"metadata = 1\n", good + b"version = 2\n",
                     good.replace(b"0x00000001", b"4294967296"),
                     good.replace(b"0x00000001", b"-2147483649"),
                     good.replace(b"0x00000001", b"0xgg"),
                     good.replace(b"0x00000001", b"0x00000002"),
                     good + b"\0", b"[lm_preferences]\nmetadata = 1\n"]:
            with self.subTest(data=data):
                self.assertEqual(self.lib.parse(data, len(data)), 2)

    def test_unknown_section_without_preferences_is_missing(self):
        data = b"[nintendont]\nmetadata = unrelated\n"
        self.assertEqual(self.lib.parse(data, len(data)), 1)

    def test_v1_full_and_partial_migrate_without_writing_or_inventing_new_presence(self):
        for values in ({i: i * 3 for i in range(46)}, {8: 0xFFFFFFFF, 45: 1}, {}):
            with self.subTest(values=values):
                old = b"; legacy settings\n[nintendont]\nprivate_launcher_key = retained\n" + ini(values, 1)
                self.path.write_bytes(old)
                self.assertEqual(self.boot(), 0)
                self.assertEqual(self.block.version, 2)
                self.assertEqual(self.lib.valid(), 1)
                self.assertEqual(self.block.presentHi & 0xC000, 0)
                self.assertEqual(list(self.block.values)[46:], [0xFFFFFFFF, 0xFFFFFFFF])
                for index, value in values.items():
                    self.assertEqual(self.block.values[index], value)
                self.assertEqual(self.path.read_bytes(), old)
                migrated = dict(values)
                migrated.update({46: 0, 47: 0})
                self.request(migrated)
                self.assertEqual(self.service(), 0)
                self.assertEqual(self.backup.read_bytes(), old)
                self.assertIn(b"private_launcher_key = retained\n", self.path.read_bytes())
                self.assertEqual(self.boot(), 0)
                self.assertEqual(self.block.presentHi & 0xC000, 0xC000)

    def test_v1_crc_domain_and_reserved_tail_are_authenticated(self):
        good = ini({0: 1, 45: 1}, 1)
        cases = [good.replace(b"metadata = 0x00000001", b"metadata = 0x00000000"),
                 good.replace(b"version = 1", b"version = 2"),
                 ini({0: 1, 45: 1}, 2).replace(b"version = 2", b"version = 1"),
                 good + b"timer_run_in_menus = 0\n", good + b"reset_room_bind = 0\n"]
        # Even a matching v1 CRC including the new keys is not a v1 schema.
        for index in (46, 47):
            words = [0xFFFFFFFF] * 46 + [0, 0]
            high = 1 << (index - 32)
            crc = zlib.crc32(struct.pack(">51I", 1, 0, high, *words))
            cases.append(f"[lm_preferences]\nversion = 1\n{KEYS[index]} = 0\nchecksum = 0x{crc:08X}\n".encode())
        for data in cases:
            with self.subTest(data=data):
                self.assertEqual(self.lib.parse(data, len(data)), 2)

    def test_version_and_checksum_keys_do_not_collide_with_new_fields(self):
        for values in ({46: 0, 47: 1}, {46: 1, 47: 0}, {47: 0xFFFFFFFF}):
            data = ini(values)
            self.assertEqual(self.lib.parse(data, len(data)), 0)
            self.assertEqual(self.lib.valid(), 1)
            for index, value in values.items():
                self.assertEqual(self.block.values[index], value)
        data = ini({46: 1, 47: 1}) + b"reset_room_bind = 1\n"
        self.assertEqual(self.lib.parse(data, len(data)), 2)

    def test_v1_wire_and_noncanonical_v2_tail_are_rejected_without_io(self):
        self.boot()
        for field, value in (("version", 1), ("presentHi", 0x10000), ("reserved0a", 1)):
            self.request({46: 0, 47: 0})
            setattr(self.block, field, value)
            self.lib.checksum()
            opens = self.lib.metric(0)
            self.assertEqual(self.service(), 2)
            self.assertEqual(self.lib.metric(0), opens)
        self.request({0: 1})
        self.block.values[46] = 0
        self.lib.checksum()
        self.assertEqual(self.service(), 2)

    def test_corrupt_boot_is_visible_and_does_not_write(self):
        data = ini({0: 1}).replace(b"checksum", b"bad_checksum")
        self.path.write_bytes(data)
        self.assertEqual(self.boot(), 2)
        self.assertEqual(self.path.read_bytes(), data)
        self.assertEqual(self.block.ready, 1)
        self.assertEqual(self.block.presentLo, 0)

    def test_unavailable_device_and_wrong_game_do_not_read(self):
        self.lib.setup(os.fsencode(self.path), 0x474C4D4A, 0)
        self.assertEqual(self.boot(), 4)
        self.assertEqual(self.block.ready, 0)
        self.assertEqual(self.lib.metric(0), 0)
        self.lib.setup(os.fsencode(self.path), 0x474D534A, 1)
        self.block.magic = 1234
        self.boot()
        self.assertEqual(self.block.magic, 1234)
        self.assertEqual(self.lib.metric(0), 0)

    def test_boot_recovers_backup_when_canonical_missing(self):
        self.backup.write_bytes(b"[nintendont]\nx = unchanged\n" + ini({0: 3}))
        self.assertEqual(self.boot(), 0)
        self.assertEqual(self.block.values[0], 3)
        self.request()
        self.assertEqual(self.service(), 0)
        self.assertIn(b"x = unchanged\n", self.path.read_bytes())

    def test_boot_recovers_backup_after_launcher_recreates_own_section(self):
        original = b"[nintendont]\nx = current\n"
        self.path.write_bytes(original)
        self.backup.write_bytes(ini({0: 3}))
        self.assertEqual(self.boot(), 0)
        self.assertEqual(self.block.values[0], 3)
        self.assertEqual(self.path.read_bytes(), original)
        self.request()
        self.assertEqual(self.service(), 0)
        self.assertTrue(self.path.read_bytes().startswith(original))

    def test_oversize_and_short_read_never_truncate_original(self):
        for data, failure, expected in [(b"x" * 32769, 0, 2), (ini({0: 1}), 1, 3)]:
            with self.subTest(failure=failure):
                self.lib.failure(0)
                self.path.write_bytes(data)
                self.boot()
                self.request()
                self.lib.failure(failure)
                self.assertEqual(self.service(), expected)
                self.assertEqual(self.path.read_bytes(), data)

    def test_invalid_wire_does_not_touch_file(self):
        self.path.write_bytes(ini({0: 1}))
        self.boot()
        self.request()
        self.block.checksum ^= 1
        old = self.path.read_bytes()
        opens = self.lib.metric(0)
        self.assertEqual(self.service(), 2)
        self.assertEqual(self.lib.metric(0), opens)
        self.assertEqual(self.path.read_bytes(), old)

    def test_absent_values_must_have_canonical_sentinel(self):
        self.boot()
        self.request({0: 1})
        self.block.values[8] = 0
        self.lib.checksum()
        self.assertEqual(self.service(), 2)
        self.assertFalse(self.path.exists())

    def test_failed_writes_preserve_old_main_or_backup(self):
        for fault in range(2, 9):
            with self.subTest(fault=fault):
                self.lib.failure(0)
                old = b"[nintendont]\nuntouched = yes\n" + ini({0: fault})
                self.path.write_bytes(old)
                self.boot()
                self.request()
                self.lib.failure(fault)
                self.assertEqual(self.service(), 3)
                recoverable = self.path if self.path.exists() else self.backup
                self.assertEqual(recoverable.read_bytes(), old)
                self.lib.failure(0)
                self.assertEqual(self.boot(), 0)
                self.assertEqual(self.block.values[0], fault)

    def test_capacity_overflow_keeps_prior_file(self):
        data = b";" + b"x" * 32000 + b"\n"
        self.path.write_bytes(data)
        self.boot()
        self.request()
        self.assertEqual(self.service(), 3)
        self.assertEqual(self.path.read_bytes(), data)

    def test_allocation_failure_is_nonfatal_and_never_opens_files(self):
        self.path.write_bytes(ini({0: 1}))
        self.lib.failure(9)
        self.assertEqual(self.boot(), 3)
        self.assertEqual(self.block.ready, 1)
        self.assertEqual(self.lib.metric(0), 0)
        self.lib.failure(0)
        self.boot()
        self.request()
        opens = self.lib.metric(0)
        old = self.path.read_bytes()
        self.lib.failure(9)
        self.assertEqual(self.service(), 3)
        self.assertEqual(self.lib.metric(0), opens)
        self.assertEqual(self.path.read_bytes(), old)


if __name__ == "__main__":
    unittest.main()
