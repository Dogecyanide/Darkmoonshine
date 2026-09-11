# Moonshine Luigi's Mansion 0.3.41 — focused runner tests

**25 tests for the final build of the day.** Priorities are reusable SD states,
room/floor coverage, and surviving gameplay after a load—not just displaying
one restored frame. Split tests between runners if useful; report test numbers.
Mark **PASS / REFUSED / CRASH / WRONG RESULT / BLOCKED / NOT TESTED**.

## Setup and common checks

- Back up the normal memory-card save and the entire `lm_states` folder.
  Install matching .41 launcher/payload and confirm **X0.3.41** on screen.
  Use fresh .41 states; older builds' archives are intentionally incompatible.
- Use the completed Japanese Hidden Mansion practice file. Keep launcher/video
  settings, progressive-mode choice, controller port and normal game save
  unchanged across the reboot tests. Cheats/debugging should be off.
- **D-pad Left = save, Right = load, Down = menu.** “Menu warp” means selecting
  a destination in Room Warps; it is not a state load. Normally wait until Luigi
  is controllable and any transition/rumble has settled before saving.
- Export gives a name to an SD archive. Wait for completion. **EXPORTED: REBOOT
  READY** and browser label **REBOOT ARCHIVE** identify a reboot-format file.
  If it says THIS BOOT ONLY, photograph it and mark reboot tests blocked; the
  same-session tests are still useful. Don't repeatedly reboot an ineligible file.
- **Import only fills the memory slot.** Close the menu and use Right/Load to
  actually restore gameplay. Never remove the SD while a storage operation runs.
- After an accepted load, check position, camera tracking, room objects and HUD,
  then move for roughly 20 seconds and use an ordinary door where available.
  Record delayed crashes too. A restored frame alone is not a pass.
- For health, money and tank tests, disable any practice option that forces or
  refills the value being measured. Don't write test progress to the normal
  memory-card save. Runtime display/colour preferences remain session-local.
- If a setup isn't available on your file, mark NOT TESTED and choose a different
  ordinary mansion room where appropriate. Do not force known unsupported boss,
  Dojo or rush-mode boundaries to complete this checklist.

## A. SD archives and restarting — highest priority

1. **Named export and first reuse.** Save in Parlor, export with a recognizable
   name and note the archive ID. Leave by a normal door, import the file and load
   back. Check gameplay and use the door again. Keep this exact archive for 3–5.
2. **Import with an empty memory slot.** Clear the resident slot using the menu
   (not the SD file), confirm it is empty, then import the archive from test 1.
   Load it and use a door. The named SD file must remain available afterward.
3. **Soft game reset.** Soft-reset, re-enter the same practice file, import the
   archive from test 1 and load it. Check the camera and door cleanup, then load
   again without replacing the saved state. Report import and load separately.
4. **Full Wii reboot.** Fully power off/restart the Wii with the same setup.
   Enter gameplay, import that same archive and load. Move, use a door and return,
   then load again. Neither exporting a replacement nor merely seeing the name
   in the browser counts as passing this test.
5. **Reuse after another full reboot.** Restart the Wii a second time and reuse
   the original archive again, without resaving/re-exporting it. If possible,
   import/load while standing in a different ordinary mansion room this time.
   This checks that the file isn't effectively single-use.

**Preserve this group's logs before doing more saves.**

## B. Natural routes, distance and camera coverage

6. **Foyer stairs.** Save at the bottom of the Foyer, walk to the top and load.
   The camera must return with Luigi and keep following. Repeat once in the
   opposite direction with a new state saved at the top.
7. **Natural Parlor → Anteroom route.** Save in Parlor, enter Anteroom normally,
   load back to Parlor, then enter Anteroom again. Check the first door animation
   and what happens after it finishes.
8. **Reverse natural route.** Save in Anteroom, leave normally for Parlor, load
   back inside, then leave again. This exercises the opposite cleanup direction.
9. **Several rooms away.** Save in a runner-chosen room, walk at least 3–5 rooms
   away without menu warps, then load. Record the actual room sequence rather
   than only “long distance.” Re-enter a neighboring room after the load.
10. **Natural cross-floor route.** Save on one floor, use normal stairs/doors to
    reach another floor, then load. Make a new state on the other floor and
    reverse the route once. Record both saved/current rooms and floors.

## C. Menu warps and post-load cleanup

11. **Warp to the same room.** Save, use Room Warps to reload that very room,
    then load the pre-warp state. Move and use a door. A menu warp to the same
    room must not silently make the state unusable.
12. **Parlor → Anteroom menu warp.** Save in Parlor, menu-warp to Anteroom, load
    Parlor, then enter Anteroom normally. Note whether failure happens on load,
    on the first door frame, or after entering.
13. **Parlor → Storage Room.** Save in Parlor, menu-warp to Storage Room and load
    back. Walk around, use a door, then load once more. This targets the old
    “returned correctly, then crashed a few frames later” sequence.
14. **Reverse return route and reboot.** Save/export in Storage Room, menu-warp to
    Foyer, import/load Storage and use a door. After a full Wii reboot, try the
    same archive from Foyer again. Report same-session and reboot results, and
    note the actual floors; room names alone do not establish a floor change.
15. **Warp after restoring.** Save in Parlor, menu-warp elsewhere and load back.
    Now menu-warp Parlor → Anteroom, then load the original state again.
    Finish with a natural door crossing. This checks teardown of a restored room.

**Copy the logs after B and after C; the eight-file ring is not a whole-day history.**

## D. Meaningful gameplay rewind

16. **Ghost capture replay.** In an ordinary mansion encounter, save while
    controllable before catching a ghost. Catch it, load, and try the capture
    again. Check ghost presence/health, animation, collision and room-clear state.
    Record a save refusal instead of repeatedly pressing the button.
17. **Player damage.** Save, take normal damage, then load; do not use health
    setters or other health-changing practice actions during this comparison.
    Check the saved health, position and normal control. Take another hit after
    loading to expose a stuck damage/invulnerability state.
18. **Money or pearl pickup.** Save before collecting an available pickup,
    collect it, load, then collect it again. The pickup and displayed totals
    should return together, without an invisible or uncollectable leftover.
19. **Element and tank state.** Save with an available element/tank amount, use
    some of it or change it through normal gameplay, then load. Check the HUD
    and actual Poltergust behavior agree. Test a shot after loading; record which
    element was used. Disable tank-forcing options for this measurement.
20. **Room event / lights-on replay.** Save before an available ordinary room
    puzzle or final ghost that changes the room state. Complete it, load, and
    attempt it again. Watch lights, doors, rewards, objects and any intro/event
    appearing early or triggering twice. Use a suitable room on your file.

## E. Refusal safety, archive switching and a longer session

21. **Load during a door animation.** With a good state already saved, press
    Right once during a normal door transition. Expect a clean refusal, not a
    mid-transition restore. Once controllable, load normally and check that the
    old state still works. Don't spam inputs or force a refused operation.
22. **Save during a transition.** Keep a known good state, then press Left once
    during a door animation—or an ordinary non-boss cutscene if one is available.
    Expect refusal without replacing the old state. Afterward, load normally and
    verify it returns to the original saved point. Note which transition you used.
23. **Switch between two SD archives.** Export named A and B in different rooms
    (preferably different floors). Alternate import → load A/B for three rounds.
    Confirm the selected name/ID loads the right state, and move/use a door after
    each load. There is one resident slot; these are two files on SD.
24. **Room reset interaction.** In a room where the practice Room Reset option
    is supported and ready, save, perform the reset, wait until controllable,
    then load the pre-reset state. Test the room's interaction and leave normally.
    If the tool is unavailable/busy, report that rather than forcing it.
25. **Mixed practice session.** Spend 10–15 minutes practicing a short ordinary
    mansion route. Mix new saves, repeated loads, natural doors, a few menu warps
    and an existing SD archive. Leave memory diagnostics enabled if practical;
    record the lowest GAME/SYS **M** readings and screenshot any BAD indicator.
    Look for delayed crashes, worsening pauses or newly refused states—not only
    immediate failures. List the main rooms and approximate load count.

## Handling a failure without losing the evidence

A safe refusal is not a crash. Record the exact message and whether gameplay
remains normal. Mark dependent tests BLOCKED if their prerequisite fails; don't
repeat the same known failure just to fill another row.

**On a crash, stop the current run and preserve its data before further testing.**
After the logs are copied, you may fresh-boot and continue unrelated tests.
Do not overwrite the normal memory-card save or edit archive/key files.

Copy the **entire `lm_dumps` folder** after each group of five tests and after an
unexpected refusal/crash, using separate folders labelled with runner/test range.
There are only eight rotating attempts (`lm_attempt_a.bin` through
`lm_attempt_h.bin`), not eight savestates or a complete session recording.
Successful saves rotate logs; the first load after boot can also start one.
If you cannot copy between groups, prioritize preserving the failure before
making further saves, and tell us earlier logs may have been overwritten.

## Send back

- Short numbered results: e.g. **12 PASS** or **14 REFUSED after full reboot**.
  Include build, Wii/controller type, saved/current room and floor, natural door
  versus menu warp, and soft reset versus full reboot.
- For failures, say exactly when: **import / load / first frame / movement /
  door / menu warp**. Screenshots or a short clip are especially useful for
  wrong camera, early cutscenes, garbled graphics or refusal messages.
- Each preserved `lm_dumps` folder, plus tested
  `lm_states/archive_########.lms` files and their matching `.name0`/`.name1`
  files. Keep the whole `lm_states` folder backed up, including
  `archive_key0.bin` and `archive_key1.bin`; don't delete or edit those keys.
- SD-root `luigis_mansion_crash_a.bin/.txt` and
  `luigis_mansion_crash_b.bin/.txt` when present. Existing crash files may be
  older; send them as found. A hard freeze may not generate a new report.

This is an experimental Wii acceptance run. **REBOOT READY describes the
archive format, not a guarantee that every live room/setup will accept it.**
No .41 hardware pass is being claimed in advance.
