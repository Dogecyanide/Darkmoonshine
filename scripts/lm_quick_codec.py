"""Read-only independent decoder for LM's checksummed LML4 state payloads."""
import struct
import zlib

MAGIC = 0x4C4D4C34
BLOCK = 0x20000


def _block(data: bytes, expected: int) -> bytes:
    result = bytearray()
    pos = 0
    last_match = None

    def length(initial):
        nonlocal pos
        value = initial
        if initial == 15:
            while True:
                if pos == len(data):
                    raise ValueError('Truncated LZ4 length')
                extension = data[pos]
                pos += 1
                value += extension
                if value > expected:
                    raise ValueError('LZ4 length exceeds block')
                if extension != 255:
                    break
        return value

    while pos < len(data):
        token = data[pos]
        pos += 1
        literals = length(token >> 4)
        if literals > len(data) - pos or literals > expected - len(result):
            raise ValueError('Invalid LZ4 literals')
        result.extend(data[pos:pos + literals])
        pos += literals
        if pos == len(data):
            if len(result) != expected or (last_match is not None and
                    (literals < 5 or last_match > expected - 12)):
                raise ValueError('Invalid LZ4 final sequence')
            return bytes(result)
        if len(data) - pos < 2:
            raise ValueError('Truncated LZ4 offset')
        offset = data[pos] | data[pos + 1] << 8
        pos += 2
        if not offset or offset > len(result):
            raise ValueError('Invalid LZ4 offset')
        match = length(token & 15) + 4
        if match > expected - len(result):
            raise ValueError('LZ4 match exceeds block')
        last_match = len(result)
        # Repetition of the preceding period models overlapping LZ4 copies.
        period = bytes(result[-offset:])
        result.extend((period * ((match + offset - 1) // offset))[:match])
    raise ValueError('Missing LZ4 final literals')


def decode_fast(packed: bytes, expected: int) -> bytes:
    """Decode exact LML4 input/output or raise ValueError; never write files.

    Unlike Moonshine's MSL4 payload, LML4 includes its own final Adler32 word.
    """
    if not isinstance(expected, int) or not 0 <= expected <= 0xFFFFFFFF:
        raise ValueError('Invalid raw size')
    if len(packed) < 12 or struct.unpack_from('>II', packed) != (MAGIC, BLOCK):
        raise ValueError('Invalid LML4 header')
    result = bytearray()
    pos = 8
    while len(result) < expected:
        if len(packed) - pos < 12:
            raise ValueError('Truncated LML4 block')
        raw, flags = struct.unpack_from('>II', packed, pos)
        pos += 8
        size = flags & 0x7FFFFFFF
        plain = bool(flags & 0x80000000)
        if raw != min(BLOCK, expected - len(result)) or not size or \
                (size != raw if plain else size >= raw) or size > len(packed) - pos - 4:
            raise ValueError('Invalid LML4 block extent')
        data = packed[pos:pos + size]
        result.extend(data if plain else _block(data, raw))
        pos += size
    if len(packed) - pos != 4 or struct.unpack_from('>I', packed, pos)[0] != zlib.adler32(result):
        raise ValueError('LML4 trailing data or checksum mismatch')
    return bytes(result)
