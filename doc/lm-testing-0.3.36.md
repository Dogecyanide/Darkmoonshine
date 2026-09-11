# Moonshine Luigi's Mansion 0.3.36 - runner test checklist

Experimental candidate, not 1.0. Clean Japanese GLMJ01 revision 0 only.
The priorities are post-menu-warp loads, cross-floor loads, and being able
to KEEP PLAYING afterward. A successful first frame is not a complete pass.

Nobody needs to do everything: start with A, then choose familiar routes
and features. Different runners covering different rooms is very useful.

This candidate adds missing HUD/effect snapshot owners, validates both complete
particle pools, corrects the lag-counter address, rearms specific one-time room
events, and adds an SD archive browser with request-specific import feedback.
These are targeted fixes, not proof that all routes or encounters now work.
Install the WHOLE app folder: this version needs its matching launcher for SD
storage. The memory reservation is unchanged. The snapshot now includes a
compressed copy of the shared game-resource archive, in addition to 1,536 new
core bytes. This candidate uses ONE memory slot; SD files provide the library.
The expanded coverage needs testing and does not make states portable across
reboots or different boss maps.

## Before starting

- Back up your memory-card save. Prefer a completed Hidden Mansion practice
  file, and record its mansion mode, progression and owned medals.
- Confirm the HUD says 0.3.36. Make fresh mod states: format 21 rejects old
  layouts. Never resume a native Dolphin checkpoint from an older mod build,
  because that also restores its old code.
- Record platform (Wii/Wii U/Dolphin), controller model, known firmware and
  any trigger remaps. Especially note Phob versus stock GCC. Do not change
  firmware just for this test.
- Room clear/reset and world options change live progression. They do not
  automatically save the memory card, but a later manual save can preserve
  them. Use a disposable copy of the card for those tests.
- Keep the SD inserted while running. Do not remove storage or corrupt files
  to provoke a failure. Stop after a crash and preserve evidence before a
  new session or more saves overwrite useful logs.
- Mark PASS / REFUSED / CRASH / WRONG RESULT / NOT TESTED. A refusal is not
  a crash. Record exact status/G/X codes when possible.

## Controls

D-pad Left saves the selected slot; Right loads; Down opens the menu.
Stick/D-pad navigates, A selects/applies, B backs out. L/R changes pages.
Room Warps uses left/right to jump ten entries, A twice to confirm, and X
for Boo-safe alternate entries. A menu warp is a native scene reload, NOT
a savestate load. Please identify which operation you used in every report.

The menu owns controller input but does not pause the world. Enemies may
continue acting. After EPOCH, hold Z to reveal the detailed diagnostic panel;
the journal records refusal details without holding Z.

## A. Quick checks - please start here

1. **Boot / movement / menu.** Enter gameplay, move in every direction, turn,
   use A/B and the C-stick/GBH as appropriate. Visit several menu categories,
   close it and move again. Report missing inputs, duplicate actions, world
   input leaking through the menu or unreadable/cut-off text.
   If Left/Right seems ignored, TAP it once with the menu closed, release,
   then open Savestates and photograph the sticky `Dpad/Link/Edge/Use` line
   and `Gate` line. No held-button capture is needed. Use=1 means the request
   reached the save/load handler, not that the state succeeded.
2. **Physical L/R.** Enable Input display. With both triggers released, pull
   L gently, then fully, release, and repeat with R. The matching trigger
   graphic/raw number should react, not the other side. R normally vacuums;
   L expels a stocked, owned element. An empty tank alone cannot prove L is
   broken. Record whether soft pull, full click or both are wrong. Especially
   important on Phob: the new compatibility patch needs physical testing.
3. **R-pump popup.** Enable Displays -> R-pump display. L alone must not start
   a count. R should vacuum and show a small right-side popup. Compare short
   and long holds. The result should remain for 90 game updates after release,
   then vanish. OFF hides it. Record clipping, overlap or wrong counts.
4. **Same-room baseline.** Save at an obvious safe position, walk elsewhere,
   load, then move and vacuum for 20-30 seconds. Repeat three times. Check
   position, facing, camera, health and obvious room state. Report missing
   audio separately, even if gameplay otherwise works.
5. **Blackout both ways.** In stable Foyer, toggle ON, close the menu and
   observe, then toggle OFF. Repeat in another room you know. It must not
   remain permanently BUSY. Distinguish a refused option from an option
   that changes its label but does not change lighting.

## B. Main release blockers - post-warp and cross-floor

For each route, save/load only after Luigi is controllable. Use a fresh
state for a NEW route, but keep the same state for repeats of one route.
After EACH successful load, move for 20-30 seconds, vacuum, then enter and
leave a normal door where available. Repeat the main routes three times.

6. **Same-room menu reload.** Save in Parlor, menu-warp to Parlor, wait until
   settled, then load the earlier state. This previously refused even with
   the same room name. Repeat in another room if it passes.
7. **Parlor -> Anteroom menu route.** Save in Parlor, menu-warp to Anteroom,
   load Parlor, then use a normal door. Repeat three times.
8. **Reverse route.** Save in Anteroom, menu-warp to Parlor, load Anteroom,
   then use a door. Test 7 does not establish this direction.
9. **State made AFTER a warp.** Menu-warp to Parlor first. Make a new state,
   then test a same-room load and a load after a normal door to Anteroom.
   This checks the saved side, not only the live destination.
10. **Two menu warps.** Save A, menu-warp to B, then C, then load A.
    Record all three rooms and test the next door.
11. **Mixed route.** Save Parlor, use a normal door to Anteroom, menu-warp
    elsewhere in the mansion and load Parlor. Reverse which part uses a
    menu warp if the first sequence passes.
12. **Foyer camera regression.** Save at the bottom near spawn, walk to the
    top of the stairs and load. Luigi AND camera must return. Then walk
    upstairs and use the intended door: no locked upstairs camera or early
    neighboring-room cutscene.
13. **Foyer -> Storage menu route.** Save Foyer, menu-warp Storage, load
    Foyer and check movement/doors. Reverse it: save Storage, warp Foyer,
    load Storage. Report direction-specific failures.
14. **Natural cross-floor.** Save on one floor, reach another using ONLY
    normal movement/doors, then load. Test the reverse direction too.
    Record actual start/end rooms; "upstairs" alone is ambiguous.
15. **Distant floor menu route.** Choose an accessible 3F room and basement
    room. Test menu-warp/load in both directions. Avoid boss arenas in this
    test: they cross a different map boundary.
16. **Long natural route.** Save, visit 2, then 4, then 6 or more rooms and
    load using only normal doors. Extend only after shorter routes pass
    movement/door checks. Record visited rooms IN ORDER, not just distance.
17. **Delayed failure.** After a distant successful load, play naturally
    for 2-5 minutes: GBH, furniture, ghosts and several doors. Note the time
    after load and the exact action preceding any problem.
18. **Repeated mixed cycle.** After short routes pass, try 5-10 cycles of
    save -> menu warp -> load -> normal door between familiar rooms. Also
    reuse ONE unchanged state for several cycles. Stop on the first failure.

## C. Safety refusals and the expanded memory slot

19. **Load during door animation.** With a known-good state, press load once
    during a door sequence. Expect BUSY/DOOR, no crash, and no delayed load
    by itself. After the transition finishes, deliberately press load again.
20. **Save during door animation.** Start with a valid state already saved.
    Try to save during a door sequence. It should refuse. Afterward, check
    loading still uses the earlier successful state, not a partial replacement.
21. **Other busy situations, optional.** If encountered naturally, try an
    action during a cutscene, ladder, loading gap or GBH interaction. A safe
    refusal is acceptable; a crash or unexpected delayed action is not.
    These situations are not all claimed supported. Do not mash requests.
22. **One expanded slot.** Confirm the menu offers one memory slot. Save a
    visible position, replace it with another, and load three times. Check
    the new position and model appearance, then move and use a door.
23. **Capacity refusal, if encountered.** Record CACHE FULL needed/available
    sizes and both rooms. The previously active valid state should survive
    a refused save/import. Do not deliberately fill the SD.
24. **Recovery after refusal.** After a safe busy/epoch refusal, make a fresh
    stable same-room state and load. Normal state controls should recover;
    stale refusal details should not be mistaken for a new failure.

## D. Room tools and practice options - use a backed-up card

25. **Ordinary room reset.** In a supported cleared ghost room, choose Room
    tools -> Reset current room. Press A once and cancel: progression should
    be unchanged. Then confirm a reset. Check the actual dark/uncleared
    setup, ghosts, camera, interaction and exit/re-entry, not just dim lighting.
26. **Foyer reset.** Try from lower and upper Foyer. Check linked-room
    lighting/camera, spawn placement and ordinary traversal afterward.
27. **Special reset, choose familiar rooms.** Nursery, Twins, Guest Room,
    Astral Hall, Observatory, Ceramics, Artist, Cold Storage, Pipe Room or
    another supported room. Describe missing puzzle pieces, flags or actors.
    If practical, complete the room again after resetting it.
28. **Fortune-teller bookkeeping, optional.** On a disposable progressed
    file with some Mario items shown, reset and check those items become
    available to show again without granting never-collected items.
    Record exactly which items had been shown before reset.
29. **Unsupported reset.** NO ENTRY POINT should refuse clearly and leave
    the room playable. Twelve hall/unused logical segments lack a validated
    reset entry: 15,18,26,29,31,32,54,58,64,65,68,71. Expected refusal is not
    a successful reset. Coverage is 60 mansion room IDs plus four bosses.
30. **Room clear regression.** Clear an ordinary room, leave and re-enter.
    Check ghosts, lighting, exits and expected completion behavior. Cancel
    confirmation once too. Report duplicate actors or functional locked doors.
31. **New clear recipes, pick a subset.** Ballroom, Graveyard, Rec Room,
    Nursery, Twins, Guest Room, Study, Observatory, Ceramics, Artist,
    Balcony3F, Cold Storage and Secret Altar. Check rooms you know well.
    Some special recipes do not use a normal lights-on step. Source recipes
    cover 72 logical IDs; this is NOT a claim of 72 hardware-tested rooms.
32. **States around room tools.** Save BEFORE clear/reset, perform it, load,
    and test movement/door use. Separately make a NEW state AFTER the tool
    and test that. Report which side the saved state represents.
33. **Boo-safe entries.** In Room Warps compare X: Boo-safe OFF/ON at a room
    labelled "Alternate entry available". Check placement and Boo-event
    behavior, then normal exit/re-entry. "Uses standard entry" destinations
    should keep their normal point regardless of the toggle.
34. **Poltergust tank.** Select FIRE/WATER/ICE in Game
    options -> Tank preset and A to set/refill. Close, expel with L and check
    depletion and a corresponding room interaction. NONE empties the tank.
    This edits the tank, not medal inventory or acquisition cutscene flags.
    Record medals owned and whether the game permits using each preset.
    Release suction/spray before applying. Check gauge behavior after a
    state load and menu warp too.
35. **Existing options, optional.** On a disposable card copy, spot-check
    what you use: HP 1/100, mansion mode, Boo spawning/gates, fire doors,
    Observatory door, traps, unlock-all, plant presets, music and stop.
    Change ONE option at a time and record room/value. These are not all
    reversible without restoring a state/card. Boo gates can legitimately
    show N/A when a completed file already has enough Boos.
36. **Boss restart, separate optional test.** Use an existing boss menu warp,
    then Room tools -> Restart current boss battle. Report separately from
    mansion-room loads. This is not a complete Boss Rush, and mansion/boss
    map savestate portability is not promised.

## E. Displays, timing and menu usability

37. **R-pump interruption.** Hold R, open/close the menu, release, then start
    a fresh hold. Repeat around a successful load. Partial holds must not
    survive or create huge counts. L+R together may count R; L alone after
    an expired result must not restart it.
38. **Input display.** Test sticks, face buttons, D-pad and soft/full
    triggers. Check graphics/raw values match physical input and remain
    readable alongside the memory HUD and popup. OFF hides them. All four
    D-pad indicators should now react; holding Z must not hide this display.
39. **Metadata.** Compare POSITION and POSITION + DELTA while idle, walking,
    turning and on stairs. Room/map/HP/angle should remain sensible.
    Speed is horizontal XZ displacement in game units per completed update;
    XYZ/deltas remain hundredths of a game unit, not a verified
    internal velocity field. Report stale data or teleport-size movement
    remaining as ongoing enormous speed during later ordinary updates.
40. **Lag counters.** Reset, play in a quiet room, then a known laggy room.
    Record the route and whether counts grow plausibly. Menu/state I/O is
    intended to be excluded. This measures extra retraces/slow updates, not
    every definition of game-engine lag or a speedrun timer.
41. **Timing reference, optional.** Record a monitored press/hold, arm, then
    try earlier/later and shorter/longer inputs. Check re-arming after load.
    This compares to YOUR reference, not verified pearl-dupe/Chauncey success.
    A real trick clip with frame-by-frame input expectations would help.
42. **Luigi colour.** Try white/red, custom RGB, then original clothing.
    Check cap/shirt after a normal door, menu warp and state load. Report
    other textures changing or corruption. Separate cutscene models are
    not all recoloured; that alone is a known limitation.
43. **Menu/overscan.** Use stick and D-pad, L/R pages, held-direction repeat,
    cancel confirmations and reopen categories. Report lost cursors,
    unintended game actions, clipped labels and obstructive overlays.
    Full-screen captures are best for layout problems.

## F. SD archives - Wii only, SAME RUNNING SESSION

Optional. Do not remove the SD during these tests. Playable exports are
numbered .lms files under lm_states. Attempt journals are NOT playable
savestates. Cross-reboot portability is still not implemented.

44. **Export/import round trip.** Save A and export a new file. Save a
    different B into the slot, open Savestates -> Browse SD archives, select
    A's file, press A twice to import, then back out and explicitly Load.
    Import alone must not move Luigi. Note the file ID for your report.
    Check A, followed by movement and a door.
45. **Two archives.** Export two distinct states. Import/load each from the list
    into the single memory slot in the same session. Check it never loads
    the wrong file and that each remains usable after several imports.
46. **Missing archive.** Choose a non-existent archive ID. Expect an error
    naming that requested ID (not stale success), with the current valid
    state remaining usable. Do not corrupt a file or
    unplug storage to manufacture an error.
47. **Session boundary, optional LAST test.** After collecting useful session
    results, soft-reboot and try an old SD archive. A session/EPOCH refusal
    is currently expected. The browser must label it DIFFERENT SESSION and
    refuse import; manual import must show an error, not success. It must
    not crash or partially restore data.
    Then make a fresh same-room state in the new session and load it.
    In-game preferences are also session-local; reset settings are a known
    limitation, not a newly solved persistent-configuration feature.

## G. New regression targets - please pick what you know

48. **Post-warp save / second warp / load.** Save, menu-warp and try loading.
    Record a refusal if one occurs. Then make a NEW state in that room,
    menu-warp again and load the new state. This specific sequence previously
    crashed. If it loads, test a door, suction, element meter and GBH, then
    keep playing for two minutes. Stop on the first failure.
49. **Rearmed encounters.** After completing Parlor or Astral Hall, reset
    and perform the candle/puzzle trigger again. Check the encounter actually
    starts and can be completed twice. Nursery: reset, initiate Chauncey,
    and check both the room sequence and boss intro rather than lighting alone.
50. **Boss intro replay.** Where you know the battle, warp to/restart each
    boss twice. Bowser must enter a usable battle, not idle indefinitely.
    These are individual restarts, not an implemented Boss Rush chain.
51. **Archive list navigation.** With several existing files, check sorting,
    Up/Down, A confirmation, B back, and X refresh from the first page. With
    more than eight files, use Right/Left between pages. Every listed file
    should exist; do not delete files or remove the SD during a session.
52. **Archive cancellation / busy.** Press A once on an importable file,
    change selection or press B, and verify nothing was replaced. After a
    confirmed import starts, B may leave the browser while the transfer
    finishes; a second storage action must not start over it. Then load and
    verify the intended slot. Slot changes during transfer must be blocked.
53. **Old build / session feedback.** Existing incompatible files should
    remain visible but labelled, not offered as successful imports. A missing
    manual ID should leave the old slot loadable. Following an earlier import
    success, a failed new request must display its OWN error and file ID.
54. **Lag and speed sanity.** Updates should now increase during ordinary
    play; zero lag in a quiet room is fine. Compare a known slow scene. Speed
    should be zero idle, nonzero walking, and reset at a state-load/menu-warp
    boundary rather than count the teleport as normal movement. Capture the
    screen if values remain frozen or labels overlap.

## Exactly what to send back

Return files even for a session without crashes: passing long routes help.
Copy evidence only after the console is off and storage is safe to remove.

- BOTH lm_dumps/lm_attempt_a.bin and lm_dumps/lm_attempt_b.bin.
  They rotate on successful saves and track the last TWO save generations,
  not every savestate from the whole evening. Further saves can replace
  useful evidence, so stop after a failure.
- luigis_mansion_crash_a.bin/.txt, luigis_mansion_crash_b.bin/.txt if present,
  and ndebug.log. A hard freeze may have no exception report; the journals
  still help. Existing crash files may be older: give build and approximate
  time of the new problem.
- Exact test number, source/destination/intermediate rooms in order,
  normal doors versus menu warps, memory slot, and whether the state was
  saved BEFORE or AFTER a warp/reset/clear.
- Exact refusal text/G/X code; instant crash versus loaded frame then crash
  versus delayed failure. Name the action after load and approximate delay.
  Say whether the Wii reset itself or normal power-off worked. A reboot
  alone does not establish out-of-memory.
- Video/screenshot if possible, and pass counts. "Three loads, each followed
  by door exit/re-entry" is more useful than just "worked".

Copyable report:

~~~text
Build: 0.3.36
Platform / controller / known firmware:
Normal or Hidden Mansion / progression:
Test number(s):
Saved room / slot / before or after warp or room tool:
Route in order (mark normal doors and menu warps):
Location where I pressed LOAD:
Result + exact status/G/X code:
Action after load / time until failure:
Repeated passes before failure:
Other options changed:
Attached both journals + crash files/log/video:
~~~

Dojo, rush chains, physical item/medal spawning and a verified trick-success
detector are not included. Boss-map state loads remain guarded. A short
route passing is not proof of mansion-wide reliability. Precise failures
are valuable; you do not need to diagnose their cause.
