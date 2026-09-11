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
LEGACY_HEADER_VERSION = 1
HEADER_VERSION = 2
PHASE_MAGIC = 0x53504853  # SPHS
GAME_ID_GLMJ = 0x474C4D4A
SAVE_COMPLETE_PHASE = 0x7F
LOAD_START_PHASE = 0x01
EPOCH_PHASE_FLAG = 0x80000000
EPOCH_GUARD_SHIFT = 22
EPOCH_GUARD_MASK = 0x3FC00000
EPOCH_MASK = 0x003FFFFF
U32_MASK = 0xFFFFFFFF
ACTION_NAMES = {1: "save", 2: "load", 3: "post-load", 4: "warp"}
WARP_PHASE_NAMES = {
    0x01: "request", 0x02: "accepted", 0x10: "dispatch",
    0x20: "appearance", 0x21: "prepared", 0x30: "settling",
    0x7F: "arrived", 0xD0: "rejected",
}
STATUS_NAMES = {
    0: "empty",
    1: "saved",
    2: "loaded",
    3: "busy",
    4: "badcrc",
    5: "badheap",
    6: "epoch",
    7: "toobig",
}
REJECT_SUMMARY_PHASE = 0xD0
REJECT_SAVED_IDENTITY_PHASE = 0xD1
REJECT_LIVE_IDENTITY_PHASE = 0xD2
ATTEMPT_NAMES = tuple(f"lm_attempt_{letter}.bin" for letter in "abcdefgh")


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
    version: int
    generation: int
    game_id: int
    mod_crc32: int
    save_sequence: int
    records: tuple[PhaseRecord, ...]
    trailing_bytes: int


def generation_is_newer(candidate: int, current: int) -> bool:
    delta = (candidate - current) & U32_MASK
    return 0 < delta < 0x80000000


def telemetry_count(value: int, sentinel: int) -> str:
    return "?" if value == sentinel else str(value)


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
    if version not in (LEGACY_HEADER_VERSION, HEADER_VERSION) or header_size != HEADER.size:
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
        raise DumpError(f"{source}: invalid anchor sequence {save_sequence}")

    payload_size = len(data) - header_size
    record_count, trailing_bytes = divmod(payload_size, record_size)
    records = tuple(
        PhaseRecord(*PHASE.unpack_from(data, header_size + index * record_size))
        for index in range(record_count)
    )
    first = records[0]
    save_anchor = first.action == 1 and first.phase == SAVE_COMPLETE_PHASE
    load_anchor = version == HEADER_VERSION and first.action == 2 and first.phase == LOAD_START_PHASE
    if not first.valid or not (save_anchor or load_anchor):
        raise DumpError(f"{source}: first record is not a supported save/load anchor")
    if first.sequence_begin != save_sequence:
        raise DumpError(f"{source}: first record does not match the header")

    return Journal(
        source=source,
        version=version,
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
        f"mod_crc32={journal.mod_crc32:08X} version={journal.version} "
        f"anchor={ACTION_NAMES[journal.records[0].action]} sequence={journal.save_sequence}"
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
        if record.action == 4:
            epoch = f" warp_event={WARP_PHASE_NAMES.get(record.phase, 'unknown')}"
        elif record.action == 3 and record.phase in (0xF4, 0xF3):
            kind = "heap" if record.phase == 0xF4 else "render-target"
            epoch = f" {kind}_fault address={record.arg0:08X} value={record.arg1:08X}"
        elif record.action == 2 and 0xF4 <= record.phase <= 0xF6:
            epoch = (f" camera[{record.phase - 0xF4}]"
                     f" saved={record.arg0:08X} live={record.arg1:08X}")
        elif record.action == 2 and record.phase in (0xF7, 0xF8):
            side = "saved" if record.phase == 0xF7 else "live"
            epoch = (f" camera_{side}_fault address={record.arg0:08X}"
                     f" value={record.arg1:08X}")
        elif record.action in (1, 2) and record.phase == 0xF9:
            epoch = (f" reboot_profile_check={action}"
                     f" fault={record.arg0:08X} value={record.arg1:08X}")
        elif record.action == 2 and record.phase == 0xFA:
            epoch = (f" reboot_profile_mismatch word={record.arg0}"
                     f" live_value={record.arg1:08X}")
        elif ((record.action == 1 and record.phase == 0x5F) or
              (record.action == 2 and record.phase in (0x07, 0x6B))):
            stage = "save-source" if record.action == 1 else (
                "saved-source" if record.phase == 0x07 else "restored-copy")
            verdict = "ok" if record.arg0 == 0 and record.arg1 == 0 else "invalid"
            epoch = f" grain={stage}:{verdict}"
        elif record.action == 2 and record.phase & EPOCH_PHASE_FLAG:
            guard = (record.phase & EPOCH_GUARD_MASK) >> EPOCH_GUARD_SHIFT
            epoch = (
                f" epoch_guard=X{guard:02X}"
                f" epoch_mask={record.phase & EPOCH_MASK:08X}"
            )
        elif record.action == 2 and record.phase == REJECT_SUMMARY_PHASE:
            status = (record.arg0 >> 24) & 0xFF
            guard = (record.arg0 >> 16) & 0xFF
            saved_volumes = (record.arg0 >> 8) & 0xFF
            live_volumes = record.arg0 & 0xFF
            removed = (record.arg1 >> 24) & 0xFF
            added = (record.arg1 >> 16) & 0xFF
            models = record.arg1 & 0xFFFF
            epoch = (
                f" reject_summary=status={STATUS_NAMES.get(status, status)}"
                f" guard=X{guard:02X}"
                f" volumes={telemetry_count(saved_volumes, 0xFF)}>"
                f"{telemetry_count(live_volumes, 0xFF)}"
                f" changes=-{telemetry_count(removed, 0xFF)}"
                f"+{telemetry_count(added, 0xFF)}"
                f" models={telemetry_count(models, 0xFFFF)}"
            )
        elif record.action in (1, 2) and record.phase == 0xD3:
            status = (record.arg0 >> 24) & 0xFF
            epoch = (
                f" refusal={STATUS_NAMES.get(status, status)}"
                f" gate={(record.arg0 >> 16) & 0xFF}"
                f" stable_frames={record.arg0 & 0xFFFF} detail={record.arg1:08X}"
            )
        elif record.action in (1, 2) and record.phase == 0xD4:
            epoch = f" refusal_live_map={record.arg0} scene={record.arg1}"
        elif record.action in (1, 2) and record.phase == 0xD5:
            epoch = f" refusal_gate_value={record.arg0:08X} draw_state={record.arg1}"
        elif record.action in (1, 2) and record.phase == 0xD6:
            epoch = f" refusal_loop_mode={record.arg0} exit={record.arg1}"
        elif record.action == 2 and record.phase == 0xD7:
            epoch = (f" resource_masks=active:{record.arg0 >> 24:02X}"
                     f" records:{(record.arg0 >> 16) & 0xFF:02X}"
                     f" wanted={(record.arg0 >> 8) & 0xFF}>{record.arg0 & 0xFF}"
                     f" layout={record.arg1 >> 31} map={(record.arg1 >> 30) & 1}"
                     f" backing={(record.arg1 >> 16) & 0x7F:02X}>"
                     f"{(record.arg1 >> 8) & 0x7F:02X}")
        elif record.action == 2 and record.phase == 0xD8:
            fault = {1: "resource-range", 2: "resource-record", 0x10: "model-fields",
                     0x11: "saved-model-owner", 0x12: "live-model-owner",
                     0x13: "unmatched-volume", 0x14: "model-loading",
                     0x46: "saved-map-archive-owner", 0x47: "live-map-archive-owner"}
            epoch = (f" ownership_fault={fault.get(record.arg0, record.arg0)}"
                     f" value={record.arg1:08X}")
        elif record.action == 2 and record.phase == 0xD9:
            side = "saved" if record.arg0 & 0x80000000 else "live"
            epoch = (f" unmatched_volume={side} census_index={record.arg0 & 0x7FFFFFFF}"
                     f" object={record.arg1:08X}")
        elif record.action == 2 and record.phase == 0xDA:
            epoch = (f" unmatched_volume_name_hash={record.arg0:08X}"
                     f" backing={record.arg1:08X}")
        elif record.action == 2 and record.phase in (
            REJECT_SAVED_IDENTITY_PHASE,
            REJECT_LIVE_IDENTITY_PHASE,
        ):
            side = (
                "saved"
                if record.phase == REJECT_SAVED_IDENTITY_PHASE
                else "live"
            )
            epoch = (
                f" reject_identity={side}"
                f" map={record.arg0:08X} scene={record.arg1:08X}"
            )
        print(
            f"  {index:04d} seq={record.sequence_begin:10d} "
            f"action={action:<9} phase={record.phase:08X} "
            f"arg0={record.arg0:08X} arg1={record.arg1:08X} {validity}{epoch}"
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode up to eight rotating GLMJ files in an lm_dumps directory (legacy a/b supported)."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="an lm_dumps directory or one or more lm_attempt_*.bin files",
    )
    args = parser.parse_args(argv)

    paths = input_paths(args.paths)
    if not paths:
        parser.error("no lm_attempt_a.bin through lm_attempt_h.bin files found")

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
