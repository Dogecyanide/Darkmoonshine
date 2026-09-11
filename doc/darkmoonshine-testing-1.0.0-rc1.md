# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 1

25-test runner stress test. Prioritize reusable SD states, long/post-warp
restores and normal gameplay AFTER loading. Split groups between runners if
useful; nobody needs to redo a blocked prerequisite just to fill a checkbox.
Report **PASS / REJECTED / CRASH / WRONG RESULT / BLOCKED / NOT TESTED**.

## Before testing

- Use Japanese GLMJ01 revision 0 and a completed Hidden Mansion practice file.
  Back up the normal memory-card save and entire `lm_states` folder first.
  Don't overwrite the normal game save with experimental test progress.
- Install the whole matching RC1 app. The Homebrew Channel name is DarkMoonshine,
  but its existing `moonshine_luigis_mansion` folder, `moonshine_lm.ini`,
  `lm_states`, `lm_dumps` and crash filenames deliberately remain unchanged.
- Create fresh RC1 states/archives. The layout remains format 27, but .44 and
  other builds' archives have a different build identity and are not loadable.
  Keep old evidence/settings/keys intact; do not relabel archive headers.
- D-pad Left saves, Right loads, Down opens the menu. A Room Warps destination
  reloads a scene; this is different from restoring a savestate. SD Import only
  fills the memory slot: close the menu and press Right to restore gameplay.
- Normally wait until Luigi is controllable and the transition has finished.
  Deliberate transition/rumble tests are identified below. Don't force refusals.
- After EACH accepted load, check position, camera, actors, collision, HUD and
  audio; move for about 20 seconds and use a normal door if available. A correct
  first frame alone is not a pass. Note delayed or next-door crashes.
- Keep game file, video/progressive-mode choice, controller port and launcher
  settings consistent across reboot tests. Do not remove SD during an operation.
- The top-left action popup is brief. Refusal details remain in the States menu
  and diagnostic files; the giant memory HUD is no longer required for testing.

## A. Everyday loads and natural routes

1. **Branding, popup and controls.** Confirm DarkMoonshine, authors Dogecyanide,
   Nintendont Team, and the full RC1 version in the launcher. In gameplay,
   save/load using D-pad and the menu. Check brief Saving/Saved and Loading/Loaded
   notices, readable inside the top-left edge, with no permanent memory/version
   bar or heartbeat. Repeat the same action: its notice should appear again.
   Opening/navigating editors must not accidentally save/load gameplay.

2. **Ten same-room restores.** Save at a recognizable point; move, interact
   and load ten times without replacing the state. Include turning the camera,
   vacuuming and a normal door crossing after the last load. Watch for degraded
   control, audio, increasing pauses or a delayed crash.

3. **Foyer stairs in both directions.** Save at the bottom, walk to the top and
   load; the camera must return and keep tracking. Save at the top and reverse
   the route. Test walking toward a door afterward, not just the stairs.

4. **Natural Parlor/Anteroom in both directions.** Save in Parlor, enter
   Anteroom normally, load Parlor and re-enter normally. Repeat with a new state
   saved inside Anteroom and a normal exit before loading. No early cutscene,
   missing actor or broken first door animation should appear.

5. **Long natural cross-floor route.** Save, walk at least five rooms away and
   across at least one floor change, then load. Record the actual room/floor
   sequence. Repeat in the reverse direction with a new state. Use a neighboring
   normal door after both returns.

## B. Menu-warps and repeated teardown

6. **Warp to the same room.** Save, menu-warp to that very room and load the
   pre-warp state. Use a door, return and repeat three times without resaving.

7. **Parlor/Anteroom menu warp.** Save Parlor → menu-warp Anteroom → load Parlor
   → enter Anteroom normally. Repeat three times with the original state, then
   test the reverse using a new Anteroom state.

8. **Former Storage/Boneyard crash chain.** Save/export a named Parlor archive.
   Import it, warp Storage → load Parlor → warp Boneyard → load Parlor → enter
   Anteroom normally → return → load once more. Repeat the complete chain twice
   without resaving. Keep this original archive for tests 11–15.

9. **Distant menu-warps across floors.** Save in a runner-chosen ordinary room.
   Warp to three distant ordinary rooms, loading the original state and using
   a normal door after each. Include upstairs and basement destinations. Record
   actual rooms/floors; distance alone is not proof of a floor change.

10. **Save AFTER a warp or restore.** Menu-warp to Storage, save there, warp
    elsewhere and load Storage. Use a door; make a new state in that restored
    room, warp elsewhere and load the new state. Repeat with a different room.
    This checks that restored rooms can safely become new snapshots.

## C. SD archives that remain reusable

11. **Named export/import and empty slot.** Use test 8's named Parlor archive,
    or make one now. Note its ID and REBOOT READY/THIS BOOT ONLY result. Clear
    only the resident memory slot, import the archive through the browser and
    load it. Check the name/room and normal door. Keep the original file unchanged.

12. **Soft reset.** Soft-reset the game, enter the same practice file, import
    that original archive and load it. Use the door and load again. Report
    import and gameplay load separately; a successful browser entry is not a load.

13. **Two complete Wii reboots.** Fully power off/on, import the original
    archive and load. Move/use a door. Power off/on a second time and reuse the
    SAME file again, preferably while standing in another ordinary room. Do not
    create a replacement archive between reboots. If export said THIS BOOT ONLY,
    report that and mark this case blocked rather than repeatedly forcing it.

14. **Reboot plus the full failure chain.** After a cold boot, import the same
    original Parlor archive. Repeat Storage warp → Parlor load → Boneyard warp
    → Parlor load → Anteroom normal door. Return and load again. Report the exact
    action if the first loads work but a later door or warp fails.

15. **Three named archives, one memory slot.** Export distinct named states in
    three ordinary rooms, covering more than one floor. Alternate browser-import
    → gameplay-load A/B/C for three rounds. The correct room/name must follow
    each import; use a door after every load. These are three SD files, not three
    simultaneously resident slots.

## D. Gameplay and transient state

16. **Ghost and pickup replay.** In an available ordinary encounter, save
    before catching a ghost/collecting its pearls or money. Complete it, load,
    and replay twice. Check the ghost, collectible objects and totals rewind
    together, without invisible duplicates or a broken room-clear event. Disable
    any practice options forcing the measured values.

17. **Damage and both rumble directions.** Save while quiet, take a hit or
    trigger another rumble, then load during the rumble. Health/control should
    rewind and old rumble should stop. Trigger new rumble afterward; it must
    start and end normally. If practical, also SAVE during rumble, wait for it
    to end and load that state. Report the event and controller type.

18. **Elements and room practice actions.** Save with a known element/tank
    amount, expend it naturally and load; test the actual Poltergust afterward.
    Disable tank-forcing options for that comparison. Separately, save before
    a supported room reset/clear or Blackout toggle, apply it, wait until idle
    and load. Check lights, actors and doors agree. Report unavailable/busy
    actions without forcing them; name the room/action used.

19. **Ordinary room event replay.** Save before a puzzle, final ghost or
    lights-on event available on your file. Complete it, load and repeat. Check
    event timing, lights, doors and rewards, including leaving/re-entering after
    the restored event. An unavailable setup is NOT TESTED, not an automatic pass.

20. **Door/cutscene refusal preserves the old state.** Keep a known-good state.
    Request a load once during an active normal door, then a save during another
    transition or ordinary cutscene. Expect a clean Busy/Rejected result without
    overwriting the old state. Once idle, load it and use a door normally. Do not
    repeatedly spam a rejected operation; note any stuck controls or rumble.

## E. UI, persistence, boundaries and endurance

21. **Timer and simultaneous displays.** Enable the Sunshine timer, input,
    metadata/speed, lag and R-pump displays together. Check they remain readable
    around the small action popup; R-pump must respond to R, not L alone. Check
    timer pause with the mod menu, resume on closing, rewind on load, retention
    through normal doors and reset on menu scene reload. If available, compare
    Boo capture/introduction stop/resume against expected practice behavior.
    Note slowdown versus the same route with displays off.

22. **Creation, colour and persistent preferences.** Change timer position,
    one character colour, TIME visibility, streak styling, Luigi colour and
    display toggles. Test fast held movement and fine taps; keep one edit and
    discard another. Wait for SETTINGS: SAVED TO SD. Reboot and check the kept
    settings return. Load an earlier RC1 state: CURRENT preferences must not
    rewind to the old state's settings. Input-reference settings should persist
    too if you use them. Never assume every option represents saved game progress.

23. **Names, deletion and continued SD use.** Use two disposable archives only.
    Rename one with the keyboard, test cancelling an edit, then refresh/reboot
    and check its name. Open Delete and cancel; both files must remain. Import
    disposable A, then confirm deleting A from SD: B remains, and A's resident
    memory copy still loads. Export/import a new archive afterward. Deletion is
    permanent; do not use your only useful state. Never pull SD mid-operation.

24. **Secret Altar boundary probe.** Keep a known-good ordinary mansion state.
    Menu-warp to Secret Altar and wait until idle. Try loading the earlier state
    once. The .44 logs show safe rejection here; rejection alone is a KNOWN
    LIMITATION, not a crash. Record the States-menu reason and preserve logs.
    Separately try returning with Room Warps while idle; distinguish that from
    a state load. Do not force King Boo/boss-map restores or edit flags/headers
    to bypass a guard. If stuck, preserve evidence and reboot normally.

25. **Endurance practice session.** Practice for 20–30 minutes, aiming for
    roughly 50–100 loads without rushing transitions. Mix natural multi-floor
    routes, menu warps, repeated reuse of one old state, new captures and named
    SD imports. Include doors/movement after loads. Report main rooms, approximate
    save/load counts and any delayed crashes, new refusals, worsening pauses,
    audio/rumble issues or visual corruption. A crash-free isolated load is not
    the goal; normal sustained practice is.

## Preserve and send the evidence

Copy the entire `lm_dumps` folder after each group of five and immediately after
an unexpected refusal/crash, using separate folders labelled runner/test range.
There are eight rotating journals, `lm_attempt_a.bin` through `lm_attempt_h.bin`,
not eight playable states or a whole-day recording. Further saves can overwrite
older evidence. If copying between groups isn't practical, prioritize a failure
before doing more attempts and mention that earlier logs may have rotated out.

Send numbered results, Wii/controller type, rooms/floors, whether movement used
normal doors or menu warps, and whether the state came from memory or SD. For a
failure distinguish import / load / first frame / later movement / normal door /
menu warp. Include the original tested `.lms` archive and matching `.name0/.name1`
sidecars when relevant, plus `luigis_mansion_crash_a/b.bin` and `.txt` if present.
An old crash file may remain from another version; send it as found. A freeze
may not produce a fresh report. Keep archive keys backed up privately; do not
publish or edit them. No need to send the game ISO.

RC1 is a release candidate, not a universal mansion/boss guarantee. Secret
Altar's resource boundary remains guarded. The native practice clock still
rolls at 36 counted minutes; Dojo/rush and exact pearl-dupe/Chauncey success
judgments are not included. SD services/preferences require Wii, not Dolphin.
