# DarkMoonshine RC2 verification — 2026-09-10

Build: **V1.0.0 Frozen in Time. Release candidate 2**.

## Verified locally

- Both diagnostic presets configured and built successfully with the bundled
  PPC/ARM toolchains. Full host suite: **986 tests, zero failures/errors/skips**.
- Native timer call `8000B9B8` is no longer hooked. The patched Dolphin ISO
  retains its original `48056091` call word, so menu state cannot skip it.
  Host/native-DOL tests preserve frame conversion, native active flags,
  reset/event behavior and snapshot inclusion.
- Presenter gate authenticates native double-buffer byte `804A0BBA` and its
  screenshot entry/exit setters. Only optional gameplay overlays are hidden
  during single-buffer presentation; no VI/buffer-mode state is changed.
- Fifteen timer-renderer tests plus independent review verify cold/warm cache
  parity, nonuniform fresh backgrounds, digit/style changes, offscreen bounds,
  oversized fallback and dirty-row coverage. Creation tests pass unchanged.
- Default running-timer host result in the full suite: reference 0.827 ms,
  cached 0.088 ms/presentation; cold 0.550 ms. Texture decoding work drops
  about 89%; default flushing drops 80%. These are **not Wii measurements**.
- Element wait reasons preserve every existing predicate, ordering and the
  same two game writes. No safety gate was removed to make a preset apply.
- Wii resident blob: **292,633 bytes** (89.3% of the 320 KiB build ceiling);
  emulator: **271,933 bytes**. Packed manifest sizes: 292,636 / 271,936.
  Fixed timer cache: 66,216 bytes including keys, within the existing reserved
  MEM1 mod region. No extra GAME/SYS allocation or MEM2 reservation.
- Both manifests: 26 writes, 33 checks; unchanged `0x82000` arena reservation.
  Snapshot format 27 / storage protocol 6 unchanged; new build identity still
  requires fresh RC2 archives. Whole Wii mod CRC32: **500F70B6**.
- BPS built from authenticated clean Japanese DOL SHA-1
  `722005ea9c1eab54b114f814734d8f327e5614ee`; target ISO CRC32 **F59A35E5**
  independently verified by the builder. No retail ISO is packaged.

## Release artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii RC2 ZIP | 1161833 | `9cf12714cd04abde9af9c17cd30d0b4d6e897a619e5046fd2d10d33df806d8be` |
| Dolphin RC2 ZIP | 102593 | `2dc755526fb71d3dedd4595af79e743510321518d338e6a82cb32d441f6aed0b` |
| Wii mod_lmj.bin | 293380 | `3537129eff984f6bd6ce5e63ca99759ab57771a5ae18689680c195a7a14e924d` |
| Wii boot.dol | 1570464 | `7c360a7b4c6ec9dc10a42efe0d9a3884e942a32d5c43182fd6172756f7169225` |
| RC2 BPS | 272205 | `3b49f4b3c34d5c57483d45ee02909981489eb40e23027b102acfdd073461ad08` |

Both ZIPs pass CRC verification and exact entry-set/source-byte checks: seven
Wii entries, five Dolphin entries. Metadata name/authors/version, miniz license,
ten numbered cases and byte-identical current/versioned checklists verified.
Prior RC1 release artifacts were not overwritten.

## SD handoff

Backed up and hash-verified the seven existing RC1 app files at
`../sd-backups/lm-before-darkmoonshine-rc2-20260910/moonshine_luigis_mansion/`.
Installed only the seven staged RC2 app files into
`D:/Apps/moonshine_luigis_mansion/`, plus the versioned ZIP into `D:/lm_builds/`.
Every destination was flushed and hash-compared with its source. All 48 tracked
root/state/journal/normal-save files retained their pre-install lengths/hashes.
No files were deleted. The SD was released to the user after verification.

## Remaining acceptance

No RC2 Wii or Dolphin gameplay run was performed by the assistant. Host
parity/build validation is not real-hardware timing or cache acceptance. The
ten-case checklist prioritizes the corrected untimed-menu exploit, lag A/B,
native menu flicker, edited layouts, and SD/warp/state regressions. Large
uncached custom timer panes can still cost more than native-size panes.
Secret Altar's known unmatched-event state-load guard remains; the RC1 logs
show a successful separate Room Warps exit to Artist's Studio. This is not
unrestricted mansion/boss savestate acceptance.
