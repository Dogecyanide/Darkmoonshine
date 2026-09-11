# 0.3.34 verification

Built on 2026-09-06. Wii and emulator runtime results are separate from native
tests and authenticated retail-code checks.

## Latest Wii evidence

Preserved original SD logs under workspace `sd-captures/20260907-004034`.
The current crash A identifies the 0.3.33 payload CRC `A9D47118`; crash B is
older. Attempt A generation 18 and B generation 17 belong to the current run.
Several earlier loads completed, followed by a menu warp to Anteroom that
reached ARRIVED. The final load passed both saved-source and restored-copy
grain checks, then crashed on the first native HUD draw.

DSI at `8003BB3C`, DAR `0000013F`: wrapper `803C3298` references picture
`8135F448`; picture+`EC` is `101`, and the failing instruction reads `+3E`.
The fixed GBH picture wrapper was omitted from the snapshot although its
scene-owned GAME allocation was restored. The exact picture pointer at save
time is absent from the dump: ownership mismatch is established, but the
specific stale-allocation alias in this run is not proven.

## Changes

- Capture GBH screen/picture/fade owners, digit wrappers and health wrappers
  in two authenticated ranges. Exclude persistent font and destructor records.
  Added raw state: 1,152 bytes. Snapshot format 18 rejects older layouts.
- Gate save/load on the native player's door action (mode 2, state `20` hex).
  Bounded GAME reads and the exact player vtable check precede controller
  access. The fresh check runs during identity construction, including after
  audio quiescing and under freeze before writes. BUSY/DOOR is a refusal, not
  a deferred load; retry after the transition. This is not a general guard for
  all cutscenes, ladders or boss transitions.
- Independent, default-OFF **Displays -> R-pump display** toggle. Live and
  last completed hold count game-update frames. Digital R OR raw analogue
  value >= 30; press frame included, release excluded. Menus, disconnects,
  player replacement and state operations cancel partial holds. Preferences
  remain session-local. No trick-success predicate is claimed.

See [test route](lm-practice-0.3.34.md), [HUD evidence](lm-hud-state.md) and
[door evidence](lm-door-transition-gate.md).

## Automated checks

- All **348** tests pass, including native R-pump counters, menu integration,
  door predicate/bounds tests and authenticated Japanese retail HUD/door code.
- Wii PPC/ARM/launcher and Dolphin payload builds succeed. All 53 hook writes
  remain authenticated. No new hook or heap reservation was needed.
- Final source whitespace checks pass with existing LF/CRLF notices only.
- Independent HUD and door reviews completed. Runtime repetition on Wii is
  still required; tests do not prove the latest crash is resolved.

## Emulator evidence

The generated local ISO passes target CRC32 `A8F93423`. The existing isolated
Dolphin 5.0 copy booted it and displayed `LM STATE X0.3.34`, F:OK C:OK H:OK.
Evidence is saved at `build-lm-emu/dolphin-0.3.34-boot-check.png` and the
matching `.log`; the log identifies the exact ISO and arena `80538400`.
No host crash was observed. One Start input did not immediately leave the
attract loop, so no extended input/TAS/modal detour was attempted. Gameplay,
R-pump interaction and save/load remain unverified. The test window remains
open. No old native checkpoint was loaded and the downloaded newer Dolphin
was not used.

## Artifacts

Console ZIP: `build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.34.zip`

- 1,105,053 bytes; SHA256 `52DC2FC0BF137D5ECD7F3FA7C536E5B58804F68A58C127191FF55DEE705CCB27`.
- Resident payload including data/BSS: 128,249 bytes, +1,452 versus 0.3.33.
  This is 39.1% of the 320 KiB link cap, not 39.1% of the 512 KiB arena.
  The 532,480-byte arena reserve includes the separate 8 KiB debug-stack gap.
- `mod_lmj.bin`: 128,924 bytes; SHA256 `9390F04AA070393384FC9317F3523579788452E3805454A0306B48EE24FB1870`.
- `boot.dol`: 1,562,688 bytes; SHA256 `057778C44493B201785A082E8B68E1DCD4A5DBDA665C56EADAD3F3D2115302E1`.

Emulator resident payload: 125,073 bytes. BPS: 125,324 bytes, SHA256
`8E224BF11F12A2D0EB2F8470A42BFB981F76937CF30A6358D5E586F92E2F0B70`.
Clean input and earlier versioned images are preserved.

Dolphin ZIP: `build-lm-emu/Moonshine-Luigis-Mansion-Dolphin-Experimental-0.3.34.zip`,
75,230 bytes; SHA256
`32E4F05A32FE69DA6EB3D66116CC38E977F30FE93639EA59E401FB62E0AB0526`.
Contains only BPS, instructions and project/miniz licenses. No game ISO/save
is included in either distributable.

## SD installation

Preserved and verified all five previous app files at workspace
`sd-backups/lm-before-0.3.34-20260907-004034/moonshine_luigis_mansion`.
Staged the new console ZIP in `build-lm-diag/deploy-0.3.34`.
Before replacement, all five live files matched that backup. Installed only
`boot.dol`, `mod_lmj.bin`, `meta.xml`, `icon.png` and `licenses/miniz.txt` into
resolved `D:/Apps/moonshine_luigis_mansion`. Flushed each and verified its
read-back SHA256 against the staged ZIP contents. The displayed app version
is Full-State Experimental 0.3.34.

Preferences, memory-card saves, archived states, diagnostic logs and other
applications were untouched. SD access is finished and the user was told it
can be ejected. Make fresh format-18 states before testing.
