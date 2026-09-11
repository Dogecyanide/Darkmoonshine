# Moonshine Luigi's Mansion 0.3.40 - runner test checklist

Six focused tests, **one attempt each**. The reported 0.3.39 tests passed; this
round checks SD naming, one restore regression, and a controlled reboot data
pair. **0.3.40 still needs Wii verification. Cross-boot state loading is not
supported; names should persist, but old-session states must remain refused.**

## Before starting

- Back up the memory-card save and the existing `lm_states` folder, including
  its `.name0` / `.name1` sidecars. Use the completed Japanese Hidden Mansion
  practice file. Install the **matching 0.3.40 launcher and payload**, confirm
  the version, and make fresh 0.3.40 states. Do not import an older build.
- D-pad **Left = save**, **Right = load**, **Down = menu**. “Menu-warp” means
  selecting a destination in **Room Warps**, not loading a state. Save/load only
  once Luigi is controllable, never during a door animation.
- In the name editor: **stick or D-pad = select**, **A = type**, **B = backspace**,
  **X = space**, **Y = upper/lower case**, **L/R = page**, **Z then A = clear**.
  **START then A = keep**; **hold X and press START, then A = discard**.
  **B cancels a confirmation prompt** and returns to editing.
- In the SD browser: **A = import**, **X = rename**, **Y = refresh**, **B = back**.
  Import fills the memory slot; use Load/D-pad Right separately to restore it.
- Wait for export/rename completion before leaving or removing the SD.
  **Stop at the first crash** and preserve logs after safely powering down.
  A safe refusal is not a crash; record the message and continue only if play
  remains normal.

## The six tests

| # | Setup and sequence | Result / short note |
|---|---|---|
| 1 | **Named export and browser:** save in a safe spot, export with a recognizable name, and note its archive ID. Close and reopen the SD browser. Check that the name is correct and the editor/browser text is legible and not cut off. | |
| 2 | **Rename and discard:** rename that test archive and keep the new name. Reopen it, change the draft, then discard. The kept name must remain unchanged after refreshing/reopening. Try B on a confirmation prompt: it should return to editing, not commit. | |
| 3 | **Length limit and blank fallback:** rename the test archive using 31 characters, then try adding a 32nd; it must remain at 31 without broken text. Keep it. Then clear the name and keep the blank name: the browser should show the archive ID, and the state must still exist. | |
| 4 | **Same-session restore regression:** import that archive, load it, move for 20–30 seconds and use a normal door. Then make a fresh state in Parlor → menu-warp to Storage Room → load back to Parlor → move briefly → menu-warp to Anteroom. Check the final warp as well as the restored frame. | |
| 5 | **BOOT A capture:** fully reboot the Wii. Enter the usual Hidden Mansion file and stay at the initial bottom-Foyer position; no menu warps or state loads first. Wait about five seconds once controllable, save, then export named **BOOT A**. Note its actual archive ID and wait for completion. Preserve this session's journals before test 6 overwrites them. | |
| 6 | **BOOT B capture:** fully reboot the Wii again with identical setup. Repeat the same file, initial Foyer position and five-second wait, with no warps/loads; save and export named **BOOT B**. Check that **BOOT A's name** still appears in the browser. **Do not import or load BOOT A**: its different-session status is expected. | |

Use **PASS / REFUSED / CRASH / WRONG RESULT / NOT TESTED**. For test 4, note
the same-session import/load result and the final Parlor-to-Anteroom warp
separately in the same row. No repeats or long endurance session are required.

For the BOOT A/B pair, keep launcher/video settings, progressive-mode choice,
controller and memory-card file unchanged. Do not overwrite the normal game
save between the two boots. Do not substitute a soft reset or a Dolphin
checkpoint for the full reboot. The pair is for **offline comparison**, not a
claim that rebooted savestates can already be loaded.

## Send back

Send the short result table, Wii/controller model, screenshots of any naming
or layout problem, and the exact step of any crash/refusal.

- The two newly created BOOT A and BOOT B `lm_states/archive_########.lms`
  files, clearly identified by their actual IDs.
- Their matching `archive_########.name0` and `archive_########.name1` sidecars
  from `lm_states`, when present. Keep both copies; do not rename or edit them.
  Including the naming-test archive's sidecars also helps if tests 1–3 fail.
- Each BOOT A/B session's `lm_dumps/lm_attempt_a.bin` and
  `lm_dumps/lm_attempt_b.bin`, ideally copied into separate before/after folders.
- If a crash occurs, the SD-root `luigis_mansion_crash_a.bin`,
  `luigis_mansion_crash_a.txt`, `luigis_mansion_crash_b.bin` and
  `luigis_mansion_crash_b.txt`, when present.

Hard freezes may not create a new report, and existing crash files may be
older. Send them as found rather than assuming they belong to this test.
**No Dojo, rush-mode, boss-arena or general settings-persistence testing is
needed this round.**
