# Moonshine Luigi's Mansion 0.3.39 - runner test checklist

Six focused Wii tests, **one attempt each**. This candidate is **not yet
Wii-verified**. We are checking the next transition after a savestate restore,
not just whether the saved position appears.

## Before starting

- Back up your memory-card save and use your completed Japanese Hidden Mansion
  practice file. Install the whole matching app folder and confirm **0.3.39**.
- Start test 1 from a fresh game boot, **before making or loading a mod state**.
  For tests 2–6, make a fresh state for each row. Format **24** is incompatible
  with previous formats. Do not import an older archive or restore an older
  Dolphin checkpoint. All saves and loads in these tests stay in one session.
- D-pad **Left = save**, **Right = load**, **Down = menu**. “Menu-warp” means
  choosing a destination in **Room Warps**, not loading a savestate. Wait until
  Luigi is controllable after each transition before the next action.
- After each successful restore, move for **20–30 seconds**, then perform the
  transition specified in that row. Do not add extra doors beforehand. Check
  camera, controls, graphics and audio; do not save or load during a door
  animation.
- **Stop at the first crash.** Keep the SD inserted while running. After safely
  powering down, preserve the logs before another session overwrites them.

## The six tests

| # | Setup and sequence | Result / short note |
|---|---|---|
| 1 | **No-state control:** fresh boot → reach Parlor → menu-warp to Anteroom. Do not save or load a mod state first. Move around after the warp. | |
| 2 | **Same-room control:** save at a clear spot in the bottom Foyer → walk a few steps within that room → load → move around. | |
| 3 | **Main reproduction:** save in Parlor → menu-warp to Storage Room → load back to Parlor → move → menu-warp to Anteroom. The final warp is essential. | |
| 4 | **Reverse direction, if 3 passes:** save in Storage Room → menu-warp to Parlor → load back to Storage Room → move → menu-warp to Anteroom. | |
| 5 | **Same-room reload, then leave:** save in Parlor → menu-warp to Parlor → load → move → menu-warp to Anteroom. | |
| 6 | **Normal-door control:** save in Parlor → enter Anteroom through the ordinary door → load back to Parlor → move → use that ordinary door again. No menu warp during this sequence. | |

Use **PASS / REFUSED / CRASH / WRONG RESULT / NOT TESTED**. PASS means the
whole row finishes and normal control returns. A safe refusal is not a crash:
record its exact status/G/X codes. If test 3 is refused, mark 4 NOT TESTED.
Continue to other rows only if normal play is intact. No repeats or long
endurance session are required.

## Send back

Send the short result table, Wii/controller model, and screenshots or clips of
unexpected behavior. Identify the exact failed step: state load, movement,
opening the menu, final menu warp, or ordinary door contact. Note whether the
destination appeared and which room was visible when the game stopped.

Please include these SD files when present:

- `lm_dumps/lm_attempt_a.bin` and `lm_dumps/lm_attempt_b.bin`
- `luigis_mansion_crash_a.bin` and `luigis_mansion_crash_a.txt` at the SD root
- `luigis_mansion_crash_b.bin` and `luigis_mansion_crash_b.txt` at the SD root

Hard freezes may not create a new exception report, and crash files can be
older than the latest test. Send them as found with both attempt journals.
**No SD import/export, reset portability, settings, Dojo, rush-mode or
boss-arena tests are required this time.**
