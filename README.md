# DarkMoonshine

## V1.0.0 Frozen in Time

A practice mod for **Luigi's Mansion (Japan, GLMJ01 revision 0)**, built on
[Moonshine](https://github.com/panther03/moonshine)'s Wii/Nintendont foundation.

**Authors: Dogecyanide, Nintendont Team.**

The Wii launcher injects the mod into an unmodified game image at boot.
**No patched ISO is required.** Supply your own legally dumped Japanese game;
no retail ISO or game save is included.

Dogecyanide reports that **all ten RC4 Wii checklist items worked**, including
the repeated-load, cross-floor, post-warp, SD-import and reboot cases.
V1.0.0 promotes that gameplay implementation with final release branding.
This is route-specific hardware feedback, not a promise that every boss,
cutscene or resource transition supports savestates.

See the [release notes](doc/darkmoonshine-1.0.0.md) and the packaged
[runner checklist](doc/lm-testing-current.md).

## Features

- **One RAM savestate**, with D-pad save/load and guarded cross-room,
  cross-floor and post-menu-warp restores.
- **Named SD archives:** an in-game keyboard, browser, import/export, rename,
  and confirmed deletion. Eligible `REBOOT READY` archives can be reused after
  soft reset or a full Wii reboot with the same compatible build and setup.
- **Room warps and tools:** 68 named room/boss destinations, supported native
  room reset/clear recipes, and a configurable Reset Room button combination.
- **Sunshine-style timer:** original angled artwork, visibility and TIME-icon
  switches, per-character colour and layout editing, and a separate streak
  editor. The optional native-menu counting setting is persistent; the mod
  menu always counts active game time.
- **Practice displays:** Moonshine-style controller display, position/angle/
  horizontal-speed metadata, lag counting, R-pump hold frames, and recorded
  input-timing references.
- **Game options:** normal/Hidden Mansion selection, Boo and blackout options,
  health presets, door/trap settings, Poltergust tank presets, plant presets,
  and background-music controls.
- **Luigi colour:** change his shirt and cap RGB colour.
- **Persistent Wii preferences** in `moonshine_lm.ini`, plus a themed launcher
  with Auto Boot and on-demand storage startup.
- **Faster state processing:** bounded LZ4 compression with denser Deflate
  fallback and equivalent table-based checksums. Integrity and ownership
  checks remain in place.

The game continues running while the mod menu is open. A memory savestate is
separate from a normal memory-card save; the menu's game-save action writes
retail progress, not a savestate.

## Install on Wii

Extract `DarkMoonshine-1.0.0-Frozen-in-Time.zip` so the SD card contains:

```text
apps/moonshine_luigis_mansion/boot.dol
apps/moonshine_luigis_mansion/icon.png
apps/moonshine_luigis_mansion/meta.xml
apps/moonshine_luigis_mansion/mod_lmj.bin
Darkmoonshine_Theme/
```

Install the matching launcher and payload together. Keep existing custom theme
files if preferred. Launch **DarkMoonshine** from the Homebrew Channel and
choose your clean Japanese game image. The filename need not be `GLMJ01.iso`;
the launcher checks the actual game identity.

Existing app, settings, archive and log paths intentionally retain their old
names. Do not create a second app directory when upgrading. Back up normal
saves and the complete `lm_states` directory, including its private key and
name records. Keep those keys private.

**Make fresh V1.0.0 archives after upgrading.** Snapshot format remains 28,
but final branding changes the authenticated build identity. RC4 and earlier
archives are not compatible with the final build; renaming them cannot
convert them. Existing archives and preferences are not deleted by upgrading.

## Controls

| Action | Control |
| --- | --- |
| Save to RAM | D-pad Left |
| Load from RAM | D-pad Right |
| Open practice menu | D-pad Down |
| Choose a category or row | Control stick or D-pad |
| Enter/apply | A |
| Back/close | B |
| Change page / adjust a value | Follow the displayed L/R and D-pad hints |

Editors, naming and deletion show their own controls and confirmation prompts.
**Import fills the RAM slot; Load restores gameplay.** Wait for the completion
notice before removing storage. Closing a changed menu saves preferences;
wait for its saved acknowledgement before powering off.

## Limits and reporting

- Secret Altar can introduce unmatched event resources that safely refuse a
  load of an earlier mansion state. Boss/map boundaries are not universally
  supported. Room warps and savestate compatibility are separate features.
- `BUSY`, `EPOCH` and capacity refusals are safety decisions, not invitations
  to bypass the checks. Wait until a door/transition settles before trying again.
- There is **one resident state**, not two. Named SD archives provide a library
  without borrowing live staging or rollback memory.
- Timing displays measure inputs; they do not yet certify pearl dupes or
  Chauncey one-cycle success. Dojo, Boss Rush and Portrait Rush are deferred.
- The timer follows native game updates and scripted stops, with the native
  36-counted-minute rollover. It is not a wall-clock timer.
- Dolphin is a separate development target; Wii SD services and preferences
  are not supplied by the Dolphin BPS alone.

After a problem, preserve the **entire `lm_dumps` folder**, available
`luigis_mansion_crash_a/b.bin` and `.txt` reports, and `ndebug.log`.
Include the exact rooms/actions and whether failure occurred during Import,
Load, or subsequent gameplay. The eight rotating attempt files are diagnostic
journals, not eight savestates. Share affected archives/name records privately
when requested, but never include the SD authentication keys.

## Build and developer references

With Python, CMake, Ninja, Git LFS and the repository's LFS objects available:

```powershell
python setup_venv.py
cmake --preset diagnostic_console
cmake --build --preset diagnostic
```

Outputs include the versioned ZIP and the byte-identical compatibility filename
`build-lm-diag/moonshine_luigis_mansion_launcher.zip`. Each package includes
`TESTING.md`. The [payload guide](lm_diag/README.md),
[storage contract](doc/lm-state-storage.md),
[compression benchmark](doc/lm-compression-benchmark.md), and
[current priorities](doc/lm-current-priorities.md) cover implementation details.
For local emulator builds, use the separate
[Dolphin development guide](doc/dolphin-development.md).

The inherited Sunshine payload remains as porting reference. Its targets are
hidden unless explicitly configured with `-DLM_BOOTSTRAP=OFF`; never apply
Sunshine DOL/BPS/mod outputs to Luigi's Mansion.

## Lineage and credits

- [Moonshine](https://github.com/panther03/moonshine), by Dogecyanide,
  panther03 and contributors, provides the savestate and launcher foundation.
- [Nintendont](https://github.com/FIX94/Nintendont) and
  [Better Nintendont](https://github.com/SuperrSonic/Better-Nintendont) provide
  the GameCube-on-Wii runtime.
- [Yasiki](https://github.com/Moddimation/Yasiki),
  [Booldozer](https://github.com/ColinShark/Booldozer), and the Luigi's Mansion
  decompilation community provide reverse-engineering references.

The timer artwork originates from Super Mario Sunshine; supplied launcher
theme assets retain their original provenance. Existing source/license
notices are retained, including the packaged miniz and LZ4 notices.
