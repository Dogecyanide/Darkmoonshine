"""Native regression tests for the Japanese retail idle-resource exception."""
from __future__ import annotations

import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
INACTIVE = 0xFFFFFFFF
SAVED_IDLE = struct.pack(
    ">16I", 0xF0, 0x2AF4, 0, 0, 0, 0, 0x2A8200, 0, 0, 0, 0, 0, 0, 0, 0, 0x2B
)
LIVE_IDLE = struct.pack(
    ">16I", 0xF0, 0x80011B28, 0, 0, 0, 0, 0x8113CFA0, 0,
    0, 0, 0, 0, 0, 0, 0, 0x2B
)


class LmStateResourceNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-resource-")
        output = Path(cls.temp.name) / ("resource.dll" if os.name == "nt" else "resource.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_state_resource_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.equivalent.argtypes = [ctypes.c_uint, ctypes.c_uint,
                                       ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.equivalent.restype = ctypes.c_int
        cls.lib.changes_match.argtypes = [ctypes.c_uint, ctypes.c_uint] + [ctypes.c_void_p] * 4
        cls.lib.changes_match.restype = ctypes.c_int
        cls.lib.reload_changes.argtypes = ([ctypes.c_uint, ctypes.c_uint] +
                                           [ctypes.c_void_p] * 4 + [ctypes.c_ulong] * 5)
        cls.lib.reload_changes.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def equivalent(self, saved=SAVED_IDLE, live=LIVE_IDLE,
                   saved_id=INACTIVE, live_id=INACTIVE):
        return self.lib.equivalent(saved_id, live_id, saved, live)

    def changes(self, active=0x3F, records=0x7F, saved=SAVED_IDLE,
                live=LIVE_IDLE, saved_id=INACTIVE, live_id=INACTIVE):
        saved_ids = (ctypes.c_ulong * 7)(2, 30, 6, INACTIVE, INACTIVE, INACTIVE, saved_id)
        live_ids = (ctypes.c_ulong * 7)(34, 25, 30, 33, 29, 24, live_id)
        return self.lib.changes_match(active, records, saved_ids, live_ids,
                                      bytes(6 * 64) + saved, bytes(6 * 64) + live)

    def test_captured_slot6_stale_callback_and_relative_backing(self):
        self.assertEqual(self.equivalent(), 1)
        self.assertEqual(self.changes(), 1)
        self.assertEqual(self.equivalent(LIVE_IDLE, SAVED_IDLE), 1)

    def test_both_ids_must_be_inactive(self):
        for saved_id, live_id in ((1, 1), (0, INACTIVE), (INACTIVE, 3), (6, 6)):
            self.assertEqual(self.equivalent(saved_id=saved_id, live_id=live_id), 0)
            self.assertEqual(self.changes(saved_id=saved_id, live_id=live_id), 0)

    def test_pending_or_completed_cleanup_state_is_not_idle(self):
        for state in (1, 2, 3, 255):
            for side in (0, 1):
                pair = [bytearray(SAVED_IDLE), bytearray(LIVE_IDLE)]
                pair[side][2] = state
                self.assertEqual(self.equivalent(*map(bytes, pair)), 0)
        pair = [bytearray(SAVED_IDLE), bytearray(LIVE_IDLE)]
        pair[0][2] = pair[1][2] = 2
        self.assertEqual(self.equivalent(*map(bytes, pair)), 0)

    def test_equal_nonzero_auxiliary_pointer_is_not_idle(self):
        for offset in range(8, 0x18):
            pair = [bytearray(SAVED_IDLE), bytearray(LIVE_IDLE)]
            pair[0][offset] = pair[1][offset] = 1
            self.assertEqual(self.equivalent(*map(bytes, pair)), 0)

    def test_every_other_record_byte_still_rejects(self):
        ignored = {3, 0x3E, 0x3F} | set(range(4, 8)) | set(range(0x18, 0x1C))
        for offset in range(64):
            live = bytearray(LIVE_IDLE)
            live[offset] ^= 0x5A
            with self.subTest(offset=offset):
                self.assertEqual(self.equivalent(live=bytes(live)), int(offset in ignored))
                self.assertEqual(self.changes(live=bytes(live)), int(offset in ignored))

    def test_native_padding_can_differ_in_both_directions_after_reallocation(self):
        saved, live = bytearray(SAVED_IDLE), bytearray(LIVE_IDLE)
        for offset, left, right in ((3, 0xF0, 0x29), (0x3E, 0xEF, 0x51), (0x3F, 0x2B, 0xC8)):
            saved[offset], live[offset] = left, right
        self.assertEqual(self.equivalent(bytes(saved), bytes(live)), 1)
        self.assertEqual(self.equivalent(bytes(live), bytes(saved)), 1)

    def reload_idle_records(self, mutate=None):
        saved_ids = (ctypes.c_ulong * 7)(2, 30, 6, 34, INACTIVE, INACTIVE, INACTIVE)
        live_ids = (ctypes.c_ulong * 7)(39, 34, 36, 7, INACTIVE, INACTIVE, INACTIVE)
        saved = bytearray(bytes(4 * 64) + SAVED_IDLE * 3)
        live = bytearray(bytes(4 * 64) + LIVE_IDLE * 3)
        for slot in range(4, 7):
            for offset in (3, 0x3E, 0x3F):
                live[slot * 64 + offset] ^= slot + offset
        if mutate:
            mutate(saved_ids, live_ids, saved, live)
        return self.lib.reload_changes(0x0F, 0x7F, saved_ids, live_ids,
                                       bytes(saved), bytes(live), 0x80C10000,
                                       0x80C20000, 0x70800, 0x80C00000, 0x817F0000)

    def test_scene_reload_idle_padding_matches_reported_mask_shape(self):
        # Synthetic endpoint bytes: the user's journal records masks, not rows.
        self.assertEqual(self.reload_idle_records(), 1)

    def test_reload_padding_exception_never_accepts_active_or_semantic_drift(self):
        for slot in range(4, 7):
            for offset in (0, 1, 2, 8, 0x0C, 0x10, 0x14, 0x1C, 0x20, 0x3C, 0x3D):
                with self.subTest(slot=slot, offset=offset):
                    def mutate(_saved_ids, _live_ids, _saved, live):
                        live[slot * 64 + offset] ^= 1
                    self.assertEqual(self.reload_idle_records(mutate), 0)
            def active_ids(saved_ids, live_ids, _saved, _live):
                saved_ids[slot] = live_ids[slot] = 42
            self.assertEqual(self.reload_idle_records(active_ids), 0)

    def test_no_record_change_for_active_id_change_still_rejects(self):
        self.assertEqual(self.changes(active=0x7F, records=0x3F), 0)
        self.assertEqual(self.changes(active=1, records=0), 0)

    def test_existing_matching_active_and_record_masks_remain_allowed(self):
        self.assertEqual(self.changes(active=0x3F, records=0x3F), 1)
        self.assertEqual(self.changes(active=0, records=0), 1)
        self.assertEqual(self.changes(active=0x7F, records=0x7F), 1)

    def test_bits_outside_seven_slots_are_rejected(self):
        for mask in (0x80, 0xFF, 0x80000000, 0xFFFFFFFF):
            self.assertEqual(self.changes(active=mask, records=mask), 0)

    def test_integrated_guard_keeps_ownership_and_map_checks(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        guard = source.split("bool guardedCrossRoomRestoreAllowed(", 1)[1].split(
            "void repairSavedVolumeList", 1)[0]
        for condition in ("!resourceLayoutCompatible(header, live)",
                          "sResourceDiff.mapChanged != 0u",
                          "sSavedResourceCensus.backingBadMask != 0u",
                          "sLiveResourceCensus.backingBadMask != 0u",
                          "sSavedResourceCensus.wantedCount != 0u",
                          "sLiveResourceCensus.wantedCount != 0u",
                          "!resourceReplacementMatches(header, live)"):
            self.assertIn(condition, guard)
        replacement = source.split("bool resourceReplacementMatches(", 1)[1].split(
            "bool guardedCrossRoomRestoreAllowed", 1)[0]
        self.assertIn("header->heapStart, header->heapEnd", replacement)
        self.assertIn("live.heapStart, live.heapEnd", replacement)
        self.assertIn("size > header->heapDataSize - savedOffset", replacement)


class LmResourceLifecycleNativeEvidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = ROOT / "build-lm-diag/clean_glmj_main.dol"
        if not path.exists():
            raise unittest.SkipTest("Maintainer's authenticated clean Japanese DOL unavailable")
        raw = path.read_bytes()
        if hashlib.sha1(raw).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise RuntimeError("Resource lifecycle evidence requires clean GLMJ01 revision 0")
        from dolreader.dol import DolFile
        with path.open("rb") as source:
            cls.dol = DolFile(source)

    def word(self, address):
        self.dol.seek(address)
        return int.from_bytes(self.dol.read(4), "big")

    def test_constructor_and_cleanup_leave_exact_padding_callback_backing_gaps(self):
        # rt=0 stores: (opcode, offset, width). Every constructor instruction
        # is authenticated, so an omitted store cannot hide in this audit.
        stores = [(36, offset, 4) for offset in range(0x1C, 0x3C, 4)] + [
            (44, 0, 2), (38, 2, 1), (38, 0x3C, 1), (38, 0x3D, 1),
            (36, 0x0C, 4), (36, 0x10, 4), (36, 0x14, 4), (36, 8, 4)]
        self.assertEqual(self.word(0x8001F928), 0x38000000)
        self.assertEqual(self.word(0x8001F96C), 0x4E800020)
        self.assertEqual(self.word(0x8001F1A0), 0x38000000)
        cleared = set()
        for index, (opcode, offset, width) in enumerate(stores):
            self.assertEqual(self.word(0x8001F92C + index * 4), opcode << 26 | 3 << 16 | offset)
            self.assertEqual(self.word(0x8001F1A4 + index * 4), opcode << 26 | 31 << 16 | offset)
            cleared.update(range(offset, offset + width))
        self.assertEqual(set(range(64)) - cleared,
                         {3, 0x3E, 0x3F} | set(range(4, 8)) | set(range(0x18, 0x1C)))

    def test_native_idle_branch_and_reuse_overwrites_are_authenticated(self):
        expected = {
            0x8001F138: 0x88030002,  # cleanup reads state byte, not padding +3
            0x8001F144: 0xA01F0000,  # flags are a halfword, excluding +2/+3
            0x8001F2D8: 0x98030002,  # reuse publishes pending state
            0x8001F2DC: 0x93C30018,  # replaces backing pointer
            0x8001F378: 0x93BB0004,  # replaces callback
            0x80011CD8: 0x7C60D214,  # record array + slot * 0x40
            0x80011CDC: 0x88030002,  # poll reads state byte
            0x80011CF0: 0x4800D435,  # completed inactive slot -> cleanup
        }
        for address, word in expected.items():
            self.assertEqual(self.word(address), word, hex(address))


if __name__ == "__main__":
    unittest.main()
