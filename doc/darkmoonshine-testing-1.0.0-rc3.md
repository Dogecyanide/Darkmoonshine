# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 3

Install the whole matching app in `Apps/moonshine_luigis_mansion`; place the
ZIP's separate `Darkmoonshine_Theme` folder on the SD root. Keep existing
custom theme files if desired. Keep settings and normal game saves, but make
fresh RC3 savestates. D-pad Down opens the menu; Left saves; Right loads.
SD Import fills memory; Load restores gameplay. A failed import must leave
the previous memory state intact.

## Ten focused tests

1. **Launcher and theme.** Boot from SD with no USB drive. Confirm the copied
   background/music work and startup no longer waits on an unused USB device.
   If using autoboot, verify it still launches and holding B still opens the
   menu. To test folder creation, temporarily rename `Darkmoonshine_Theme`,
   boot once and check that a new empty folder exists. Keep your original
   artwork safe; put it back afterward. Existing files must not be overwritten.
2. **Native menus: OFF.** In Displays → Sunshine timer, set **Run in native
   menus OFF**. Note the running time, spend about 10 seconds each in native
   START pause, Y map and Z Game Boy Horror, then exit. Time spent inside
   those menus should not count; normal gameplay should immediately count again.
   Optional overlays may hide while these menus are open.
3. **Native menus: ON.** Repeat with the option ON. Menu time should count,
   without running twice as fast on entry/exit. Open and close each several
   times. Compare about 10 seconds of ordinary gameplay before and afterward.
4. **Mod menu always counts.** With the option OFF, wait 10 seconds in the
   D-pad Down menu and another 10 in the timer editor. Time must advance with
   gameplay. Repeat with the option ON, and once with the timer hidden.
   Hiding the timer or opening the editor must not create untimed gameplay.
5. **Record/cancel/disable reset.** In Room Tools, Reset bind starts OFF. Press
   A, release it, record L+X using a full L click, then release both. Reopen the
   recorder and cancel with B alone: the previous binding should stay. Z on
   the row must clear it. Rebind your preferred 2–4-button combo; forbidden
   D-left/right/down or Start combinations must be rejected.
6. **Reset activation and guard.** Close the menu and try the exact combo in
   a stable room. It should reset once, including when held; release every
   button before retrying. Extra held buttons must prevent activation. Try
   once during a door transition: it should safely refuse, not crash or queue
   a surprise reset afterward. D-left/right and D-down must still work.
7. **After reset.** Reset Parlor, enter Anteroom normally, save, move and load.
   Repeat reset and load twice. Check actors/lights, doors, camera and timer;
   no stuck controls, unsolicited repeat resets or delayed crash.
8. **Persistence and old settings.** Change the menu-timer option and combo,
   close the menu, wait a few seconds and reboot. Both should persist, together
   with your existing timer layout, colours, display settings and game path.
   Do not delete the old INI before the first RC3 boot: migration is part of this test.
9. **Important warp/state chain.** Save Parlor, menu-warp Storage, load;
   menu-warp Boneyard, load; use the Anteroom door normally. Repeat twice.
   Keep your usual displays on and watch for renewed slowdown or flicker.
10. **Reusable SD state.** Name/export a fresh RC3 state marked REBOOT READY.
    Fully reboot, import/load it, walk through a door, then repeat after a
    second reboot using the same unchanged archive. Timer preferences must
    stay at your chosen values rather than rewinding with the game state.

## Evidence to send

Send the entire `lm_dumps` folder, any `luigis_mansion_crash_a/b.bin` and `.txt`,
and the affected named `.lms` archive plus name sidecars. Journals rotate
through eight files; collect promptly after a problem. Include test number,
exact route/actions, controller, displays and video mode. A clip helps with
timer pacing or flicker. Keep private keys on the SD, not in shared reports.

Secret Altar state-load refusals remain a known ownership limit. The timer
uses native game updates, not wall time; genuine scripted stops and its
36-counted-minute native rollover remain. The mod menu does not pause gameplay.
