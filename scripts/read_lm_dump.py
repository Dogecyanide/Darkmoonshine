#!/usr/bin/env python3
"""Read GLMJ phase-journal files without modifying them."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import sys


HEADER = struct.Struct(">IHHIIIIII")
PHASE = struct.Struct(">IIIIIIII")
HEADER_MAGIC = 0x4C4D4450  # LMDP
HEADER_VERSION = 1
PHASE_MAGIC = 0x53504853  # SPHS
GAME_ID_GLMJ = 0x474C4D4A
SAVE_COMPLETE_PHASE = 0x7F
EPOCH_PHASE_FLAG = 0x80000000
EPOCH_GUARD_SHIFT = 22
EPOCH_GUARD_MASK = 0x3FC00000
EPOCH_MASK = 0x003FFFFF
U32_MASK = 0xFFFFFFFF
ACTION_NAMES = {1: "save", 2: "load", 3: "post-load"}
ATTEMPT_NAMES = ("lm_attempt_a.bin", "lm_attempt_b.bin")


class DumpError(ValueError):
    pass


@dataclass(frozen=True)
class PhaseRecord:
    magic: int
    sequence_begin: int
    action: int
    phase: int
    phase_inverse: int
    arg0: int
    arg1: int
    sequence_end: int

    @property
    def valid(self) -> bool:
        return (
            self.magic == PHASE_MAGIC
            and self.sequence_begin != 0
            and self.sequence_begin & 1 == 0
            and self.sequence_begin == self.sequence_end
            and self.phase ^ self.phase_inverse == U32_MASK
            and self.action in ACTION_NAMES
        )


@dataclass(frozen=True)
class Journal:
    source: str
    generation: int
    game_id: int
    mod_crc32: int
    save_sequence: int
    records: tuple[PhaseRecord, ...]
    trailing_bytes: int


def generation_is_newer(candidate: int, current: int) -> bool:
    delta = (candidate - current) & U32_MASK
    return 0 < delta < 0x80000000


def parse_journal_bytes(data: bytes, source: str = "<memory>") -> Journal:
    if len(data) < HEADER.size + PHASE.size:
        raise DumpError(
            f"{source}: file is shorter than its header and first record"
        )

    (
        magic,
        version,
        header_size,
        generation,
        generation_inverse,
        game_id,
        mod_crc32,
        record_size,
        save_sequence,
    ) = HEADER.unpack_from(data)
    if magic != HEADER_MAGIC:
        raise DumpError(f"{source}: bad header magic {magic:08X}")
    if version != HEADER_VERSION or header_size != HEADER.size:
        raise DumpError(
            f"{source}: unsupported header version/size {version}/{header_size}"
        )
    if generation == 0 or generation ^ generation_inverse != U32_MASK:
        raise DumpError(f"{source}: torn generation header")
    if game_id != GAME_ID_GLMJ:
        raise DumpError(f"{source}: unexpected game ID {game_id:08X}")
    if record_size != PHASE.size:
        raise DumpError(f"{source}: unsupported record size {record_size}")
    if save_sequence == 0 or save_sequence & 1:
        raise DumpError(f"{source}: invalid save sequence {save_sequence}")

    payload_size = len(data) - header_size
    record_count, trailing_bytes = divmod(payload_size, record_size)
    records = tuple(
        PhaseRecord(*PHASE.unpack_from(data, header_size + index * record_size))
        for index in range(record_count)
    )
    first = records[0]
    if not first.valid or first.action != 1 or first.phase != SAVE_COMPLETE_PHASE:
        raise DumpError(f"{source}: first record is not a completed save")
    if first.sequence_begin != save_sequence:
        raise DumpError(f"{source}: first record does not match the header")

    return Journal(
        source=source,
        generation=generation,
        game_id=game_id,
        mod_crc32=mod_crc32,
        save_sequence=save_sequence,
        records=records,
        trailing_bytes=trailing_bytes,
    )


def read_journal(path: Path) -> Journal:
    return parse_journal_bytes(path.read_bytes(), str(path))


def input_paths(values: list[str]) -> list[Path]:
    paths: list[Path] = []
    for value in values:
        path = Path(value)
        if path.is_dir():
            paths.extend(
                path / name for name in ATTEMPT_NAMES if (path / name).is_file()
            )
        else:
            paths.append(path)
    return paths


def print_journal(journal: Journal, latest: bool) -> None:
    suffix = " [latest]" if latest else ""
    print(f"{journal.source}{suffix}")
    print(
        f"  generation={journal.generation} game_id={journal.game_id:08X} "
        f"mod_crc32={journal.mod_crc32:08X} save_sequence={journal.save_sequence}"
    )
    invalid = sum(not record.valid for record in journal.records)
    print(
        f"  records={len(journal.records)} invalid={invalid} "
        f"trailing_bytes={journal.trailing_bytes}"
    )
    for index, record in enumerate(journal.records):
        validity = "ok" if record.valid else "INVALID"
        action = ACTION_NAMES.get(record.action, f"unknown-{record.action}")
        epoch = ""
        if record.action == 2 and record.phase & EPOCH_PHASE_FLAG:
            guard = (record.phase & EPOCH_GUARD_MASK) >> EPOCH_GUARD_SHIFT
            epoch = (
                f" epoch_guard=X{guard:02X}"
                f" epoch_mask={record.phase & EPOCH_MASK:08X}"
            )
        print(
            f"  {index:04d} seq={record.sequence_begin:10d} "
            f"action={action:<9} phase={record.phase:08X} "
            f"arg0={record.arg0:08X} arg1={record.arg1:08X} {validity}{epoch}"
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode the two rotating GLMJ files in an lm_dumps directory."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="an lm_dumps directory or one or more lm_attempt_*.bin files",
    )
    args = parser.parse_args(argv)

    paths = input_paths(args.paths)
    if not paths:
        parser.error("no lm_attempt_a.bin or lm_attempt_b.bin files found")

    journals: list[Journal] = []
    failed = False
    for path in paths:
        try:
            journals.append(read_journal(path))
        except (OSError, DumpError) as error:
            print(error, file=sys.stderr)
            failed = True
    if not journals:
        return 1

    latest = journals[0]
    for journal in journals[1:]:
        if generation_is_newer(journal.generation, latest.generation):
            latest = journal
    for index, journal in enumerate(journals):
        if index:
            print()
        print_journal(journal, journal is latest)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
