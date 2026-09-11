# 0.3.35 verification and deployment

## Implemented scope

See [release changes](lm-practice-0.3.35.md),
[scene-reload ownership proof](lm-state-scene-reload.md),
[room tools](lm-room-tools-runtime.md), and
[Phob/PAD mode audit](lm-pad-analog-mode.md).

The latest 0.3.34 Wii attempts were X06/X08 refusals after menu reloads;
the crash files on that SD were older. Originals are preserved in the
workspace's `sd-captures/lm-0.3.34-20260907` directory.

The two mode-3 initialization writes and their three surrounding signatures
bring authentication to 25 modifying words plus 33 check-only words.
No unverified L/R field swap was introduced. No controller firmware or
settings were changed, and no usage reset was consumed.

## Local checks

- Full `test_*.py` discovery: **395 tests passed**, including native helper
  execution, authenticated clean-JP retail instructions, packaging, state
  resource/model closure, room recipes, trigger decoding and popup timing.
- Independent bounded review found no blocking issue in the post-warp
  ownership checks or the new VR table capture. The element setter also
  received an independent review of its native two-field write.
- Console and emulator builds passed. Console resident payload: 135,541
  bytes (41.4% of the 320 KiB working cap, about 25.9% of the 512 KiB window).
  Emulator resident payload: 132,361 bytes.
- Snapshot format 19 adds 480 raw bytes, not a larger MEM2 reservation.
- Clean ISO build authenticated retail DOL SHA1
  `722005ea9c1eab54b114f814734d8f327e5614ee` and verified patched target
  ISO CRC32 `961A722B`. Original ISO was not modified.
- The computer-use check ran the pre-existing Dolphin 5.0 isolated copy,
  booted 0.3.35 and observed title/attract screens with F/C/H OK. Short
  automated game-key presses remained unreliable, so **no interactive
  savestate, room-tool or cross-floor gameplay pass is claimed**. The test
  was stopped afterward. Boot evidence is in `build-lm-emu/` as
  `dolphin-0.3.35-boot-check.png` and `.log`.
- Source diff whitespace check passed. Git's existing LF/CRLF warnings are
  not build/test failures. Accumulated unrelated working-tree changes remain.

## Packages

Both ZIPs include the same 47-test runner checklist. Numbering and packaged
text were checked against the source. Neither contains a game ISO or save.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Wii ZIP | 1,116,714 | `DD3ED3C0A7EE89204A9234BBF56F4EB7966091915000C4A7B555955F578F7AD4` |
| Dolphin ZIP | 86,966 | `06AA86DE75CBB29C97C758CBD3C91F98FB24836F6CBD77FF63FC3A955DC09FD6` |
| Dolphin BPS | 132,625 | `05504EE19DF45BF53DDDC2EEC1408DB0F4B98BA16EE720DD8CB636B8EBF4499C` |
| Packed Wii mod | — | `8F1AE32313207ECF3867D1AA371E4C1192EDB1033F72AA11CC99219288868876` |

The versioned Wii ZIP and compatibility-name launcher ZIP are identical.

## SD installation

The old five app files were compared against the existing backup at
`sd-backups/lm-before-0.3.35-20260907/moonshine_luigis_mansion` before writing.
Only boot.dol, icon.png, meta.xml, mod_lmj.bin, licenses/miniz.txt and the
new TESTING.md were installed under `D:/Apps/moonshine_luigis_mansion`.
Each file was flushed and hash-verified by readback. The versioned Wii ZIP
was also copied and verified under `D:/lm_builds`.

Saves, launcher settings, diagnostic/crash logs, theme files and other apps
were left unchanged. SD access has finished; the card can be removed safely
after normal operating-system ejection.

## Still required

Wii confirmation of the user's physical Phob L/R channels and all new room
tool behavior. Repeated post-warp and cross-floor restores must survive
movement and normal doors, not only show one restored frame. These remain
the main release blockers; this is not advertised as mansion-wide 1.0.

Boss-map state changes, cross-reboot SD-state portability and persistent
in-game preferences remain limited. Dojo/rush are deferred, and unported
GaddWarp features are explicitly listed in the room-tool documentation.
