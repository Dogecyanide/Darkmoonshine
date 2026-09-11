# DarkMoonshine RC4 verification — 2026-09-11

Build: **V1.0.0 Frozen in Time. Release candidate 4**.

## Verified locally

- Wii and emulator payloads compile/link with the bundled toolchains. Full
  host suite: **1,048 tests, zero failures/errors/skips**. Five stale
  current-format assertions were updated from 27 to 28; capture ranges and
  other expectations were unchanged.
- Thirteen dedicated fast-codec and eight legacy-codec tests pass. Independent
  Python decoding checks the fast framing. Cases include bounded split output,
  exact required size, raw-block fallback, malformed sizes/offsets/trailers,
  checksums, alias rejection and exact consumption. The freestanding private
  memmove resolves the target's absent libc dependency.
- Twenty shared-companion tests cover real private resource data, exact fast
  capacity, dense fallback and its exact boundary, refusal preserving the old
  state, malformed framing despite recomputed wrapper CRC, validated live-only
  targets, and the unchanged full mailbox reservation.
- Forty-seven executable storage transaction tests pass, including actual fast
  rollback, forced dense fallback, exact-fit/one-byte-below admission, corrupt
  SD input restoring the old state, unknown codec rejection, bounded partial
  reads, authentication/profile checks and reboot cases.
- Twenty-five archive-comparison tests cover legacy readability, format-28
  fast/raw blocks and version/kind/checksum binding. The read-only archive
  reader validates structure/CRCs but does not claim native key authentication.
- Four CRC tests cover all input bytes with 100 running seeds, C/C++ parity,
  independent standard vectors, random/unaligned buffers and chunk continuation.
  The immutable table adds 1,024 bytes of read-only data; all passes remain.
- Six benchmark tests pass. Three private format-27 captures decode and
  round-trip byte-exactly through both codecs. No capture or key was exported
  into a release. See [measurements](lm-compression-benchmark.md) for complete
  methodology, source pin, sizes and limits.
- Independent review found no blocking issue in staged fast/dense capture,
  rollback codec tracking, format binding or full checksum/dry-decode preflight.
  Generated build commands place codec-only -O2 after the global -Oz.

## Performance and memory

The measured shared parents are 4,299,520 raw bytes. LML4 streams are
2,668,766–2,668,870 bytes versus 2,092,549–2,092,641 dense bytes. All fit the
actual companion capacity with at least 1,124,544 bytes of aligned headroom.
Host compression medians improve 23–31.5 times, validation-plus-restore decode
about 5.4–6 times. Equivalent table CRC is a separate improvement.
These are **not Wii or end-to-end speed measurements**. Raw core copying,
native synchronization and SD costs remain.

A complete densely packed inactive state measures 6,758,939–6,960,918 bytes,
exceeding the best-case exclusive inactive suffix of 5,763,040 bytes. Temporary
staging is already required for every save and SD-import rollback. Keep one
complete RAM state; two require a deliberate storage/transaction redesign.
Named SD archives remain available.

- Wii resident blob: **309,357 bytes**, 94.4% of the 327,680-byte build ceiling;
  **18,323 bytes free**. Emulator: **288,429 bytes**, 39,251 bytes free.
  Manifest-padded sizes: 309,360 / 288,432 bytes. Wii grew 13,508 bytes over RC3.
- Both manifests: 27 writes, 33 checks; arena reserve remains 0x82000.
  Snapshot format becomes 28 and companion kind 2. Storage protocol 6,
  preferences version 2, 0x50000 codec workspace and MEM2 reservations remain.
  No GAME/SYS allocation, ownership gate or validation pass was removed.
- New LML4 framing uses independent 128 KiB blocks plus an Adler32 trailer,
  distinct from Moonshine's MSL4. Dense miniz 128-probe fallback is retained.
  Both LZ4 and miniz notices accompany redistributed binaries.

## Artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii RC4 ZIP | 4919603 | `040114d8251d687304a813570ff5469c012099ce4a2fddb429d43402857bf6d5` |
| Dolphin RC4 ZIP | 113261 | `0af5666a7af205bf2e14c8a4c9b2e0c15591d25ab06ffdc77b1ef1f3c1336720` |
| Wii mod_lmj.bin | 310116 | `167ef2db7d95713d259016b5f2d21d6d6101452464aabb2dbc505541865709d1` |
| Wii boot.dol | 1568192 | `66e168660fc5ee054ca0781dba23e5d58014fcf6e12e609e362c041d5b4cf6f9` |
| RC4 BPS | 288708 | `50736d7d35c0e4742e08e9feb1d2047b41d544ae485e6b801c1ecea54b106154` |

Wii whole-mod CRC32: **70B6C02A**. Clean GLMJ01 DOL SHA-1:
`722005ea9c1eab54b114f814734d8f327e5614ee`.
Development target ISO CRC32 **710E28F2** verified by the BPS builder.
No retail ISO is packaged.

ZIP CRC/source-byte checks pass: ten Wii entries (eight app files plus two
SD-root theme assets), six Dolphin entries. Metadata and current/versioned
ten-test checklists match. Previous RC3 release ZIP hash remains unchanged.

## SD handoff

Seven existing app files were backed up and hash-verified at
`../sd-backups/lm-before-darkmoonshine-rc4-20260911/previous-app/`.
Installed eight verified app files into `D:/Apps/moonshine_luigis_mansion/`
and the versioned Wii ZIP into `D:/lm_builds/`. Existing themes retained;
every written file was flushed and source-hash checked. All **52** inventoried
pre-existing root/state/log/save/theme files retained their hashes. No files
were deleted. SD was released to the user after verification.

## Acceptance still needed

No RC4 Wii or Dolphin gameplay run was performed by the assistant. Fresh
RC4 states are required; old-build archives remain intact and incompatible.
The ten-test checklist emphasizes actual pauses, repeated overwrite/load,
cross-floor/fragile door routes, failed imports preserving RAM, and repeated
soft/full-reboot use of a named archive. Secret Altar's known ownership
refusal and other state guards remain. Earlier hardware passes do not
establish universal mansion/boss support or this candidate's runtime speed.
