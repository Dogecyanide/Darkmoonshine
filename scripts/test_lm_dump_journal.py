#!/usr/bin/env python3
"""Host contracts for the GLMJ hard-lock phase journal."""

from pathlib import Path
from contextlib import redirect_stdout
import ctypes
import importlib.util
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "launcher" / "kernel" / "SusamuneCrash.c").read_text(
    encoding="utf-8"
)
PARSER_PATH = ROOT / "scripts" / "read_lm_dump.py"
spec = importlib.util.spec_from_file_location("read_lm_dump_test", PARSER_PATH)
assert spec and spec.loader
lm_dump = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = lm_dump
spec.loader.exec_module(lm_dump)


def phase_record(
    sequence: int,
    action: int = 1,
    phase: int = lm_dump.SAVE_COMPLETE_PHASE,
    arg0: int = 0x12345678,
    arg1: int = 3,
) -> bytes:
    return lm_dump.PHASE.pack(
        lm_dump.PHASE_MAGIC,
        sequence,
        action,
        phase,
        phase ^ lm_dump.U32_MASK,
        arg0,
        arg1,
        sequence,
    )


def journal_bytes(
    generation: int = 1,
    records: tuple[bytes, ...] = (phase_record(2),),
    trailing: bytes = b"",
    version: int = lm_dump.HEADER_VERSION,
) -> bytes:
    first_sequence = lm_dump.PHASE.unpack(records[0])[1]
    header = lm_dump.HEADER.pack(
        lm_dump.HEADER_MAGIC,
        version,
        lm_dump.HEADER.size,
        generation,
        generation ^ lm_dump.U32_MASK,
        lm_dump.GAME_ID_GLMJ,
        0x89ABCDEF,
        lm_dump.PHASE.size,
        first_sequence,
    )
    return header + b"".join(records) + trailing


class LuigiMansionDumpJournalContracts(unittest.TestCase):
    def test_parser_names_the_exact_unmatched_volume_side_and_native_owner(self) -> None:
        records = [phase_record(2)]
        for side in (0, 0x80000000):
            records.append(phase_record(len(records) * 2 + 2, action=2,
                phase=0xD9, arg0=side | 20, arg1=0x80C39910))
            records.append(phase_record(len(records) * 2 + 2, action=2,
                phase=0xDA, arg0=0x907F0B39, arg1=0x80C399A0))
        for fault in (0x46, 0x47):
            records.append(phase_record(len(records) * 2 + 2, action=2,
                phase=0xD8, arg0=fault, arg1=0x804A17C8))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(lm_dump.parse_journal_bytes(
                journal_bytes(records=tuple(records))), latest=False)
        decoded = output.getvalue()
        for side in ('saved', 'live'):
            self.assertIn(f'unmatched_volume={side} census_index=20 object=80C39910', decoded)
            self.assertIn(f'ownership_fault={side}-map-archive-owner value=804A17C8', decoded)
        self.assertIn('unmatched_volume_name_hash=907F0B39 backing=80C399A0', decoded)

    def test_lm_crash_reports_use_game_specific_names(self) -> None:
        self.assertIn('"%s/luigis_mansion_crash_a.bin"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_b.bin"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_a.txt"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_b.txt"', SOURCE)
        self.assertIn(
            'LM_BRANDING_NAME " Luigi\'s Mansion crash report v%u\\r\\n"', SOURCE
        )
        self.assertIn("if (GAME_ID == SUSAMUNE_MOD_GAME_ID_LMJ)", SOURCE)

    def test_journal_is_glmj_only_and_uses_eight_rotating_files(self) -> None:
        self.assertIn('"%s/lm_dumps"', SOURCE)
        self.assertIn('"%s/lm_attempt_%c.bin"', SOURCE)
        self.assertIn("#define SUSAMUNE_LM_DUMP_SLOTS 8u", SOURCE)
        self.assertIn("LmDumpPaths[SUSAMUNE_LM_DUMP_SLOTS][64]", SOURCE)
        self.assertIn("GAME_ID != SUSAMUNE_MOD_GAME_ID_LMJ", SOURCE)
        self.assertIn("target = LmDumpTarget();", SOURCE)
        self.assertNotIn("f_unlink", SOURCE)

    def test_directory_discovers_eight_canonical_names_and_legacy_pair(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in lm_dump.ATTEMPT_NAMES[:2]:
                (root / name).write_bytes(journal_bytes())
            self.assertEqual([p.name for p in lm_dump.input_paths([directory])],
                             list(lm_dump.ATTEMPT_NAMES[:2]))
            for name in (*lm_dump.ATTEMPT_NAMES[2:], "lm_attempt_i.bin",
                         "lm_attempt_a.bin.tmp", "unrelated.bin"):
                (root / name).write_bytes(journal_bytes())
            self.assertEqual([p.name for p in lm_dump.input_paths([directory])],
                             list(lm_dump.ATTEMPT_NAMES))

    def test_parser_distinguishes_camera_profile_and_postload_heap_phases(self) -> None:
        records = [phase_record(2)]
        for phase in range(0xF4, 0xFB):
            records.append(phase_record(len(records) * 2 + 2, action=2,
                phase=phase, arg0=0x100, arg1=0xABCDEF01))
        records.extend((phase_record(18, action=1, phase=0xF9, arg0=0, arg1=0),
                        phase_record(20, action=3, phase=0xF4, arg0=7, arg1=8)))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(lm_dump.parse_journal_bytes(
                journal_bytes(records=tuple(records))), latest=False)
        text = output.getvalue()
        for index in range(3):
            self.assertIn(f"camera[{index}] saved=00000100 live=ABCDEF01", text)
        self.assertIn("camera_saved_fault address=00000100 value=ABCDEF01", text)
        self.assertIn("camera_live_fault address=00000100 value=ABCDEF01", text)
        self.assertIn("reboot_profile_check=load fault=00000100 value=ABCDEF01", text)
        self.assertIn("reboot_profile_check=save fault=00000000 value=00000000", text)
        self.assertIn("reboot_profile_mismatch word=256 live_value=ABCDEF01", text)
        self.assertIn("heap_fault address=00000007 value=00000008", text)

    def test_successful_save_starts_a_new_attempt(self) -> None:
        record = re.search(
            r"static void RecordLmDump\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            SOURCE,
            re.DOTALL,
        )
        self.assertIsNotNone(record)
        body = record.group("body")
        self.assertIn("trace->action == SUSAMUNE_PHASE_ACTION_SAVE", body)
        self.assertIn("trace->phase == SUSAMUNE_LM_SAVE_COMPLETE_PHASE", body)
        self.assertIn("BeginLmDump(trace);", body)

    def test_header_and_first_record_are_required_for_recovery(self) -> None:
        self.assertIn("sizeof(struct SusamuneLmDumpHeader) == 32", SOURCE)
        self.assertIn("header->generation ^ header->generationInverse", SOURCE)
        self.assertIn("ValidPhaseTrace(&first)", SOURCE)
        self.assertIn("first.sequenceBegin == header->saveSequence", SOURCE)

    def test_each_observed_record_is_synced_before_debug_text(self) -> None:
        poll = re.search(
            r"static void EmitPhaseTrace\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            SOURCE,
            re.DOTALL,
        )
        self.assertIsNotNone(poll)
        body = poll.group("body")
        self.assertLess(body.index("RecordLmDump(&snapshot);"), body.index("dbgprintf("))
        self.assertRegex(
            SOURCE,
            r"if \(ret == FR_OK && sync\)\s*ret = f_sync\(&LmDumpFile\);",
        )

    def test_parser_preserves_exact_records_and_reports_a_torn_tail(self) -> None:
        records = (
            phase_record(2),
            phase_record(4, action=2, phase=0x7F, arg0=0xAABBCCDD),
            phase_record(6, action=3, phase=0xE0, arg1=0x8005FF8C),
        )
        journal = lm_dump.parse_journal_bytes(
            journal_bytes(7, records, trailing=b"torn"), "attempt.bin"
        )
        self.assertEqual(journal.generation, 7)
        self.assertEqual(journal.mod_crc32, 0x89ABCDEF)
        self.assertEqual(journal.records[2].phase, 0xE0)
        self.assertEqual(journal.records[2].arg1, 0x8005FF8C)
        self.assertEqual(journal.trailing_bytes, 4)
        self.assertTrue(all(record.valid for record in journal.records))

    def test_parser_rejects_a_torn_header_or_non_save_first_record(self) -> None:
        torn = bytearray(journal_bytes())
        torn[12:16] = b"\0\0\0\0"
        with self.assertRaises(lm_dump.DumpError):
            lm_dump.parse_journal_bytes(bytes(torn))
        with self.assertRaises(lm_dump.DumpError):
            lm_dump.parse_journal_bytes(
                journal_bytes(records=(phase_record(2, action=2),))
            )

    def test_parser_uses_wrapping_generation_order(self) -> None:
        self.assertTrue(lm_dump.generation_is_newer(1, 0xFFFFFFFF))
        self.assertFalse(lm_dump.generation_is_newer(0xFFFFFFFF, 1))

    def test_legacy_save_anchor_and_v2_cold_load_anchor_are_supported(self):
        legacy = lm_dump.parse_journal_bytes(journal_bytes(version=1))
        self.assertEqual(legacy.version, 1)
        records = (phase_record(2, action=2, phase=1),
                   phase_record(4, action=2, phase=0xD3, arg0=6 << 24))
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        self.assertEqual(journal.version, 2)
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        self.assertIn("anchor=load sequence=2", output.getvalue())
        self.assertIn("refusal=epoch", output.getvalue())
        with self.assertRaises(lm_dump.DumpError):
            lm_dump.parse_journal_bytes(journal_bytes(records=records, version=1))
        with self.assertRaises(lm_dump.DumpError):
            lm_dump.parse_journal_bytes(journal_bytes(records=records[1:]))

    def test_warp_lifecycle_is_decoded_but_cannot_anchor_a_journal(self) -> None:
        records = (phase_record(2), phase_record(4, action=4, phase=0x10),
                   phase_record(6, action=4, phase=0x7F))
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        self.assertTrue(all(record.valid for record in journal.records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        self.assertIn("warp_event=dispatch", output.getvalue())
        self.assertIn("warp_event=arrived", output.getvalue())
        with self.assertRaises(lm_dump.DumpError):
            lm_dump.parse_journal_bytes(journal_bytes(records=(records[1],)))

    def test_critical_drain_precedes_sample_and_sample_cannot_rotate_twice(self) -> None:
        self.assertIn("LmCriticalPeek(ring, &snapshot, &CriticalIo)", SOURCE)
        self.assertIn("LmCriticalAcknowledge(ring, &CriticalIo)", SOURCE)
        self.assertIn("count < 4u", SOURCE)
        self.assertIn("LmPhaseCritical(&snapshot))) return;", SOURCE)
        self.assertIn("LastCriticalPhaseSequence", SOURCE)
        self.assertIn("critical phase queue overflow dropped=%u", SOURCE)

    def test_parser_decodes_grain_checks_and_preserves_fault_words(self) -> None:
        records = (
            phase_record(2),
            phase_record(4, action=1, phase=0x5F, arg0=0, arg1=0),
            phase_record(6, action=2, phase=0x07, arg0=0, arg1=0),
            phase_record(8, action=2, phase=0x6B,
                         arg0=0x8129E524, arg1=0xFFFFFFFF),
        )
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        text = output.getvalue()
        self.assertIn("grain=save-source:ok", text)
        self.assertIn("grain=saved-source:ok", text)
        self.assertIn("grain=restored-copy:invalid", text)
        self.assertEqual(journal.records[-1].arg0, 0x8129E524)
        self.assertEqual(journal.records[-1].arg1, 0xFFFFFFFF)

    def test_parser_does_not_label_unrelated_phases_as_grain_checks(self) -> None:
        records = (phase_record(2),
                   phase_record(4, action=2, phase=0x5F),
                   phase_record(6, action=1, phase=0x07),
                   phase_record(8, action=3, phase=0x6B))
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        self.assertNotIn("grain=", output.getvalue())

    def test_parser_decodes_composite_guard_and_legacy_epoch_records(self) -> None:
        guarded_phase = (
            lm_dump.EPOCH_PHASE_FLAG
            | (8 << lm_dump.EPOCH_GUARD_SHIFT)
            | 0x180
        )
        records = (
            phase_record(2),
            phase_record(4, action=2, phase=0x80000100),
            phase_record(6, action=2, phase=guarded_phase),
        )
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        text = output.getvalue()
        self.assertIn("epoch_guard=X00 epoch_mask=00000100", text)
        self.assertIn("epoch_guard=X08 epoch_mask=00000180", text)

    def test_parser_decodes_durable_rejection_telemetry(self) -> None:
        summary_arg0 = (6 << 24) | (3 << 16) | (24 << 8) | 33
        summary_arg1 = (9 << 24) | (18 << 16) | 27
        records = (
            phase_record(2),
            phase_record(
                4,
                action=2,
                phase=lm_dump.REJECT_SUMMARY_PHASE,
                arg0=summary_arg0,
                arg1=summary_arg1,
            ),
            phase_record(
                6,
                action=2,
                phase=lm_dump.REJECT_SAVED_IDENTITY_PHASE,
                arg0=0x180,
                arg1=0x19,
            ),
            phase_record(
                8,
                action=2,
                phase=lm_dump.REJECT_LIVE_IDENTITY_PHASE,
                arg0=0x181,
                arg1=0x26,
            ),
        )
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        text = output.getvalue()
        self.assertIn(
            "reject_summary=status=epoch guard=X03 volumes=24>33 "
            "changes=-9+18 models=27",
            text,
        )
        self.assertIn(
            "reject_identity=saved map=00000180 scene=00000019", text
        )
        self.assertIn(
            "reject_identity=live map=00000181 scene=00000026", text
        )

    def test_parser_marks_unavailable_rejection_counts(self) -> None:
        summary_arg0 = (6 << 24) | (2 << 16) | (0xFF << 8) | 0xFF
        summary_arg1 = (0xFF << 24) | (0xFF << 16) | 0xFFFF
        records = (
            phase_record(2),
            phase_record(
                4,
                action=2,
                phase=lm_dump.REJECT_SUMMARY_PHASE,
                arg0=summary_arg0,
                arg1=summary_arg1,
            ),
        )
        journal = lm_dump.parse_journal_bytes(journal_bytes(records=records))
        output = io.StringIO()
        with redirect_stdout(output):
            lm_dump.print_journal(journal, latest=True)
        self.assertIn(
            "guard=X02 volumes=?>? changes=-?+? models=?",
            output.getvalue(),
        )

    def test_guard_field_does_not_overlap_epoch_bits_or_flags(self) -> None:
        self.assertEqual(
            lm_dump.EPOCH_GUARD_MASK & lm_dump.EPOCH_MASK,
            0,
        )
        self.assertEqual(
            lm_dump.EPOCH_GUARD_MASK & lm_dump.EPOCH_PHASE_FLAG,
            0,
        )
        for guard in (*range(9), 0xA0):
            phase = (
                lm_dump.EPOCH_PHASE_FLAG
                | (guard << lm_dump.EPOCH_GUARD_SHIFT)
                | 0x180
            )
            decoded = (
                phase & lm_dump.EPOCH_GUARD_MASK
            ) >> lm_dump.EPOCH_GUARD_SHIFT
            self.assertEqual(decoded, guard)


class NativeJournalRotationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-dump-journal-")
        directory = Path(cls.temp.name)
        header = re.search(r"^struct SusamuneLmDumpHeader \{.*?^\};", SOURCE, re.M | re.S)
        assert header
        definitions = [header.group(0)]
        for name in ("GenerationNewer", "LmDumpTarget", "ValidLmDumpHeader",
                     "ValidPhaseTrace", "ValidLmDumpAnchor", "ReadLmDumpHeader", "CloseLmDump",
                     "DisableLmDump", "WriteLmDump", "BeginLmDump",
                     "RecordLmDump", "InitLmDump"):
            function = re.search(rf"^static \w+ {name}\([^;]*?\n\{{.*?^\}}",
                                 SOURCE, re.M | re.S)
            assert function, name
            definitions.append(function.group(0))
        (directory / "lm_dump_journal_production.inc").write_text(
            "\n\n".join(definitions), encoding="utf-8")
        output = directory / ("journal.dll" if os.name == "nt" else "journal.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"), "-I", str(directory),
                   str(ROOT / "scripts/lm_dump_journal_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.journal_seed.argtypes = [ctypes.c_uint] * 3
        cls.lib.journal_generation.argtypes = [ctypes.c_uint]
        cls.lib.journal_generation.restype = ctypes.c_uint
        cls.lib.journal_recovered.restype = ctypes.c_uint
        cls.lib.journal_phase.argtypes = [ctypes.c_uint] * 2
        cls.lib.journal_seed_anchor.argtypes = [ctypes.c_uint] * 3

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.lib.journal_reset()

    def generations(self):
        return [self.lib.journal_generation(i) for i in range(8)]

    def test_upgrade_keeps_legacy_pair_while_new_slots_fill(self):
        self.lib.journal_seed(0, 46, 1)
        self.lib.journal_seed(1, 47, 1)
        self.lib.journal_seed_anchor(0, 1, 1)
        self.lib.journal_seed_anchor(1, 1, 1)
        self.lib.journal_restart()
        self.assertEqual(self.lib.journal_recovered(), 47)
        self.assertEqual(self.lib.journal_writes(), 0)
        for generation in range(48, 54):
            self.lib.journal_save()
            self.assertEqual(self.generations()[:2], [46, 47])
        self.assertEqual(self.generations(), list(range(46, 54)))
        self.lib.journal_save()
        self.assertEqual(self.generations(), [54, *range(47, 54)])

    def test_rotation_retains_the_eight_latest_attempts_after_every_restart(self):
        for generation in range(1, 25):
            self.lib.journal_save()
            self.lib.journal_restart()
            self.assertEqual(self.lib.journal_recovered(), generation)
            self.assertEqual(sorted(filter(None, self.generations())),
                             list(range(max(1, generation - 7), generation + 1)))

    def test_recovery_finds_the_latest_in_h_not_only_a_b(self):
        for i, generation in enumerate((10, 14, 11, 13, 9, 12, 8, 15)):
            self.lib.journal_seed(i, generation, 1)
        self.lib.journal_restart()
        self.lib.journal_save()
        self.assertEqual(self.generations(), [10, 14, 11, 13, 9, 12, 16, 15])

    def test_wrapping_generation_recovery_and_oldest_selection(self):
        for i in range(8):
            self.lib.journal_seed(i, 0xFFFFFFF8 + i, 1)
        self.lib.journal_restart()
        self.assertEqual(self.lib.journal_recovered(), 0xFFFFFFFF)
        self.lib.journal_save()
        self.lib.journal_restart()
        self.assertEqual(self.lib.journal_recovered(), 1)
        self.lib.journal_save()
        self.assertEqual(self.generations()[:2], [1, 2])

    def test_torn_header_cannot_supply_recovered_generation(self):
        self.lib.journal_seed(0, 99, 0)
        self.lib.journal_seed(7, 8, 1)
        self.lib.journal_restart()
        self.assertEqual(self.lib.journal_recovered(), 8)
        self.lib.journal_save()
        self.assertEqual(self.generations(), [9, 0, 0, 0, 0, 0, 0, 8])

    def test_failed_rotation_write_preserves_other_seven_and_stops(self):
        for nth in (1, 2):
            with self.subTest(write=nth):
                self.lib.journal_reset()
                for i in range(8):
                    self.lib.journal_seed(i, i + 1, 1)
                self.lib.journal_restart()
                self.lib.journal_fail_write(nth)
                self.lib.journal_save()
                self.assertEqual(self.generations(), [0, *range(2, 9)])
                self.lib.journal_save()
                self.assertEqual(self.lib.journal_writes(), nth)

    def test_first_load_after_boot_anchors_refusal_and_does_not_rotate_each_load(self):
        self.lib.journal_phase(2, 1)
        self.lib.journal_phase(2, 0xD3)
        self.assertEqual(self.generations(), [1, 0, 0, 0, 0, 0, 0, 0])
        self.assertEqual(self.lib.journal_first_action(0), 2)
        self.assertEqual(self.lib.journal_records(0), 2)
        self.lib.journal_phase(2, 1)
        self.assertEqual(self.lib.journal_recovered(), 1)
        self.assertEqual(self.lib.journal_records(0), 3)
        self.lib.journal_save()
        self.assertEqual(self.lib.journal_first_action(1), 1)
        self.lib.journal_restart()
        self.lib.journal_phase(2, 1)
        self.lib.journal_phase(2, 0xFA)
        self.assertEqual(self.generations(), [1, 2, 3, 0, 0, 0, 0, 0])
        self.assertEqual(self.lib.journal_first_action(2), 2)
        self.assertEqual(self.lib.journal_records(2), 2)

    def test_legacy_load_anchor_is_not_recovered_as_valid_history(self):
        self.lib.journal_seed(0, 99, 1)
        self.lib.journal_seed_anchor(0, 1, 2)
        self.lib.journal_seed(7, 8, 1)
        self.lib.journal_restart()
        self.assertEqual(self.lib.journal_recovered(), 8)
        self.lib.journal_phase(2, 1)
        self.assertEqual(self.generations(), [9, 0, 0, 0, 0, 0, 0, 8])


if __name__ == "__main__":
    unittest.main()
