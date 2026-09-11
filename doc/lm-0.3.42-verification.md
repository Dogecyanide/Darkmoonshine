# 0.3.42 verification record

Date: 2026-09-08. Japanese GLMJ01 revision-0 experimental candidate.

## Evidence and scope

The supplied .41 report passed long natural/cross-floor routes, repeated
post-menu-warp loads and doors, game-state rewind and named same-session
archive reuse. Soft and cold reboot loads still refused. Both original ZIPs
were preserved separately under
`../sd-captures/lm-0.3.41-runner-20260908/`; see its ANALYSIS.md.
No exception reports were in those ZIPs. Of 42 logged load requests, 23
reached the restored-copy grain check, 13 refused X08 and six refused BUSY.
A passed grain check is not itself proof of continued gameplay stability.

All three supplied format-25 archives passed saved integrity checks. The
identified reboot refusal was an unrecognised `map2` volume: MissionMode,
not a model row, owns this archive. The new bounded native-owner helper
proves the saved/live mission, wrapper and complete RARC backing allocations,
their exact identities and reciprocal GAME lists. Existing retained-profile,
audio, renderer and compatibility checks remain. Other X08 cases cannot all
be attributed from .41 telemetry; new D9/DA records identify unmatched volumes.
This is a targeted reboot fix candidate, not a Wii acceptance claim.

The Sunshine timer uses the owner's retail textures and native pane geometry,
including per-pane rotation. A heapless Moonshine-style Creation editor edits
the nine visible targets and independent streak. LM's native captured clock
supplies time. Mod menus pause it; loads rewind it. Native event commands
remain, with verified GaddWarp Boo capture/introduction stop/resume handling.
The native clock rolls over after 36 counted minutes. Cemetery branch-specific
stops and custom Dojo/rush event assets are not fully mirrored; this is not an
unlimited full-run clock or fully certified GaddWarp timing replacement.

Wii preferences use `/moonshine_lm.ini`, `[lm_preferences]`, on the launcher's
device. Menu closure saves changed display, timer, colour, reference and
Boo-safe preferences; completed reference recordings also queue a save.
Gameplay progress and snapshot contents are excluded. A 256-byte cache-line
separated mailbox uses the existing LM config reservation. The kernel retains
unknown/launcher content and uses synced temporary/backup replacement.
Allocation and write failures report errors, not a console shutdown.
The exact acknowledgment is `SETTINGS: SAVED TO SD`. Dolphin preferences
remain session-only. No MEM1 or MEM2 reservation increase; snapshot format 25
and storage protocol 5 remain. Telemetry capability is now version 2.

## Verification

- Full host suite: **884 tests passed, zero failures/errors/skips**.
- Includes 18 actual kernel preferences tests and 16 PPC client tests: delayed
  saves, acknowledgment ownership, coalescing, CRC/presence validation, partial
  fields, short reads/writes, sync/close/rename/rollback faults, boot recovery,
  nonfatal allocation failure and repeated-save byte stability.
- Timer tests: 20 clock/retail-hook cases, seven rendering cases, and the
  executable Creation editor harness. Native map proof has 11 tests, including
  the saved graphs in all three newly supplied archives. These are host tests,
  not real SD power-cut or gameplay tests.
- Final full ARM/Wii launcher and both PPC payload builds passed. Wii resident
  blob: **226,181 bytes**, 69.0% of the existing 320 KiB working limit;
  Dolphin: **206,245 bytes**, 62.9%. Manifest size includes final alignment.
- Clean JP DOL SHA-1: `722005ea9c1eab54b114f814734d8f327e5614ee`.
  Applying the generated BPS reproduces target ISO CRC32 **5A605261**.
- ZIP CRCs, exact entry sets, source-file identity, version and equal
  current/versioned 16-case checklists pass. Wii ZIP has seven app entries;
  Dolphin ZIP has five entries. No ISO, game save or SD key is in either ZIP.
- **No interactive .42 Wii or Dolphin runtime pass was performed.** The
  supplied .41 route passes do not certify new timer/persistence/reboot code.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Wii ZIP | 1,160,063 | E1B8DCDF84408DE5F035A13DB57189FBFB717B3EC063E061DEA2699FBDD691C8 |
| Dolphin ZIP | 102,165 | B2BC7D86BE7404D57790933040FE676E70EB7C0ACE27C37F87E8AF2669CC74CD |
| Wii mod_lmj.bin | 226,940 | F13AB2251905E28E2E68DA8B828C197400402F04B9B569AA2FFD417747F67105 |
| boot.dol | 1,569,184 | EF029B88530A78DCD75F1243427D95853813AA597726CD82AC784822094D2957 |
| Dolphin BPS | 206,524 | 9701CE26037ED386DA32516AC7F668281CEA649CEF6BDF82AE4C0EAE8BD44316 |

Wii whole-mod CRC32: **9D445DA8**. Use fresh .42 states; cross-build archives
remain incompatible. Install the complete matching app folder.

D: became readable at the user's subsequent cleanup request. The seven app
files and matching versioned ZIP were installed, explicitly flushed and
reread-hashed. The ZIP is at `D:/lm_builds/`; .41 remains there as a fallback.
The prior app is backed up under
`../sd-backups/lm-cleanup-and-before-0.3.42-20260908/previous-app/`.

Cleanup archived 94 files / 45,134,145 bytes: ten old LM ZIPs and the 21-build
`D:/Apps/moonshine_luigis_mansion-backups/` tree. Every file was copied to
`retired-SD-files/` inside the same PC backup and hash-verified before removal.
`retired-manifest.csv` records source, backup, size and hash. This is recoverable
PC archiving, not loss of the previous builds. Fifty-nine tracked SD root
data/settings/log files and LM states/journals were hash-checked unchanged.
Games, other applications and recovery files were not altered.

Newly available root crash files and two legacy attempt journals were preserved
at `../sd-captures/lm-pre-0.3.42-20260908-live/`. The four crash files match an
earlier capture byte-for-byte: .38 picture cleanup and .37 dialogue cleanup.
The two journals are .40 successful save-only records. They are not new .41
crashes and supply no additional reason to hold the .42 candidate.
