# Moonshine Luigi's Mansion 0.3.43 — focused runner tests

Japanese LM, completed Hidden Mansion file. Install the complete matching .43
app folder. Snapshot format is now 26: make fresh .43 states and archives.
Keep earlier evidence, settings and archive_key0/1.bin; old-build states cannot
be loaded by this build.

D-pad Left saves, Right loads, Down opens the menu. Browser Import only fills
the memory slot; close the menu and press Right to restore gameplay.

Back up your `lm_states` folder before testing deletion. Deletion is permanent:
use disposable copies, not your only useful states. It does not clear the
resident memory slot. Never remove the SD while an operation is pending.

Record the exact route, action and refusal/crash text. Preserve the entire `lm_dumps` folder
after each of tests 1–4 and immediately after any failure, before trying again.
Send any `luigis_mansion_crash_a/b.bin` and `.txt` too. The eight rotating
attempt journals are diagnostic logs, not playable savestates.

1. **Fresh named cold-boot baseline.** Save in the Parlor and export a new
   named archive, such as `parlor43`. Note whether it says REBOOT READY.
   Fully power the Wii off/on, enter the same Hidden Mansion file, browser-import
   this original archive and load it. Move and use the Anteroom door normally.
   Keep this original archive unchanged for test 2.

2. **Repeat the previously failing reboot/Storage route.** Fully power off/on
   again. Import the same original `parlor43`, load it, move and use the
   Anteroom door normally, repeating test 1 before continuing. Then menu-warp
   to Storage Room. Load the Parlor state again; use a normal door, re-enter,
   and load again. This targets the crash from .42 test 4. If it fails, preserve
   the evidence immediately and report exactly which load or door action failed.

3. **Storage menu warp without a reboot.** Make a fresh Parlor memory state,
   menu-warp to Storage, load back to Parlor, and use/re-enter the Anteroom door.
   Repeat the warp → load → normal-door sequence three times with that same
   memory state. Check for a delayed crash, not just one correct loaded frame.

4. **Reverse direction.** Save in Storage Room, menu-warp to Parlor, then load
   the Storage state. Walk around, use a normal door and load again. Repeat once.
   Name the actual rooms used if the nearest available door changes the route.

5. **Fast positioning and saved preferences.** In Displays → Sunshine timer →
   Edit, tap the D-pad for fine movement, then hold it to move across the screen.
   Release and reverse: movement must stop immediately and restart precisely.
   Repeat for Edit streak. Keep a position with A then A; make another change
   and discard with B then A. Close the main menu and wait for
   SETTINGS: SAVED TO SD. Reboot and check that only the kept positions return.

6. **Delete cancellation and frozen target.** Export two disposable archives,
   named `DELETE TEST A` and `DELETE TEST B`. Select A in the SD browser and
   press Z. Check its displayed name and ID; Up/Down must not change the target
   while confirmation is open. Press B to cancel. Refresh: both archives must
   remain intact, and neither should have been deleted.

7. **Confirmed deletion, other archive and memory preserved.** Import disposable
   A into memory first. Select A in the browser, press Z, then make a fresh A
   press to confirm permanent deletion. Its entry must disappear after refresh,
   while B remains. Close the menu and load the resident memory state: deleting
   its SD file must not have cleared the copy already in memory.

8. **Second deletion and continued SD use.** Delete disposable B, refresh, then
   export, import and load a new named archive. Reboot and check the new archive
   remains usable. Optionally delete an old-build archive only if it is backed
   up and you actually want it gone; incompatibility must not prevent deletion.

A stale catalog or busy SD operation should refuse deletion safely: refresh
and retry only after checking the displayed target. If you see
`DELETED; NAME CLEANUP FAILED`, the state file is already deleted but
name cleanup failed. Preserve the files/logs; there is no automatic undelete.
A leftover name file reserves its archive ID against accidental reuse.

The .42 Wii tests already passed named soft/cold-reboot loads, timer visuals
and persistent preferences. These eight tests focus on .43's remaining crash
candidate and new controls; they are not a claim that .43 is already accepted.
SD archives, deletion and persistent preferences require Wii, not Dolphin.
The native timer's existing 36-counted-minute rollover remains unchanged.
