#!/usr/bin/env python3
"""Host contracts for the GLMJ hard-lock phase journal."""

from pathlib import Path
from contextlib import redirect_stdout
import importlib.util
import io
import re
import sys
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
) -> bytes:
    first_sequence = lm_dump.PHASE.unpack(records[0])[1]
    header = lm_dump.HEADER.pack(
        lm_dump.HEADER_MAGIC,
        lm_dump.HEADER_VERSION,
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
    def test_lm_crash_reports_use_game_specific_names(self) -> None:
        self.assertIn('"%s/luigis_mansion_crash_a.bin"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_b.bin"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_a.txt"', SOURCE)
        self.assertIn('"%s/luigis_mansion_crash_b.txt"', SOURCE)
        self.assertIn(
            '"Moonshine Luigi\'s Mansion crash report v%u\\r\\n"', SOURCE
        )
        self.assertIn("if (GAME_ID == SUSAMUNE_MOD_GAME_ID_LMJ)", SOURCE)

    def test_journal_is_glmj_only_and_uses_two_rotating_files(self) -> None:
        self.assertIn('"%s/lm_dumps"', SOURCE)
        self.assertIn('"%s/lm_attempt_a.bin"', SOURCE)
        self.assertIn('"%s/lm_attempt_b.bin"', SOURCE)
        self.assertIn("GAME_ID != SUSAMUNE_MOD_GAME_ID_LMJ", SOURCE)
        self.assertRegex(SOURCE, r"target\s*=\s*generation\s*&\s*1u;")

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
            r"static void PollPhaseTrace\(void\)\s*\{(?P<body>.*?)\n\}",
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


if __name__ == "__main__":
    unittest.main()
