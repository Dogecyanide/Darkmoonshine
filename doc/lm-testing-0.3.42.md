# Moonshine Luigi's Mansion 0.3.42 — focused runner tests

Japanese LM, completed Hidden Mansion file. Install the whole .42 app folder.
Keep old states, names, settings and archive_key0/1.bin. Use fresh .42 archives
for these tests; another build's archives are not compatible.

D-pad Left saves, Right loads, Down opens the menu. Browse SD → Import fills
the memory slot; close the menu and press Right to restore. You do NOT need
Import manual ID after a browser import.

Record the route/action and exact refusal text. Preserve the entire `lm_dumps` folder
after a crash/refusal and after each group below. Eight rotating
lm_attempt_a through h files are logs, not playable saves. Also send any
luigis_mansion_crash_a/b.bin and .txt. Back up lm_states, including names and
keys. Stop forcing a rejected state; preserve the evidence first.

## Reboot and state reliability — priority

1. Save Parlor and export a named archive, e.g. parlor42. Note REBOOT READY or
   THIS BOOT ONLY. Walk to Anteroom, browser-import the archive and load; then
   use the door normally. Keep this original unchanged for tests 2–4.
2. Soft-reset, enter the same Hidden Mansion file, import that original archive
   and load. If accepted, move, use the Anteroom door, and load again.
3. Fully power the Wii off/on. Import the same original archive and repeat the
   load, movement, door and second load. Capture HUD/logs on any refusal.
4. If test 3 passed, repeat after another full reboot. Then menu-warp Storage,
   load the original Parlor state again and exit through a normal door.
5. Save Storage, naturally travel to a different floor and load back; use a
   door. Repeat the reverse with a new state. Record actual rooms/floors.
6. Save Parlor → menu-warp Anteroom → load Parlor → normal Anteroom door.
   Repeat using a Storage menu warp. Check movement and a second load.

## Sunshine timer / Creation

7. Displays → Sunshine timer → Visible ON. Check the slanted/rising layout,
   TIME label and streak. Toggle TIME and Visible independently; screenshot
   distortion or bad overlaps.
8. Edit: D-pad moves; L/R resize; C-stick up/down picks an option, left/right
   adjusts it. START selects a character; X+START goes back. Recolour one digit
   and TIME separately. Check their neighbours and selected-character marker.
9. Try opacity, brightness, background opacity and padding (enable padding to
   show the background). A then A keeps. Reopen, change something, B then A
   discards. Z then A resets only the selected option. Editor D-pad presses
   must not save/load gameplay.
10. Edit streak: change RGB, position, size and opacity independently. Confirm
    the digits are unchanged. Zero streak opacity should hide just the streak.
11. Check the timer pauses with the mod menu open and resumes when closed.
    Save, wait, load: time must rewind and continue. Visibility OFF/ON must
    not reset time. Natural doors retain time; menu map reloads restart it.
12. Capture a Boo (clock stops), then trigger the next Boo introduction
    (resumes). Test a boss intro/finish if convenient. Report exact endpoints
    that differ from GaddWarp; cemetery/custom Dojo/rush timing is not complete.
13. Compare a busy scene with the timer visible versus hidden using the lag
    display. Report noticeable slowdown and slow-update counts. Screenshot
    any BAD memory indicator.

## Persistent preferences

14. Set an obvious timer position/size, one digit colour and streak colour;
    change TIME visibility, input/metadata/R-pump displays and Luigi colour.
    Close the main mod menu, reopen and wait for `SETTINGS: SAVED TO SD`.
    Record a timing reference; after release finishes recording, it should
    autosave too. Soft-reset: confirm preferences and reference return,
    including disabled toggles.
15. Fully power off/on and verify the same preferences. Load an older state
    created earlier in this .42 session/build: your CURRENT preferences must
    remain, not revert to the state-time settings.
16. Change an editor value then discard it. Close/reopen and reboot: discarded
    changes must not appear. Change one setting again, close, verify
    `SETTINGS: SAVED TO SD` and reboot once more. Confirm the launcher's game
    path/settings are still intact.

Dolphin currently has no persistent-preferences backend; do tests 14–16 on Wii.
Preferences use `/moonshine_lm.ini`, section `[lm_preferences]`. Its `.lm.tmp`
and `.lm.bak` companions support recoverable replacement without dropping the
launcher settings. Leave them alone during a write; wait for the success
message before powering off.
Health, money, room flags and progress are game state, not stored preferences.
The timer uses the native 30-update clock, including its 36-minute rollover;
it is not a full-run timer. No need to repeat all 25 old tests unless a new
failure calls for it.
