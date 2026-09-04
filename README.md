# Moonshine Luigi's Mansion

An experimental port of [Moonshine](https://github.com/panther03/moonshine)'s
Wii/Nintendont savestate architecture to the Japanese release of Luigi's
Mansion (`GLMJ01`).

## Current status

`Full-State Experimental 0.3.25` is the current hardware-testable state build.

- The custom Nintendont launcher accepts only the verified Japanese `GLMJ01`
  revision-0 executable for injection.
- A measured 512 KiB MEM1 window holds the LM payload without patching the ISO.
- One transactional slot uses Moonshine's protected 15.94 MiB MEM2 bank.
- The slot captures the complete secondary gameplay heap, its allocator and
  disposer metadata, audited gameplay-static SDATA/SBSS slices with known
  live-owned blocks excluded, the scalar halves of LM's fixed door/fade
  controller, renderer state, persistent camera descriptors and manager
  tables, the main-loop control pair, both grain-effect managers, the JPA
  particle manager and their list sentinels, LM's fixed transient
  animated-model owner registry, the door lookup/transition banks and their
  heap pointers, room/door visibility masks, the complete room event/text
  interpreter and request, the room actor-pointer table, the fixed
  room-streamer and model-resource tables, the active-event bitmap, the
  fixed scene-effect controller/list state,
  mounted-volume list header, and libc RNG state. The fade controller's
  embedded `J2DPicture` remains live;
  the three persistent camera-view objects receive a guarded sidecar only if
  they are outside the gameplay heap.
- Save/load is refused while either DVD worker is not provably asleep on an
  empty queue, while DVD, ARAM, or memory-card work is active, while the heap
  is unstable, when a slot checksum fails, or when the observed live
  allocator/resource markers differ from the saved ones.
- Each transaction drains LM's prior JAudio scene handles while preserving its
  required replacement bootstrap handle, then holds the OS scheduler while
  only lock-free snapshot work runs.
- Moonshine's ARM crash writer now accepts LM exception reports and rotates
  `susamune_crash_a/b.bin` plus readable `.txt` reports on the launcher's
  storage device.
- A cache-coherent phase journal records the last completed save/load step in
  `/ndebug.log`, even when the PowerPC hard-locks and no exception is raised.
- GLMJ builds also create `/lm_dumps` and rotate two compact binary attempt
  journals. A successful save starts the next generation; every phase the ARM
  observes afterward is synced without copying the MEM2 snapshot.
- State requests run after LM's complete framebuffer/retrace routine, matching
  Moonshine's proven post-draw timing. Additional journal markers split the
  first restored draw into matrix, scene-callback, and projection stages, then
  identify the exact direct renderer call if the callback does not return.
- Post-load tracing records eight complete restored frames, then keeps a
  low-rate two-minute tail. A successful save during that tail refreshes its
  deadline. Stick movement beyond the diagnostic deadzone or any button change
  now traces the current and following `MAIN GAME` updates; a fresh A-button
  press additionally
  opens a 240-frame door watch. Changes in room, scene, streaming, resource, or
  archive state re-arm exact tracing for two updates. An armed trace continues
  through effect and room-actor passes, the fade controller, audio callbacks,
  draw, presenter, and loop tail so a delayed movement failure has an exact
  last completed call.
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
- Snapshot format 11, introduced by `0.3.22`, added GLMJ01's transient
  animated-model owner registry at `0x803C26C8-0x803C2D94`. The model update
  validates each active heap slot against its matching companion controller
  and retires a broken ownership pair. A second guard skips only an unsafe
  controller update, preserving a recoverable effect while preventing LM's
  null model-descriptor dereference.
- Snapshot format 12 adds the fixed door-visibility subsystem at
  `0x80399510-0x80399B30`, room/door visibility masks at
  `0x803C2E10-0x803C3030`, the complete room event/text interpreter and request
  at `0x803C7CA0-0x803C8428`, and the 128-entry room actor-pointer table at
  `0x803C8490-0x803C8690`. The actor count already lives in captured SBSS, so
  the count and table now rewind together.
- Snapshot format 13 adds the pointer-free active-event bitmap at
  `0x803C20C8-0x803C2138`, closing the split where saved heap event objects
  were paired with destination-room activation flags. Cross-room restores
  also discard completed DVD request payload pointers and restart LM's
  64-entry request ring while both original OS workers and queues remain live.
- Snapshot format 14, introduced by `0.3.25`, adds the fixed scene-effect
  controller state at `0x803CE0F0-0x803CEB00`. Its list sentinels, pointer
  vectors, and active counts now rewind with their gameplay-heap nodes; the
  following destructor and asynchronous queue records remain live.

Controls are D-pad Left to save and D-pad Right to load. Confirm a same-room
restore first. For the focused cross-room test, save outside the intended foyer
door, enter it and wait until Luigi is controllable, load back outside, then
touch that same door again. Report whether both the door animation and room
load complete. `0.3.25` may attempt this restore instead of returning `EPOCH`;
a successful load is evidence for this specific resource shape, not general
cross-room support. Any different room, floor, transition, or asynchronous
state is expected to refuse safely. This remains a crash-risk feasibility
test. Audio may remain silent after a save or load until game logic starts the
room sequence again.

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
build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.25.zip
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
The overlay must start with `LM STATE X0.3.25`; wait until `F`, `C`, `H`, and
`G` are `OK` and `ST` is at least 3. The trailing `X` byte reports the guarded
cross-room path: `X00` means it has not been attempted, `XA0` means it passed,
and `X01` through `X08` identify the refusal stage: epoch mask, saved-census
generation, volume census, list topology, archive ownership, room streamer,
model census, or model replacement shape. If `ST` remains zero, photograph the
short gate name and eight-digit value shown after `G:`; they identify the
rejected live condition without weakening it. `G:PTCL` specifically means the
JPA pool ownership or fixed-sentinel audit failed, so do not attempt a state.

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

After a same-room restore succeeds, save outside the intended foyer door,
enter it and wait until Luigi is controllable, load back outside, then touch
that same door again. Report separately whether the second door animation and
the following room load complete. If re-entry succeeds, keep walking normally;
the current target is the delayed failure that appeared only after a good room
rewind. `S:LOADED` means the guarded raw rewind completed. `EPOCH` means the
observed transition fell outside this
experiment's accepted shape; photograph the full diagnostic panel rather than
retrying through a different transition. If the game crashes, save the
newest `susamune_crash_a.txt` or `susamune_crash_b.txt` from the launcher's
storage device before the next experiment overwrites the older rotating report.
Whether it raises an exception, hard-locks, or reboots, return the SD card
before making another successful state and preserve `/ndebug.log`, both
`/lm_dumps/lm_attempt_a.bin` and `/lm_dumps/lm_attempt_b.bin`, and any fresh
`susamune_crash_a/b.txt` report. The two attempt files retain the latest two
successful-save generations, so another save may overwrite the older test.
Decode either file or the whole directory without modifying it:

```powershell
.\venv\Scripts\python.exe scripts\read_lm_dump.py D:\lm_dumps
```

Replace `D:` with the SD card's drive letter. The parser marks the latest valid
generation, prints every exact phase record, and reports a torn final record.
For roughly two minutes after a load or later save, movement or a button change
arms exact update tracing; a normal A press at a door also starts the four-second
transition watch.

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
