# DarkMoonshine RC3 verification — 2026-09-10

Build: **V1.0.0 Frozen in Time. Release candidate 3**.

## Verified locally

- Both diagnostic presets built with the bundled PPC/ARM toolchains.
  Full host suite: **1,016 tests, zero failures/errors/skips**. Existing
  inherited launcher path-truncation warnings remain; payload compilation passed.
- Twenty-six clock tests cover the native owner discriminator, unchanged
  retail update, active/stopped behavior, mod-menu override, entry/exit and
  no-double-tick predicate. The new `8000B918` entry trampoline's PPC bytes
  are compiled and verified; original timer call `8000B9B8` stays untouched.
  Independent review found no ABI or phase blocker. No XFB mode is used as
  the clock/menu predicate, and the supplement never forces TIMEACTIVE.
- Reset binding executable tests exhaust all 65,536 low-word masks, output
  bounds, opening-button release, cancellation, reserved buttons, chord swaps,
  disconnect/menu disarming, extras and full-release debounce. Integration
  checks preserve queued PAD capture and guarded presenter-boundary dispatch.
  Independent review found no blocking issue. Partial chord components can
  still perform their ordinary game actions before the full chord forms.
- Forty-one preference kernel/client tests pass, including legacy checksum
  migration, current round-trip and I/O failures. The actual SD version-1 INI
  authenticated read-only and converted to a valid version-2 mailbox with
  both new fields absent, preserving their OFF defaults. Fixed size remains
  256 bytes, with unchanged cache ownership. No settings were rewritten on PC.
- Fifteen launcher tests cover actual FAT-helper behavior and startup wiring:
  missing/existing folders, regular-file conflicts, invalid paths, creation
  races/errors, lazy device mounts, menu-only audio and safe teardown.
  Port reference: Moonshine V2.3.0 commit
  `06cb7965032539a5ac88611b9c5aade7385daec9`.
- Timer parity/performance tests still pass: final host run reference 0.887 ms,
  cached 0.089 ms/presentation, cold 0.556 ms. Default flush remains 122,880
  bytes. These are host measurements, **not Wii speed claims**.
- Wii resident blob **295,849 bytes**, 90.3% of the 320 KiB build ceiling,
  31,831 bytes free. Emulator **274,989 bytes**. Packed manifest sizes are
  295,852 / 274,992. RC3 adds 3,216 resident bytes over RC2 on Wii.
- Both manifests contain 27 writes and 33 checks. Arena reservation remains
  `0x82000`; no new MEM2 reservation or game/system-heap allocation. Snapshot
  format 27 and storage protocol 6 unchanged. Whole Wii mod CRC32 **4F1CAADA**.
- Development BPS built from clean GLMJ01 DOL SHA-1
  `722005ea9c1eab54b114f814734d8f327e5614ee`; target ISO CRC32 **C5A0D43A**
  independently verified by the builder. No game image is packaged.

## Artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii RC3 ZIP | 4910630 | `6c412a1bd1a097d242da0cf8a674d82e999a77b5b7ee11e0c18c89d7b966932f` |
| Dolphin RC3 ZIP | 104456 | `fe98cb849bed70d6c291839149d5cef0721cfcd6a933c4d847b203f9748b8016` |
| Wii mod_lmj.bin | 296608 | `93042155b4e0ace6939c65ea5f97c4817d801795c87acc50920d7d0d9acd5e54` |
| Wii boot.dol | 1568192 | `024616d8f8ab1fcaed17a3854c8fe12175fcdfeb34541c38c19108028ec4154a` |
| RC3 BPS | 275268 | `3bd4ccb0247d110ab9b6a8c6c015d8be1f7e1f8a9d84971b9cfb116fd43039b4` |

ZIP CRC and source-byte verification passed: nine Wii entries (seven app
files plus two SD-root theme assets), five Dolphin entries. Metadata,
ten-case checklist, licence and allowlisted theme packaging tests pass.
The current and versioned checklists match. No previous released ZIP changed.

Copied supplied theme assets without editing:

- background.png: 688061 bytes, SHA-256
  `ee6f1df0e922e8d8a4028306b0da0b3afeb4b28fdaf1fb6e0fcd5121a64b7989`.
- bgm.mp3: 3111007 bytes, SHA-256
  `23605495d882438f02b5a98ab6c5621ec86c4d15cadf33d1ab35f2148182bf96`.

## SD handoff

Backed up and hash-verified all seven existing app files under
`../sd-backups/lm-before-darkmoonshine-rc3-20260910/previous-app/`.
Staged the exact nine-entry ZIP; installed seven app files to
`D:/Apps/moonshine_luigis_mansion/`, and the versioned ZIP to `D:/lm_builds/`.
Created `D:/Darkmoonshine_Theme` and copied its two missing assets. Existing
custom assets would be retained. Every written file was flushed and compared
with its source. All **50** inventoried pre-existing root/state/log/save/theme
files retained their SHA-256 hashes; no files were deleted. Original
`Moonshine_Theme`, private keys and normal saves were not changed.
The SD was released to the user after verification.

## Hardware acceptance still needed

No RC3 Wii or Dolphin gameplay test was performed by the assistant. The RC2
user passed the exercised tests; its SD reboot item was not repeated. RC3's
ten-case checklist prioritizes menu timing, bind behavior, configuration
migration, launcher/theme boot and important cross-room/reboot state routes.
Secret Altar's known unmatched event-resource refusal remains guarded.
