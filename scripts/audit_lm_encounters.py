"""Read local, lawfully extracted LM JMap tables with retail-width field hashes.

This is an inspection tool, not a patch generator. The DOL supplies field names;
the output does not include game assets. Supply an optional room or event filter.
"""
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def field_hash(name: str) -> int:
    value = 0
    for char in name:
        value = ((ord(char) + (value << 8)) & 0xFFFFFFFF) % 0x1FFFFD9
    return value


def rows(path: Path, dol: Path = ROOT / "build-lm-diag/clean_glmj_main.dol"):
    names = {field_hash(s[:-1].decode()): s[:-1].decode()
             for s in re.findall(rb"[\x20-\x7e]{3,}\x00", dol.read_bytes())}
    names.update({field_hash(s): s for s in ("scale_y", "scale_z")})
    data = path.read_bytes()
    count, field_count, data_offset, entry_size = struct.unpack_from(">IIII", data)
    fields = [struct.unpack_from(">IIHBB", data, 16 + i * 12)
              for i in range(field_count)]
    result = []
    for row in range(count):
        entry = {}
        for hash_, mask, off, shift, type_ in fields:
            pos = data_offset + row * entry_size + off
            if type_ in (1, 6):
                value = data[pos:data_offset + (row + 1) * entry_size].split(
                    b"\0", 1)[0].decode(errors="replace")
            elif type_ == 2:
                value = struct.unpack_from(">f", data, pos)[0]
            else:
                value = struct.unpack_from({4: ">H", 5: ">B"}.get(type_, ">I"),
                                           data, pos)[0]
                value = (value & mask) >> shift
            entry[names.get(hash_, f"hash_{hash_:08X}")] = value
        result.append(entry)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--room", type=int)
    parser.add_argument("--event", type=int)
    parser.add_argument("--positions", action="store_true")
    args = parser.parse_args()
    for index, entry in enumerate(rows(args.path)):
        if args.room is not None and entry.get("room_no", entry.get("RoomNo")) != args.room:
            continue
        if args.event is not None and entry.get("EventNo") != args.event:
            continue
        print(index, {k: v for k, v in entry.items() if args.positions or
                      not k.startswith(("pos_", "dir_", "scale_"))})
