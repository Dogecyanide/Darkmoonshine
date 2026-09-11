# LZ4 1.10.0

Vendored from Moonshine V2.3.0 commit
`06cb7965032539a5ac88611b9c5aade7385daec9`, `vendor/lz4/`.
The LM wrapper uses distinct `LML4` magic and appends its own big-endian Adler32;
it is not the upstream `MSL4` container, whose checksum is stored externally.
The LM freestanding config supplies a private overlap-safe byte `memmove`;
volatile accesses prevent the compiler from replacing it with a libc call.

Source: https://github.com/lz4/lz4/tree/v1.10.0/lib

`lz4.c` and `lz4.h` retain upstream's BSD 2-Clause notices. The only source
adaptation removes the `stddef.h`, `stdint.h`, and `limits.h` includes; the
freestanding configuration supplies those types/constants before inclusion.
No compression or decompression algorithm is modified.

The mod uses caller-owned workspace, independent 128 KiB blocks, checked
decompression, portable memory access, and no allocation or file APIs.
`state_lz4_config.h` selects a 32 KiB hash table. Unused library entry points
are removed by the final section-garbage-collecting link.

## LM framing

All container words are big-endian. The eight-byte header is `LML4` followed
by block size `0x20000`. Each block has its exact decoded size and payload size
as two words. The payload-size high bit indicates an uncompressed block; its
remaining bits must equal the decoded size. Otherwise the payload must be
nonempty and smaller than that block's decoded size. Every block except the
last decodes to exactly 128 KiB. A final Adler32 word covers the complete raw
stream, including raw-fallback blocks. No trailing bytes are accepted.

The same caller-owned `0x50000` workspace serves fast compression, bounded
dry-validation and actual decoding, without heap allocation. Compression counts
the complete required size even if either of its two output spans runs out.
The original 128-probe zlib compressor remains available for dense fallback,
and decoding still accepts legacy zlib streams. A decoder commit must only run
after successful dry-validation of the same immutable input.
