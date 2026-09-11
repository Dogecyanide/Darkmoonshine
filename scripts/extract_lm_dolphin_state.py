"""Extract GLMJ01 MEM1/fake VMEM from a local Dolphin 5.0 Win64 native state.

Requires optional `lzokay`. Layout follows Dolphin 5.0 Core/State.cpp and
HW/Memmap.cpp; input and extracted retail memory are local diagnostics only.
"""

import argparse
import json
from pathlib import Path
import struct


def decode(path):
    import lzokay
    data = Path(path).read_bytes()
    if data[:6] != b"GLMJ01" or len(data) < 24:
        raise ValueError("expected a Japanese LM Dolphin state")
    expected = struct.unpack_from("<I", data, 8)[0]
    if expected > 256 * 1024 * 1024:
        raise ValueError("unreasonable native state size")
    if expected == 0:
        return data[24:]
    offset = 24
    output = bytearray()
    while offset < len(data):
        if offset + 4 > len(data):
            raise ValueError("truncated native block length")
        size = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        if not size or size > 140000 or offset + size > len(data):
            raise ValueError("invalid native LZO block")
        output += lzokay.decompress(data[offset:offset + size], min(128 * 1024, expected - len(output)))
        offset += size
        if len(output) > expected:
            raise ValueError("native state exceeds declared size")
    if len(output) != expected:
        raise ValueError("native state decompressed size mismatch")
    return output


def extract(source, directory):
    directory = Path(directory)
    if directory.exists() and any(directory.iterdir()):
        raise ValueError("diagnostic output directory must be new or empty")
    data = decode(source)
    candidates = []
    offset = 0
    while True:
        offset = data.find(b"GLMJ01\0\0", offset)
        if offset < 0:
            break
        fake = offset + 0x2000000 + 0x40000 + 4
        if (fake + 0x2000000 + 4 <= len(data)
                and data[fake - 4:fake] == b"B\0\0\0"
                and data[fake:fake + 4] == b"LMST"
                and data[fake + 0x2000000:fake + 0x2000000 + 4] == b"B\0\0\0"):
            candidates.append((offset, fake))
        offset += 8
    if len(candidates) != 1:
        raise ValueError(f"expected exactly one validated MEM1/VMEM layout, found {candidates}")
    ram, fake = candidates[0]
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "mem1.bin").write_bytes(data[ram:ram + 0x1800000])
    (directory / "fake-vmem.bin").write_bytes(data[fake:fake + 0x2000000])
    info = {"source": str(Path(source).resolve()), "payload_bytes": len(data),
            "mem1_payload_offset": ram, "fake_vmem_payload_offset": fake,
            "mem1_base": "80000000", "fake_vmem_base": "70000000"}
    (directory / "extraction.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    print(json.dumps(extract(args.state, args.output), indent=2))
