# Moonshine Luigi's Mansion 0.3.38 - runner test checklist

Six focused Wii tests, **one attempt each**. This candidate is **not yet
Wii-verified**. We are checking whether a restored scene can be cleaned up
safely by the next warp or ordinary door transition.

## Before starting

- Back up your memory-card save and use your completed Japanese Hidden Mansion
  practice file. Install the whole matching app folder and confirm **0.3.38**.
- Start test 1 from a fresh game boot, **before making or loading a mod state**.
  For tests 2–6, make a fresh state for each row. Format **23** needs new states;
  do not import an older archive or restore an older Dolphin checkpoint.
- D-pad **Left = save**, **Right = load**, **Down = menu**. “Menu-warp” means
  selecting a destination in **Room Warps**, not loading a savestate. Wait until
  Luigi is controllable after each transition before taking the next action.
- After a successful restore, move for **20–30 seconds**, then do the next
  transition specified in that row. Do not add extra doors before that step:
  we want to identify which transition fails. Check camera, controls, graphics
  and audio. Do not save or load during a door animation.
- **Stop at the first crash.** Keep the SD inserted while running; after safely
  powering down, preserve the logs before another session overwrites them.

## The six tests

| # | Setup and sequence | Result / short note |
|---|---|---|
| 1 | **No-state control:** fresh boot → reach Parlor → menu-warp to Anteroom. Do not save or load a mod state first. Move around once the warp finishes. | |
| 2 | **Same-room control:** save at a clear spot in the bottom Foyer → walk a few steps within that room → load → move around. | |
| 3 | **Exact restore-then-warp route:** save in Parlor → menu-warp to Storage Room → load back to Parlor → move → menu-warp to Anteroom. The final warp is the important step. | |
| 4 | **Reverse restore direction, if 3 passes:** save in Storage Room → menu-warp to Parlor → load back to Storage Room → move → menu-warp to Anteroom. | |
| 5 | **Same-room reload, then leave:** save in Parlor → menu-warp to Parlor → load → move → menu-warp to Anteroom. | |
| 6 | **Normal-door control:** save in Parlor → enter Anteroom through the ordinary door → load back to Parlor → move → use that ordinary door again. No menu warp during this sequence. | |

Use **PASS / REFUSED / CRASH / WRONG RESULT / NOT TESTED**. A row passes only
if its entire sequence finishes and normal control returns. A safe refusal is
not a crash: record its exact status/G/X codes and continue only if normal
play is intact. If test 3 is refused, mark test 4 NOT TESTED and proceed to 5
only if the game is still behaving normally. No repeats or long endurance
session are required.

## Send back

Send the short result table, Wii/controller model, and screenshots or clips of
unexpected behavior. For a crash, identify the exact step: state load, movement,
opening the menu, final menu warp, or normal door contact. Note whether the
destination ever appeared and which room was still visible when it stopped.

Please include these SD files when present:

- `lm_dumps/lm_attempt_a.bin` and `lm_dumps/lm_attempt_b.bin`
- `luigis_mansion_crash_a.bin` and `luigis_mansion_crash_a.txt` at the SD root
- `luigis_mansion_crash_b.bin` and `luigis_mansion_crash_b.txt` at the SD root

Hard freezes may not produce a new exception report. Crash files may also be
older: send them as found along with both attempt journals. **No SD import or
export, settings, Dojo, rush-mode or boss-arena tests are required this time.**
