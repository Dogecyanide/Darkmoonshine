"""DTM card preflight tests; all card data below is synthetic."""

import contextlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import write_lm_dtm as movie


def fix_checksums(data):
    for start, end, destination in ((0, 0x1fc, 0x1fc), (0x2000, 0x3ffc, 0x3ffc),
                                    (0x4000, 0x5ffc, 0x5ffc), (0x6004, 0x8000, 0x6000),
                                    (0x8004, 0xa000, 0x8000)):
        checksum = inverse = 0
        for offset in range(start, end, 2):
            word = (data[offset] << 8) | data[offset + 1]
            checksum = (checksum + word) & 0xffff
            inverse = (inverse + (word ^ 0xffff)) & 0xffff
        struct.pack_into(">HH", data, destination,
                         checksum if checksum != 0xffff else 0,
                         inverse if inverse != 0xffff else 0)


def synthetic_card():
    data = bytearray(b"\xff" * (4 * 131072))
    struct.pack_into(">HH", data, 0x22, 4, 1)
    for directory in (0x2000, 0x4000):
        data[directory:directory + 6] = b"GLMJ01"
        struct.pack_into(">HH", data, directory + 0x36, 5, 1)
        struct.pack_into(">H", data, directory + 0x1ffa, 1)
    for bat in (0x6000, 0x8000):
        data[bat:bat + 0x2000] = b"\0" * 0x2000
        struct.pack_into(">HHHH", data, bat + 4, 1, 58, 5, 0xffff)
    data[0xa000:0xa00e] = b"synthetic save"
    fix_checksums(data)
    return data


class MovieCardTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.card = self.root / "MemoryCardA.JAP.raw"
        self.iso = self.root / "game.iso"
        self.iso.write_bytes(b"GLMJ01 synthetic ISO")

    def write_card(self, data=None):
        self.card.write_bytes(synthetic_card() if data is None else data)
        return self.card

    def test_explicit_card_decision_required_before_reading_iso_even_from_state(self):
        for from_state in (False, True):
            with self.subTest(from_state=from_state):
                with self.assertRaisesRegex(ValueError, "ignores GCI folders"):
                    movie.build_movie(self.root / "absent.iso", [], from_state)

    def test_verified_card_is_read_only_and_dtm_never_clears_it(self):
        self.write_card()
        original = self.card.read_bytes()
        result = movie.build_movie(self.iso, [{"polls": 1}], raw_card=self.card)
        self.assertEqual(self.card.read_bytes(), original)
        self.assertEqual(result[137], 1)
        self.assertEqual(result[151:153], b"\x01\x00")
        self.assertEqual(result[137:158], bytes((
            1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0)))

    def test_allow_empty_is_explicit_and_does_not_request_clear_save(self):
        result = movie.build_movie(self.iso, [{"polls": 1}], True, allow_empty_card=True)
        self.assertEqual(result[12], 1)
        self.assertEqual(result[151:153], b"\x01\x00")
        self.assertFalse(self.card.exists())

    def test_override_cannot_hide_an_invalid_explicit_card(self):
        with self.assertRaisesRegex(ValueError, "not both"):
            movie.build_movie(self.iso, [], raw_card=self.card, allow_empty_card=True)

    def test_missing_card_folder_gci_and_wrong_size_are_rejected(self):
        for path in (self.card, self.root):
            with self.assertRaisesRegex(ValueError, "existing RAW card"):
                movie.validate_raw_card(path)
        for data in (b"GLMJ01" + bytes(64 + 8192 - 6), bytes(1024)):
            self.write_card(data)
            with self.assertRaisesRegex(ValueError, "size is invalid"):
                movie.validate_raw_card(self.card)

    def test_header_size_and_japanese_encoding_are_checked(self):
        for offset, value, error in ((0x22, 8, "header size"), (0x24, 0, "Japanese encoding")):
            data = synthetic_card()
            struct.pack_into(">H", data, offset, value)
            fix_checksums(data)
            self.write_card(data)
            with self.assertRaisesRegex(ValueError, error):
                movie.validate_raw_card(self.card)

    def test_each_metadata_checksum_is_checked_without_repair(self):
        for offset in (0x10, 0x2010, 0x4010, 0x6010, 0x8010):
            with self.subTest(offset=offset):
                data = synthetic_card()
                data[offset] ^= 1
                self.write_card(data)
                with self.assertRaisesRegex(ValueError, "metadata checksum"):
                    movie.validate_raw_card(self.card)
                self.assertEqual(self.card.read_bytes(), data)

    def test_active_directory_not_stale_backup_determines_save_presence(self):
        data = synthetic_card()
        data[0x4000:0x4040] = b"\xff" * 64
        fix_checksums(data)
        self.write_card(data)
        with self.assertRaisesRegex(ValueError, "no active GLMJ01"):
            movie.validate_raw_card(self.card)
        struct.pack_into(">H", data, 0x3ffa, 2)
        fix_checksums(data)
        self.write_card(data)
        self.assertEqual(movie.validate_raw_card(self.card), self.card.resolve())

    def test_foreign_region_save_is_not_japanese_completion(self):
        data = synthetic_card()
        for offset in (0x2000, 0x4000):
            data[offset:offset + 6] = b"GLME01"
        fix_checksums(data)
        self.write_card(data)
        with self.assertRaisesRegex(ValueError, "no active GLMJ01"):
            movie.validate_raw_card(self.card)

    def test_invalid_save_count_chain_and_erased_payload_are_rejected(self):
        for kind, error in (("count", "block count"), ("start", "block chain"),
                            ("cycle", "block chain"), ("length", "match its length"),
                            ("erased", "data is erased")):
            with self.subTest(kind=kind):
                data = synthetic_card()
                if kind == "count":
                    struct.pack_into(">H", data, 0x4038, 0)
                elif kind == "start":
                    struct.pack_into(">H", data, 0x4036, 4)
                elif kind == "cycle":
                    struct.pack_into(">H", data, 0x4038, 2)
                    struct.pack_into(">H", data, 0x800a, 5)
                elif kind == "length":
                    struct.pack_into(">H", data, 0x800a, 6)
                else:
                    data[0xa000:0xc000] = b"\xff" * 0x2000
                fix_checksums(data)
                self.write_card(data)
                with self.assertRaisesRegex(ValueError, error):
                    movie.validate_raw_card(self.card)

    def test_primary_bat_is_used_only_when_newer(self):
        data = synthetic_card()
        struct.pack_into(">H", data, 0x800a, 0)
        fix_checksums(data)
        self.write_card(data)
        with self.assertRaisesRegex(ValueError, "match its length"):
            movie.validate_raw_card(self.card)
        struct.pack_into(">H", data, 0x6004, 2)
        fix_checksums(data)
        self.write_card(data)
        self.assertEqual(movie.validate_raw_card(self.card), self.card.resolve())

    def test_cli_refuses_card_overwrite_and_preserves_output_on_bad_card(self):
        self.write_card()
        original = self.card.read_bytes()
        steps = self.root / "steps.json"
        steps.write_text('[{"polls": 1}]')
        arguments = ["write_lm_dtm.py", "--iso", str(self.iso), "--steps", str(steps),
                     "--raw-card", str(self.card), "--output", str(self.card)]
        with patch.object(sys, "argv", arguments):
            with self.assertRaisesRegex(ValueError, "must differ"):
                movie.main()
        self.assertEqual(self.card.read_bytes(), original)
        output = self.root / "existing.dtm"
        output.write_bytes(b"untouched")
        self.card.write_bytes(b"not a card")
        arguments[-1] = str(output)
        with patch.object(sys, "argv", arguments):
            with self.assertRaisesRegex(ValueError, "size is invalid"):
                movie.main()
        self.assertEqual(output.read_bytes(), b"untouched")

    def test_cli_omitted_card_choice_never_writes(self):
        output = self.root / "new.dtm"
        arguments = ["write_lm_dtm.py", "--iso", str(self.iso),
                     "--steps", "absent.json", "--output", str(output)]
        with patch.object(sys, "argv", arguments), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                movie.main()
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
