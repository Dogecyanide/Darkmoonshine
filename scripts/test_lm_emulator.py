import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_lm_emulator as emulator
import gen_iso_bps as bps
import prepare_lm_dolphin as profile
import write_lm_dtm as movie
import extract_lm_dolphin_state as state_extract


class EmulatorPatchTests(unittest.TestCase):
    def make_patch(self, source):
        builder = bps.BpsBuilder({
            (0, 4): zlib.crc32(source[:4]),
            (8, 4): zlib.crc32(source[8:12]),
        })
        builder.source_read(0, 4)
        builder.target_read(b"LM")
        builder.source_copy(8, 4)
        builder.zeros(100000)
        return builder.encode(len(source), zlib.crc32(source))

    def test_independent_apply_verifies_all_four_action_types(self):
        source = b"abcdefghijklmnop"
        with tempfile.TemporaryDirectory() as directory:
            src, dst = Path(directory) / "source.iso", Path(directory) / "target.iso"
            src.write_bytes(source)
            patch = self.make_patch(source)
            emulator.apply_patch(src, patch, dst)
            self.assertEqual(dst.read_bytes(), b"abcdLMijkl" + b"\0" * 100000)
            self.assertEqual(src.read_bytes(), source)

    def test_wrong_source_refused_before_target_is_opened(self):
        with tempfile.TemporaryDirectory() as directory:
            src, dst = Path(directory) / "source", Path(directory) / "target"
            src.write_bytes(b"wrongwrongwrong!")
            dst.write_bytes(b"untouched")
            with self.assertRaisesRegex(ValueError, "source checksum"):
                emulator.apply_patch(src, self.make_patch(b"abcdefghijklmnop"), dst)
            self.assertEqual(dst.read_bytes(), b"untouched")

    def test_corrupt_patch_and_in_place_apply_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            src, dst = Path(directory) / "source", Path(directory) / "target"
            src.write_bytes(b"abcdefghijklmnop")
            patch = bytearray(self.make_patch(src.read_bytes()))
            patch[10] ^= 0x20
            with self.assertRaisesRegex(ValueError, "patch checksum"):
                emulator.apply_patch(src, bytes(patch), dst)
            self.assertFalse(dst.exists())
            with self.assertRaisesRegex(ValueError, "different paths"):
                emulator.apply_patch(src, bytes(patch), src)

    def test_authenticated_writes_use_replacement_word(self):
        layout = {
            "mod_region_size": 16,
            "fst": {"source_offset": 100, "target_offset": 200, "size": 12},
            "first_file_offset": 300,
            "dol": {"iso_offset": 0, "new_text_slot": 2, "size": 1600},
            "hooks": [{"address": 0x80004000, "iso_offset": 600}],
            "relocated_files": [], "iso_size": 2048,
        }
        manifest = {"code": "60000000", "size": 4, "base_addr": 0x804B8400,
                    "writes": [[0x80004000, 0xDEADBEEF, 0x60000000]]}
        operations = bps.build_operations(layout, manifest)
        write = next(operation for operation in operations if operation["target_offset"] == 600)
        self.assertEqual(write["value"], b"\x60\0\0\0")


class DolphinProfileTests(unittest.TestCase):
    def test_profile_copies_valid_japanese_save_and_refuses_existing_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            save = Path(directory) / "save.gci"
            data = bytearray(64 + 8192)
            data[:6] = b"GLMJ01"
            struct.pack_into(">H", data, 0x38, 1)
            save.write_bytes(data)
            destination = Path(directory) / "isolated"
            profile.prepare(destination, save)
            self.assertEqual((destination / "GC/JAP/Card A/01-GLMJ-LMHiddenMansion.gci").read_bytes(), data)
            self.assertEqual(json.loads((destination / "lm-test-profile.json").read_text())["keys"]["menu"], "Down")
            marker = json.loads((destination / "lm-test-profile.json").read_text())
            self.assertEqual(marker["memory_card_backend"], "gci-folder")
            self.assertTrue(marker["dolphin_5_movie_requires_raw_card"])
            self.assertIn("SlotA = 8", (destination / "Config/Dolphin.ini").read_text())
            with self.assertRaisesRegex(ValueError, "existing Dolphin profiles"):
                profile.prepare(destination, save)

    def test_wrong_region_and_truncated_gci_are_refused_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            save, destination = Path(directory) / "save", Path(directory) / "isolated"
            data = bytearray(64)
            data[:6] = b"GLME01"
            save.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "Japanese"):
                profile.prepare(destination, save)
            self.assertFalse(destination.exists())
            data[:6] = b"GLMJ01"
            struct.pack_into(">H", data, 0x38, 1)
            save.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "block count"):
                profile.prepare(destination, save)
            self.assertFalse(destination.exists())


class InputMovieTests(unittest.TestCase):
    def test_held_buttons_and_neutral_sticks_use_dolphin_wire_format(self):
        with tempfile.TemporaryDirectory() as directory:
            iso = Path(directory) / "game.iso"
            iso.write_bytes(b"GLMJ01 test source")
            result = movie.build_movie(iso, [{"polls": 3, "buttons": ["A", "Left"]}],
                                       allow_empty_card=True)
            self.assertEqual(result[:4], b"DTM\x1a")
            self.assertEqual(struct.unpack_from("<Q", result, 21)[0], 3)
            self.assertEqual(struct.unpack_from("<Q", result, 13)[0], (1 << 63) - 1)
            self.assertGreater(struct.unpack_from("<Q", result, 237)[0], 0)
            self.assertEqual(result[149:153], bytes((1, 1, 1, 0)))
            self.assertEqual(result[256:], struct.pack("<H6B", 0x102, 0, 0, 128, 128, 128, 128) * 3)


class NativeStateTests(unittest.TestCase):
    def test_block_sizes_and_last_partial_block_follow_native_header(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "native.s02"
            header = bytearray(24)
            header[:6] = b"GLMJ01"
            struct.pack_into("<I", header, 8, 131075)
            source.write_bytes(header + struct.pack("<I", 1) + b"a" + struct.pack("<I", 1) + b"b")
            sizes = []
            def fake_decode(data, size):
                sizes.append(size)
                return data * size
            with patch.dict(sys.modules, {"lzokay": SimpleNamespace(decompress=fake_decode)}):
                self.assertEqual(state_extract.decode(source), b"a" * 131072 + b"bbb")
            self.assertEqual(sizes, [131072, 3])

    def test_native_extraction_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "capture"
            destination.mkdir()
            existing = destination / "mem1.bin"
            existing.write_bytes(b"existing checkpoint")
            with self.assertRaisesRegex(ValueError, "new or empty"):
                state_extract.extract(Path(directory) / "absent", destination)
            self.assertEqual(existing.read_bytes(), b"existing checkpoint")


if __name__ == "__main__":
    unittest.main()
