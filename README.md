# Moonshine Luigi's Mansion

An experimental port of [Moonshine](https://github.com/panther03/moonshine)'s
Wii/Nintendont savestate architecture to the Japanese release of Luigi's
Mansion (`GLMJ01`).

## Current status

`Full-State Experimental 0.3.19` is the current hardware-testable state build.

- The custom Nintendont launcher accepts only the verified Japanese `GLMJ01`
  revision-0 executable for injection.
- A measured 512 KiB MEM1 window holds the LM payload without patching the ISO.
- One transactional slot uses Moonshine's protected 15.94 MiB MEM2 bank.
- The slot captures the complete secondary gameplay heap, its allocator and
  disposer metadata, audited gameplay-static SDATA/SBSS slices with known
  live-owned blocks excluded, LM's renderer state, persistent camera
  descriptors and manager tables, the main-loop control pair, both
  grain-effect managers and their list sentinels, the verified room/map flag
  slice, the fixed room-streamer and model-resource tables, the mounted-volume
  list header, and libc RNG state. The three persistent camera-view objects
  receive a guarded sidecar only if they are outside the gameplay heap.
- Save/load is refused while DVD, ARAM, or memory-card work is active, while
  the heap is unstable, when a slot checksum fails, or when the observed live
  allocator/resource markers differ from the saved ones.
- Each transaction drains LM's prior JAudio scene handles while preserving its
  required replacement bootstrap handle, then holds the OS scheduler while
  only lock-free snapshot work runs.
- Moonshine's ARM crash writer now accepts LM exception reports and rotates
  `susamune_crash_a/b.bin` plus readable `.txt` reports on the game-source
  storage device.
- A cache-coherent phase journal records the last completed save/load step in
  `/ndebug.log`, even when the PowerPC hard-locks and no exception is raised.
- State requests run after LM's complete framebuffer/retrace routine, matching
  Moonshine's proven post-draw timing. Additional journal markers split the
  first restored draw into matrix, scene-callback, and projection stages, then
  identify the exact direct renderer call if the callback does not return.
- Post-load tracing records eight complete restored frames, then keeps a
  low-rate two-minute tail. A successful save during that tail refreshes its
  deadline. A fresh A-button press opens a 240-frame door watch: the initiating
  `MAIN GAME` update is traced exactly, and changes in room, scene, streaming,
  resource, or archive state re-arm exact tracing for two updates without
  tracing every quiet frame.
- Cross-room checks retain a complete 22-field epoch mask plus the saved and
  live values of the highest-priority mismatch on both the overlay and in
  `/ndebug.log`.
- The first cross-room path is deliberately narrow. It accepts only volume
  count/head drift (`M00000180`) with an unchanged tail and all other epoch
  fields unchanged, then requires a bounded, validated ordered substitution
  whose surviving volumes form an exact common subsequence.
- Generation-keyed volume, seven-slot room-streamer, and 262-entry model-table
  censuses must also prove that the changing archives and backing allocations
  are game-heap-owned and that no asynchronous resource operation is live.
- On an accepted load, the raw game heap and captured fixed owner tables rewind
  together, the saved `JKRFileLoader` links are repaired, and both GX vertex and
  texture caches are invalidated before gameplay resumes. The experiment does
  not call LM's archive unload/load or room-reconcile routines.

Controls are D-pad Left to save and D-pad Right to load. Confirm same-room
restores first, then repeat the two bounded tests that produced the 0.3.14
captures: save at the foyer bottom and load at the top, followed by save before
a foyer door and load after it. `0.3.19` may attempt those restores instead of
returning `EPOCH`; a successful load is evidence for this specific resource
shape, not general cross-room support. Any different room, floor, transition,
or asynchronous state is expected to refuse safely. This remains a crash-risk
feasibility test. Audio may remain silent after a save or load until game logic
starts the room sequence again.

The inherited Sunshine payload remains in the repository as porting reference.
Its build targets are hidden unless CMake is explicitly configured with
`-DLM_BOOTSTRAP=OFF`; do not apply those DOL/BPS/mod-bin outputs to Luigi's
Mansion.

## Goal

The target is exact savestates between any ordinary mansion rooms in `map2`,
including transitions between floors. Floor is not the compatibility boundary:
allocator and resource lifetime are. Boss arenas and other maps are explicitly
out of scope for the first implementation.

As with Moonshine, the release architecture must boot a clean, verified
Japanese `GLMJ01` image and inject the game-side payload at runtime through the
custom Nintendont launcher. A pre-patched ISO is neither a user requirement nor
a distributable release artifact.

See [the GLMJ01 porting plan](doc/glmj01-porting.md) for the architecture,
runtime gates, and test ladder.

## Build the launcher

On Windows with Python, CMake, Ninja, Git LFS, and the repository's LFS objects
present:

```powershell
python setup_venv.py
cmake --preset diagnostic_console
cmake --build --preset diagnostic
```

The build emits a version-labelled tester package plus a stable compatibility
name:

```text
build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.19.zip
build-lm-diag/moonshine_luigis_mansion_launcher.zip
```

Both ZIPs are byte-identical. Use the version-labelled file when sharing a
build; every future `LAUNCHER_VERSION` automatically gets its own filename.

Extract it so the SD card contains:

```text
apps/moonshine_luigis_mansion/boot.dol
apps/moonshine_luigis_mansion/icon.png
apps/moonshine_luigis_mansion/meta.xml
apps/moonshine_luigis_mansion/mod_lmj.bin
```

The launcher stores its own settings in `/moonshine_lm.ini`. No game image is
included or accepted into this repository; test with a legally dumped Japanese
disc or ISO.

## Wii experimental-state test

Back up any real memory-card data, install the four packaged files under
`apps/moonshine_luigis_mansion/`, and launch a clean revision-0 GLMJ01 image.
The overlay must start with `LM STATE X0.3.19`; wait until `F`, `C`, `H`, and
`G` are `OK` and `ST` is at least 3. The trailing `X` byte reports the guarded
cross-room path: `X00` means it has not been attempted, `XA0` means it passed,
and `X01` through `X08` identify the refusal stage: epoch mask, saved-census
generation, volume census, list topology, archive ownership, room streamer,
model census, or model replacement shape. If `ST` remains zero, photograph the
short gate name and eight-digit value shown after `G:`; they identify the
rejected live condition without weakening it.

Press D-pad Left once. `S:SAVED` and a nonzero `SZ` confirm a committed slot.
Change a visible state in the same room, then press D-pad Right once. A good
first restore says `S:LOADED`. `BUSY`, `BADCRC`, `BADHEAP`, `EPOCH`, or
`TOOBIG` is a deliberate refusal and should be photographed with the rest of
the overlay. For a preflight mismatch that reports `EPOCH`, the `E:` row
identifies the first differing field, the `M` value records every differing
preflight field, and the final pair is `saved>live`. A later generic refusal
can still show `E:NONE M00000000`.

For a volume mismatch, `V:` shows saved/live member counts, total removals and
additions, and whether the live order is an exact saved-list suffix (`HEAD1`,
`HEAD2`, or `HEADN`). Six reserved rows show up to three removals followed by
three additions. Each row gives object/backing ownership, the archive object
(`O`), RARC header (`R`), and exact RARC size in bytes. `VR` marks saved
removals whose object or RARC allocation was reused by an added archive; `VC`
and `D` compare the current-volume pointer and directory ID. Only the guarded
front-substitution shape described above can proceed; every other mismatch
still returns `EPOCH`.

`RM` is LM's streamed room-archive manager. It compares the seven active room
IDs (`A`), their complete 0x40-byte records (`R`), manager layout (`L`), the
256-entry room map (`G`), fixed backing pointers (`K`), and transient reconcile
marks (`M`). `RA` gives the first two changed active slots as
`slot:saved>live`; `RW` compares the wanted-room set, reports any ordered
sequence change as `Q`, and prints its first removed and added IDs. `FFFFFFFF`
means that side has no displayed change. This manager is separate from the
model archives named in the `V` rows. Its fixed control state is now included
in the snapshot and its invariants are part of the narrow cross-room guard.

`MM` is the separate 262-entry model-resource owner used by archives such as
`tenjyo`, `bat`, `rat`, and `door`. It compares both of LM's writable model
tables, reports their saved/live hashes and the total number of changed model
indices, then names the first four. `P`, `R`, or `B` after the index means the
primary table, secondary registry, or both changed. Both tables and their
fixed output arrays now rewind with the game heap; their census still has to
pass the guarded cross-room checks before a restore is attempted.

After same-room restores repeat reliably, try only the foyer stair and adjacent
door cases first. `S:LOADED` means the guarded raw rewind completed. `EPOCH`
means the observed transition fell outside this experiment's accepted shape;
photograph the full diagnostic panel rather than retrying through a different
transition. If the game crashes, save the
newest `susamune_crash_a.txt` or `susamune_crash_b.txt` from the game-source
device before the next experiment overwrites the older rotating report.
If it hard-locks or reboots without a new crash report, return the SD card and
preserve `/ndebug.log`. For roughly two minutes after a load or later save, a
normal A press at a door starts a four-second transition watch. Its final
`Susamune: phase` line can identify the exact retail call that did not return.

## Lineage and credits

- [Moonshine](https://github.com/panther03/moonshine), by Dogecyanide,
  panther03, and contributors, supplies the savestate and launcher foundation.
- [Nintendont](https://github.com/FIX94/Nintendont) and
  [Better Nintendont](https://github.com/SuperrSonic/Better-Nintendont) supply
  the GameCube-on-Wii runtime.
- [Yasiki](https://github.com/Moddimation/Yasiki),
  [Booldozer](https://github.com/ColinShark/Booldozer), and the Luigi's Mansion
  decompilation ecosystem provide reverse-engineering reference material.

This is experimental software. Keep real memory-card data backed up while
testing early builds.
