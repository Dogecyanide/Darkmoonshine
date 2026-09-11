# DarkMoonshine — V1.0.0 Frozen in Time

Install the whole matching app in `Apps/moonshine_luigis_mansion`; place the
ZIP's separate `Darkmoonshine_Theme` folder on the SD root. Keep existing
custom theme files, settings and normal game saves. Make **fresh V1.0.0 states**:
earlier builds' archives remain backups, not compatible imports.
D-pad Down opens the menu; Left saves; Right loads.
SD Import fills memory; Load restores gameplay. A failed import must leave
the previous memory state intact.

The maintainer reported all ten RC4 tests passed on Wii. The final release
changes branding and packaging only; this checklist remains available for
new setups and release smoke checks.

## Ten focused tests

1. **Save/load pause.** In the foyer, save, move and load five times. Note how
   long Saving and Loading freeze gameplay; a short recording is ideal.
   Compare with RC3 only if convenient. Check Luigi, camera, health and timer
   rewind together. PC speed measurements are not a promised Wii multiplier.
2. **Repeated overwrites.** Make and load 10–20 states in one room, changing
   position between saves. Each load must restore the latest save. Walk through
   a normal door afterward; watch for delayed crashes or stuck rumble.
3. **Busy room.** Save and load in a dark room with several ghosts/effects,
   once calm enough to accept a state. Continue playing afterward. A capacity
   refusal must preserve the prior state; a transition refusal is not a crash.
4. **Different floor.** Save on one floor, warp to another and load. Repeat in
   the other direction. Use a normal door and load again after each return.
   Report exact room names and whether refusal or crashing occurs.
5. **Fragile route.** Save Parlor, menu-warp Storage, load; menu-warp Boneyard,
   load; enter Anteroom normally. Repeat twice. Check camera, doors, actors
   and controls after loading, not just the first restored frame.
6. **Door guard.** Keep a valid state, try saving/loading during a normal door
   transition, then retry once settled. The transition should safely refuse.
   It must not corrupt the old state or perform a surprise delayed action.
7. **Named SD export/import.** Name/export a fresh V1.0.0 state A. Save a different
   position B in memory, import A and load. It must restore A completely.
   Export another room and switch between the two named archives twice.
8. **Failed import preserves memory.** With a working V1.0.0 state in memory,
   try importing an old-build archive if one is available. It should refuse
   without replacing the RAM state. Close the browser and load the RAM state.
   Do not edit archive files or keys to force an import.
9. **Reusable reboot state.** Export a fresh V1.0.0 state marked REBOOT READY.
   Soft reboot, import/load it and use a door. Fully restart the Wii, import
   and load the same unchanged archive, then use a door again. Report whether
   failure was during Import, Load, or subsequent gameplay.
10. **Short practice session.** Play for 10–15 minutes with your usual timer
    and input displays, occasional saves/loads, room resets and menu warps.
    Confirm your saved timer layout, native-menu timer option, reset combo
    and launcher theme remain correct. Collect logs promptly after any fault.

## Evidence to send

Send the entire `lm_dumps` folder, any `luigis_mansion_crash_a/b.bin` and `.txt`,
and the affected named `.lms` archive plus name sidecars. Journals rotate
through eight files; collect promptly after a problem. Include test number,
exact route/actions, controller, displays and video mode. A clip helps with
pause duration or flicker. Keep private keys on the SD, not in shared reports.

V1.0.0 retains one RAM slot plus named SD archives. The second complete state
did not fit safely in the current layout. Secret Altar state-load refusals
remain a known ownership limit. The timer uses native game updates, not wall
time; genuine scripted stops and its 36-counted-minute rollover remain.
The mod menu does not pause gameplay.
