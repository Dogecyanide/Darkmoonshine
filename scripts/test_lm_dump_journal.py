#!/usr/bin/env python3
"""Host contracts for the GLMJ hard-lock phase journal."""

from pathlib import Path
import importlib.util
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


if __name__ == "__main__":
    unittest.main()
