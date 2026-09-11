# 0.3.33 verification

Built on 2026-09-06. Emulator and Wii runtime results are tracked separately.

## Wii evidence and changes

- The user confirmed 0.3.32 menu warps and D-pad saving worked. A save followed
  by a menu warp to Anteroom and a load then crashed.
- Original SD files were copied to workspace `sd-captures/20260906-224608`.
  Crash B is the current 0.3.32 payload, CRC `D0E41038`; crash A and attempt B
  are older and were not treated as this reproduction.
- The current attempt completed LOAD/7F, then crashed in the first draw:
  DSI at `8012E3A4`, DAR `0000004B`, `r3=FFFFFFFF`, grain controller
  `8129E490`. The preceding load reads controller+`94`; retail initialization
  makes that field a self-sentinel at controller+`40`.
- Three model-effect managers were omitted from fixed state, although their
  private heaps and model arrays were already copied with GAME. Their model
  update runs before the failing renderer and can write the exact failed
  field through a stale model pointer. The omitted owners and instruction
  ordering are proven; the specific alias in this Wii run remains unproven.
- Capture three separate `0x2A8` managers, excluding intervening destructor
  registrations. Snapshot format 17 rejects older layouts. Added raw state is
  2,016 bytes after alignment; heap reservations are unchanged.
- Read-only grain validation checks the live save source and saved load source
  before destructive snapshot/game writes. Post-copy validation is diagnostic
  only. All three records use the bounded critical queue, preserving both fault
  address and value against sampled draw telemetry.
- Player room IDs occupy the low byte of a packed word. Correct decoding fixes
  false ROOM MISMATCH results without changing the destination list or widening
  archive ownership guards.

See [changes and retest](lm-practice-0.3.33.md) and
[authenticated retail proof](lm-effect-manager-reload-proof.md).

## Automated checks

- All 327 tests pass. Native helpers cover grain validation with two existing
  real MEM1 captures, invalid pool bounds and poisoned sentinel nodes; retail
  tests authenticate the Japanese DOL before checking instruction boundaries.
- Native telemetry tests model independent PPC/ARM caches, queue publication,
  old-build fallback, overflow and survival through 1,000 sampled draw events.
  Parser tests preserve the two raw fault words and label only matching phases.
- Independent final review found no new blocking hazard. Compiled validator
  stack usage is 64 bytes; the source mapper is a leaf with no stack frame.
- Wii PPC/ARM/launcher build and emulator build succeed. All 53 hook writes
  remain authenticated; no new hook was required.
- Source whitespace checks pass, with existing Git LF/CRLF notices only.

## Emulator evidence

The task-specific portable Dolphin 5.0 loaded the 0.3.32 injected ISO. When the
user attempted to enter the completed Hidden Mansion file, the host process
exited. Windows recorded APPCRASH `0xC0000005`, module offset `0x6B1A48`, for
`LM-Test-Dolphin.exe`. No native checkpoint or PPC exception was captured, and
no savestate-load reproduction ran. Logs were preserved in
`build-lm-emu/diagnostic-capture-0.3.32-host-exit`.

This is not evidence of the same failure as the Wii's post-load DSI. The cause
of the host crash remains unproven. The subsequent 0.3.33 attempt booted and
displayed its HUD using the same isolated Dolphin 5.0 copy without the TAS
panel. No new host crash was observed, but injected controller taps did not
reliably leave the title/attract loop. Gameplay and the save/warp/load route
remain unverified. No older native checkpoint was loaded over the new code.

An official 2606a portable copy was downloaded, but the user chose to stay with
the installed version. That downloaded executable was never run; optional
test-script changes for it were reverted. Personal Dolphin profiles and saves
were left untouched.

## Build artifacts

Console ZIP: `build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.33.zip`

- 1,104,284 bytes; SHA256 `795E8AA8FC23AE3AA953DA3B73FEFA20A72305F560AFB8E866BB8919C9AE1A89`.
- Resident payload including data/BSS: 126,797 bytes, 992 bytes over 0.3.32.
  The 532,480-byte arena reserve includes the separate 8 KiB debug-stack gap.
- `mod_lmj.bin`: 127,472 bytes; SHA256 `7FD08E6F8F74F6AEE596576651CF6C79C2E83C58669338F5566DC1B09E0CE62F`.
- `boot.dol`: 1,562,688 bytes; SHA256 `CD0A016F7CB24A4A318B70F0954D6F57CF67A6A29F6F8FD2BEC464662CD416A3`.

Emulator payload: 123,621 bytes. BPS: 123,872 bytes, SHA256
`F3C1E7C417C9E1840B2968E6F173C016C2F89DDECD89F8B89F17694CAD2F5D4D`.
The separately generated local ISO passes target CRC32 `7DC9A42D`; clean input
and earlier versioned images are preserved. No retail ISO/save is distributed.

Dolphin ZIP: `build-lm-emu/Moonshine-Luigis-Mansion-Dolphin-Experimental-0.3.33.zip`,
74,237 bytes; SHA256
`85B6A87FB84AA15B9461E0C3CD174B25BB6761182CE7B940443E087930A62065`.
Contains only BPS, instructions and project/miniz licenses. The release README
explicitly distinguishes the earlier Dolphin setup from unverified 0.3.33 play.

## SD status

The current 0.3.32 app was copied and all five files hash-verified at workspace
`sd-backups/lm-before-0.3.33-20260906/moonshine_luigis_mansion`.
The new console ZIP is extracted under `build-lm-diag/deploy-0.3.33`.
Before replacement, all five live files still matched the preserved 0.3.32
backup. Installed only `boot.dol`, `mod_lmj.bin`, `meta.xml`, `icon.png` and
`licenses/miniz.txt` into resolved `D:/Apps/moonshine_luigis_mansion`, flushed
each file and verified its read-back SHA256 against the staged ZIP contents.
Preferences, game saves, archived states and diagnostic logs were untouched.

SD is released after this verification. This is an experimental candidate,
not a claimed runtime fix. Make fresh format-17 states, then retest Parlor
save -> menu warp to Anteroom -> load, followed by movement and door use.
