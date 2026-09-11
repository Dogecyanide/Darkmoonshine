"""Build a verified GLMJ01 Dolphin development ISO and source-safe BPS.

Only a clean revision-0 main.dol is accepted. Retail assets are copied from
the supplied ISO; BPS literals contain only injected code and changed words.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import zlib

import gen_iso_bps as bps


LM_DOL_SHA1 = "722005ea9c1eab54b114f814734d8f327e5614ee"
CHUNK_SIZE = 8 * 1024 * 1024


def check_distinct_paths(source, *outputs):
    paths = [Path(path).resolve() for path in (source, *outputs)]
    if len(set(paths)) != len(paths):
        raise ValueError("source ISO and every output must have different paths")


def verify_source(iso_path, manifest):
    from pyisotools.iso import GamecubeISO

    if Path(iso_path).stat().st_size != bps.GC_ISO_SIZE:
        raise ValueError("expected a full-size clean GameCube ISO")
    with Path(iso_path).open("rb") as source:
        if source.read(8) != b"GLMJ01\x00\x00":
            raise ValueError("source must be Japanese Luigi's Mansion GLMJ01 revision 0")
        disc = GamecubeISO.from_iso(Path(iso_path))
        source.seek(disc.bootheader.dolOffset)
        dol = source.read(disc.dol.size)
    if hashlib.sha1(dol).hexdigest() != LM_DOL_SHA1:
        raise ValueError("main.dol is not the verified clean GLMJ01 executable")
    if manifest.get("game_id") != int.from_bytes(b"GLMJ", "big"):
        raise ValueError("payload is not for GLMJ01")
    if manifest.get("runtime") != "dolphin":
        raise ValueError("payload must be compiled with LM_EMULATOR=ON")
    if any(len(write) != 3 for write in manifest["writes"]):
        raise ValueError("LM requires authenticated address/expected/replacement writes")
    for address, expected in (
        [(write[0], write[1]) for write in manifest["writes"]]
        + manifest.get("checks", [])
    ):
        section = next((section for section in disc.dol.sections
                        if section.address <= address
                        and address + 4 <= section.address + section.size), None)
        if section is None:
            raise ValueError(f"unmapped authenticated address {address:#x}")
        offset = section.offset + address - section.address
        actual = struct.unpack_from(">I", dol, offset)[0]
        if actual != expected:
            raise ValueError(f"retail check failed at {address:#x}: {actual:08X}")


def decode_number(stream):
    result, shift = 0, 1
    for _ in range(10):
        raw = stream.read(1)
        if not raw:
            raise ValueError("truncated BPS integer")
        value = raw[0]
        result += (value & 127) * shift
        if value & 128:
            return result
        shift <<= 7
        result += shift
    raise ValueError("oversized BPS integer")


def apply_patch(source_path, patch, target_path):
    """Apply and verify all BPS checksums without holding a disc in memory."""
    check_distinct_paths(source_path, target_path)
    if len(patch) < 19 or patch[:4] != bps.BPS_MAGIC:
        raise ValueError("invalid BPS header")
    source_crc, target_crc, patch_crc = struct.unpack("<III", patch[-12:])
    if zlib.crc32(patch[:-4]) & 0xFFFFFFFF != patch_crc:
        raise ValueError("BPS patch checksum mismatch")
    stream = io.BytesIO(patch[:-12])
    stream.seek(4)
    source_size = decode_number(stream)
    target_size = decode_number(stream)
    metadata_size = decode_number(stream)
    if len(stream.read(metadata_size)) != metadata_size:
        raise ValueError("truncated BPS metadata")
    with Path(source_path).open("rb") as source:
        if Path(source_path).stat().st_size != source_size:
            raise ValueError("BPS source size mismatch")
        if bps.read_crc(source, 0, source_size) != source_crc:
            raise ValueError("BPS source checksum mismatch")
        with Path(target_path).open("w+b") as target:
            output_offset = source_relative = target_relative = 0
            while output_offset < target_size:
                command = decode_number(stream)
                action, remaining = command & 3, (command >> 2) + 1
                if remaining > target_size - output_offset:
                    raise ValueError("BPS action exceeds target")
                if action in (2, 3):
                    delta = decode_number(stream)
                    delta = -(delta >> 1) if delta & 1 else delta >> 1
                    if action == 2:
                        source_relative += delta
                    else:
                        target_relative += delta
                while remaining:
                    count = min(remaining, CHUNK_SIZE)
                    if action == 0:
                        position = output_offset
                    elif action == 2:
                        position = source_relative
                    elif action == 3:
                        if not 0 <= target_relative < output_offset:
                            raise ValueError("invalid BPS target-copy source")
                        target.seek(target_relative)
                        data = target.read(min(count, output_offset - target_relative))
                        if len(data) < count:
                            data = (data * ((count + len(data) - 1) // len(data)))[:count]
                        target_relative += count
                    else:
                        data = stream.read(count)
                    if action in (0, 2):
                        if position < 0 or position + count > source_size:
                            raise ValueError("invalid BPS source-copy range")
                        source.seek(position)
                        data = source.read(count)
                        if action == 2:
                            source_relative += count
                    if len(data) != count:
                        raise ValueError("short BPS copy")
                    target.seek(output_offset)
                    target.write(data)
                    output_offset += count
                    remaining -= count
            if stream.tell() != len(patch) - 12:
                raise ValueError("trailing BPS actions")
            target.flush()
            if bps.read_crc(target, 0, target_size) != target_crc:
                raise ValueError("BPS target checksum mismatch")
    return target_crc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True)
    parser.add_argument("--mod-manifest", required=True)
    parser.add_argument("--output-iso", required=True)
    parser.add_argument("--output-bps", required=True)
    args = parser.parse_args()
    check_distinct_paths(args.iso, args.output_iso, args.output_bps)
    manifest = bps.load_json(args.mod_manifest)
    verify_source(args.iso, manifest)
    layout = bps.create_layout(args.iso, manifest, "lmj")
    patch, builder = bps.build_patch(layout, manifest)
    for output in (args.output_iso, args.output_bps):
        Path(output).parent.mkdir(parents=True, exist_ok=True)
    target_crc = apply_patch(args.iso, patch, args.output_iso)
    Path(args.output_bps).write_bytes(patch)
    Path(args.output_bps).with_suffix(".layout.json").write_text(
        json.dumps(layout, indent=2) + "\n", encoding="utf-8")
    literal_bytes = sum(size for action, size, _ in builder.actions if action == 1)
    print(f"Verified clean GLMJ01 DOL {LM_DOL_SHA1}")
    print(f"BPS: {len(patch):,} bytes ({literal_bytes:,} injected literal bytes)")
    print(f"ISO: {args.output_iso}; verified target CRC32 {target_crc:08X}")


if __name__ == "__main__":
    main()
