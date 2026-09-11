# Moonshine Luigi's Mansion 0.3.37 - runner test checklist

Six focused Wii tests, not a full feature survey. This candidate is **not yet
Wii-verified**. We are checking post-menu-warp loads and whether you can keep
playing afterward. A restored first frame is not a complete pass.

## Before starting

- Back up your memory-card save; use your completed Japanese Hidden Mansion
  practice file. Install the whole matching app folder and confirm **0.3.37**.
- Make a **fresh mod savestate for every row below**. Format 22 is not compatible
  with old states. Wait until Luigi is controllable before saving or loading.
- D-pad **Left = save**, **Right = load**, **Down = menu**. Select destinations
  in **Room Warps** for each step marked “menu-warp.” A menu warp reloads the
  scene; it is not a savestate load.
- After every successful load, move around for **20–30 seconds**, then enter
  and leave an ordinary door. Check position, camera, controls, graphics and
  audio. Do not load while already passing through the door.
- **Stop at the first crash.** Keep the SD inserted while running; after safely
  powering down, preserve the logs before another session overwrites them.

## The six tests

| # | Fresh-state setup and sequence | Result / short note |
|---|---|---|
| 1 | **Same-room Foyer:** save at a clear spot, walk elsewhere within that room, load. | |
| 2 | **Same-room menu reload:** save in Parlor → menu-warp to Parlor → wait → load. | |
| 3 | **Anteroom route:** save in Parlor → menu-warp to Anteroom → wait → load back to Parlor. | |
| 4 | **Storage route:** save in Parlor → menu-warp to Storage Room → wait → load back to Parlor. | |
| 5 | **Reverse Storage route:** save in Storage Room → menu-warp to Parlor → wait → load back to Storage Room. | |
| 6 | **State made after a warp:** first menu-warp into a room and wait; save there → menu-warp elsewhere → wait → load. Record both rooms. | |

For each row, use **PASS / REFUSED / CRASH / WRONG RESULT / NOT TESTED**.
One attempt per row is enough for this pass. If a route works and you want to
check repeatability, repeat it up to three times using that row's fresh state;
there is no required long endurance session. A safe refusal is not a crash:
record its exact status/G/X codes and continue only if normal play is intact.

## Send back

Your short result table, Wii/controller model, and any screenshot or clip of
EPOCH, broken graphics or unexpected behavior. For a crash, say whether it
happened during loading, on the first restored frame, during movement, or on
door contact—and which door.

Please include these SD files when present:

- `lm_dumps/lm_attempt_a.bin` and `lm_dumps/lm_attempt_b.bin`
- `luigis_mansion_crash_a.bin` and `luigis_mansion_crash_a.txt`
- `luigis_mansion_crash_b.bin` and `luigis_mansion_crash_b.txt`

Hard freezes may not produce a new exception report, so the attempt journals
are still important. The crash files may be older: send them as found rather
than assuming every file belongs to this run. **No SD import/export, settings,
Dojo or rush-mode tests are required for this build.**
