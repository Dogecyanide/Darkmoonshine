# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 2

Focused Wii retest: timer correctness/performance first, then a short state regression.
Install the whole matching app. Keep existing settings and normal game saves.
Make fresh RC2 states: RC1 archives remain intact but have a different build identity.
D-pad Down opens the menu; Left saves; Right loads. SD Import fills the memory
slot; Load actually restores gameplay. A failed import must not erase that slot.

## Ten tests

1. **Critical: menu cannot save time.** In an ordinary room with a running timer,
   note the time, open the mod menu for about 10 seconds, then close it. The
   timer must have advanced with gameplay. Try a room with a moving ghost too:
   enemies must not gain untimed progress. Repeat opening/closing five times.
2. **Editor and visibility loopholes.** Spend about 10 seconds in the timer's
   Creation editor, then leave. Repeat with the timer hidden in Displays.
   Showing it again must reveal the time that passed, not a stopped timer.
3. **Timer-only lag A/B.** With other displays off, compare the same movement
   in Armory (or another busy room) with timer off versus on. Check the default
   size, then your usual custom size. Report visible slowdown and any lag-count
   difference; a short matched recording is especially useful.
4. **Combined displays and flicker.** Enable timer, input, metadata, lag counter
   and R-pump. Move around; open/close native pause and the Z/Game Boy Horror
   screen, then the mod menu. Optional gameplay displays should hide in the
   native single-buffer menu and return on exit. Check flicker at both top and
   bottom. The mod menu and action popup remain separate. Repeat with
   timer off to identify whether it is timer-specific. Report video mode too
   (progressive/interlaced), and send a clip if it still flickers.
5. **Cached artwork stays correct.** Let seconds and minutes roll over; load an
   earlier time. Toggle TIME, edit different characters' colours, streak,
   opacity and brightness, and resize/reposition near screen edges. Confirm
   changes appear immediately, digits never stick, and Keep/Discard works.
6. **Element feedback.** In Foyer and Sitting Room, try filling fire/water/ice
   while idle with their medals, then during suction/spray. If rejected, record
   the exact new reason. Try again after releasing triggers and after an event
   ends. Repeat in the hallway; verify a successful fill can be used normally.
7. **Same-room timer rewind.** Save, wait/move, then load five times. Position,
   health and timer should rewind. Time must advance again afterward; a save
   made through the menu must not create a permanently stopped timer.
8. **Previously fragile warp chain.** Save Parlor, menu-warp Storage, load;
   menu-warp Boneyard, load; use the Anteroom door normally. Repeat the chain
   twice, then take a new state and load it. No crash or stuck door/camera.
9. **RC2 SD reboot reuse and preferences.** Name/export a fresh state. Fully
   reboot the Wii, import/load it, use a door, then repeat after another reboot.
   Timer/display preferences should persist independently of the state. Keep
   that original archive unchanged for both boots.
10. **Short combined session.** Practice for 10–15 minutes with your preferred
    displays and repeated room/floor warps, SD imports and loads. Once, try a
    load during a door transition: a safe refusal must leave the old state
    usable afterward. Report slowdown, flicker, or any other regression.

## Evidence to send

After a failure or the end of the session, copy the entire `lm_dumps` folder,
any `luigis_mansion_crash_a/b.bin` and `.txt`, and the affected named `.lms`
archive plus its name sidecars. Journals rotate through eight files; collect
them promptly after a problem. Include test number, exact route/action, which
displays were enabled, video mode and a screenshot/clip when possible.
Keep your private key on your SD; do not publish it with reports.

Secret Altar state-load refusals remain a known ownership limit. This build
does not weaken that guard. The timer follows native 30 Hz game-update time,
not wall time; genuine script stops remain and its native rollover is 36
counted minutes. The mod menu does not pause the game or stop this clock.
Rendering performance/flicker still needs real-Wii acceptance; host tests are
not a hardware pass. No new Dojo/rush or trick-success detector is included.
