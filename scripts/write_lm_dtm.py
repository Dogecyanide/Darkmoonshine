"""Generate a Dolphin 5.0 input movie from explicit controller-poll steps.

DTM wire layout: dolphin-emu/dolphin tag 5.0, Source/Core/Core/Movie.h.
Poll counts are deliberate: LM switches between 60 and 30 gameplay updates.
Playback uses RAW card A, not Dolphin's GCI folder. Supply --raw-card with an
existing Japanese card containing GLMJ01, or explicitly --allow-empty-card.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


BUTTONS = {name: 1 << bit for bit, name in enumerate(
    ("Start", "A", "B", "X", "Y", "Z", "Up", "Down", "Left", "Right", "L", "R"))}
CARD_BLOCK_SIZE = 0x2000
DTM_SAVE_CONFIG = 137
DTM_MEMCARDS = 151
DTM_CLEAR_SAVE = 152


def card_checksums(data):
    words = struct.unpack(f">{len(data) // 2}H", data)
    return tuple(value if value != 0xffff else 0 for value in (
        sum(words) & 0xffff, sum(word ^ 0xffff for word in words) & 0xffff))


def validate_raw_card(path):
    """Read-only structural preflight, not LM's save checksum/completion check."""
    path = Path(path)
    if not path.is_file():
        raise ValueError("--raw-card must name an existing RAW card, not a GCI folder")
    size = path.stat().st_size
    if size not in {mbit * 131072 for mbit in (4, 8, 16, 32, 64, 128)}:
        raise ValueError("RAW card size is invalid; a .gci file is not a RAW card")
    data = path.read_bytes()
    if len(data) != size:
        raise ValueError("RAW card changed during preflight; stop emulation first")
    if struct.unpack_from(">H", data, 0x22)[0] * 131072 != size:
        raise ValueError("RAW card header size does not match its file size")
    if struct.unpack_from(">H", data, 0x24)[0] != 1:
        raise ValueError("RAW card must use Japanese encoding for GLMJ01")
    regions = ((0, 0x1fc, 0x1fc), (0x2000, 0x3ffc, 0x3ffc),
               (0x4000, 0x5ffc, 0x5ffc), (0x6004, 0x8000, 0x6000),
               (0x8004, 0xa000, 0x8000))
    for start, end, checksum in regions:
        if card_checksums(data[start:end]) != struct.unpack_from(">HH", data, checksum):
            raise ValueError("RAW card metadata checksum failed; preflight will not repair it")
    # Dolphin 5.0 chooses the backup on equal directory/BAT update counters.
    directory = (0x2000 if struct.unpack_from(">H", data, 0x3ffa)[0] >
                 struct.unpack_from(">H", data, 0x5ffa)[0] else 0x4000)
    bat = (0x6000 if struct.unpack_from(">H", data, 0x6004)[0] >
           struct.unpack_from(">H", data, 0x8004)[0] else 0x8000)
    blocks = size // CARD_BLOCK_SIZE
    for offset in range(directory, directory + 127 * 64, 64):
        if data[offset:offset + 6] != b"GLMJ01":
            continue
        block, count = struct.unpack_from(">HH", data, offset + 0x36)
        if not 1 <= count <= blocks - 5:
            raise ValueError("GLMJ01 RAW directory entry has an invalid block count")
        seen = set()
        has_data = False
        for _ in range(count):
            if not 5 <= block < blocks or block in seen:
                raise ValueError("GLMJ01 RAW save block chain is invalid")
            seen.add(block)
            payload = data[block * CARD_BLOCK_SIZE:(block + 1) * CARD_BLOCK_SIZE]
            has_data |= any(value != 0xff for value in payload)
            block = struct.unpack_from(">H", data, bat + 0xa + (block - 5) * 2)[0]
        if block != 0xffff:
            raise ValueError("GLMJ01 RAW save block chain does not match its length")
        if not has_data:
            raise ValueError("GLMJ01 RAW save data is erased")
        return path.resolve()
    raise ValueError("RAW card contains no active GLMJ01 save; Hidden Mansion would be unavailable")


def build_movie(iso, steps, from_state=False, *, raw_card=None, allow_empty_card=False):
    if raw_card is not None and allow_empty_card:
        raise ValueError("choose --raw-card or --allow-empty-card, not both")
    if raw_card is not None:
        validate_raw_card(raw_card)
    elif not allow_empty_card:
        raise ValueError("Dolphin 5.0 DTM playback ignores GCI folders: provide --raw-card "
                         "or explicitly --allow-empty-card")
    digest = hashlib.md5()
    with Path(iso).open("rb") as source:
        while block := source.read(8 * 1024 * 1024):
            digest.update(block)
    samples = bytearray()
    for step in steps:
        count = int(step["polls"])
        if not 1 <= count <= 36000:
            raise ValueError("each step needs 1..36000 controller polls")
        buttons = 0
        for name in step.get("buttons", []):
            buttons |= BUTTONS[name]
        stick = step.get("stick", [128, 128])
        cstick = step.get("cstick", [128, 128])
        triggers = step.get("triggers", [0, 0])
        samples += struct.pack("<H6B", buttons, *triggers, *stick, *cstick) * count
    polls = len(samples) // 8
    header = bytearray(256)
    header[:4] = b"DTM\x1a"
    header[4:10] = b"GLMJ01"
    header[11] = 1
    header[12] = from_state
    # A native savestate also restores Dolphin's absolute movie counters.
    # Let consumed controller records end playback, regardless of that age.
    struct.pack_into("<QQQ", header, 13, (1 << 63) - 1, polls, 0)
    header[49:49 + len(b"Moonshine LM development")] = b"Moonshine LM development"
    header[113:129] = digest.digest()
    struct.pack_into("<Q", header, 129, 1788652800)
    header[DTM_SAVE_CONFIG] = 1
    header[138] = 1  # Skip idle.
    header[141] = 1  # HLE DSP.
    header[143] = 1  # JIT CPU.
    header[144] = 1  # EFB access.
    header[145] = 1  # EFB copy.
    header[149] = 1  # XFB.
    header[150] = 1  # Real XFB for the CPU-drawn overlay.
    # EXI::Init maps this bit to RAW; DTM cannot select the GCI-folder backend.
    header[DTM_MEMCARDS] = 1
    header[DTM_CLEAR_SAVE] = 0
    header[156] = 1  # PAL60, matching the isolated profile.
    # End on consumed inputs. A zero tick count ends playback on its first poll.
    struct.pack_into("<Q", header, 237, (1 << 63) - 1)
    return bytes(header + samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True)
    parser.add_argument("--steps", required=True, help="JSON list of poll/button/stick steps")
    parser.add_argument("--output", required=True)
    parser.add_argument("--from-state", action="store_true")
    cards = parser.add_mutually_exclusive_group(required=True)
    cards.add_argument("--raw-card", help="existing Japanese RAW card A containing GLMJ01; "
                       "configure this same path in Dolphin (DTM does not store card paths)")
    cards.add_argument("--allow-empty-card", action="store_true",
                       help="knowingly bypass the save-card check; does not clear or create a card")
    args = parser.parse_args()
    output = Path(args.output)
    inputs = [Path(args.iso).resolve(), Path(args.steps).resolve()]
    if args.raw_card:
        inputs.append(Path(args.raw_card).resolve())
    if output.resolve() in inputs:
        raise ValueError("movie output must differ from input files")
    steps = json.loads(Path(args.steps).read_text(encoding="utf-8"))
    result = build_movie(args.iso, steps, args.from_state, raw_card=args.raw_card,
                         allow_empty_card=args.allow_empty_card)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    print(f"Wrote {output}: {(len(result) - 256) // 8} controller polls")
    if args.raw_card:
        print(f"Before playback, configure Dolphin RAW card A to: {Path(args.raw_card).resolve()}")
        print("Card preflight does not verify LM save checksums or Hidden Mansion completion.")
    else:
        print("WARNING: RAW card A was not verified; GCI-folder saves are ignored during playback.")


if __name__ == "__main__":
    main()
