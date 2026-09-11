# Dojo and rush reconstruction audit

Status: read-only implementation audit, 2026-09-07. The runner feedback now
establishes these as used features, not optional curiosities. No dojo/rush
runtime implementation is included by this document.

## Conclusion

Both are feasible without asking the player to patch an ISO. They need more
than a menu row or a map warp. Boss Rush is the smaller first implementation;
the dojo and Portrait Rush each need a separate encounter controller. A native
menu/state machine plus narrowly scoped runtime data overlays is preferable to
installing GaddWarp's bootstrap or replacing all its disc files.

The 0.3.35 report makes complete encounter initialization a prerequisite:
Bowser restarted into an idle fight, Nursery did not reach a functioning boss
fight, and Parlor/Astral Hall reset candles did not trigger their encounters.
These failures would propagate into rush even if every destination loaded.
Fix and validate encounter setup/reset before calling a chain a rush mode.

## Evidence used

- Verified clean and xdelta-reconstructed GLMJ01 assets under
  `build/gaddwarp/extracted/{clean,patched}`.
- Patched `Game/game.szp` compared in memory against the user's clean ISO.
- Modified event01, event10, event26, event67, event74, event77, event90,
  event101 and added events102--109.
- `lm_diag/src/lm_warp.cpp`, `include/susamune/lm_warp_transition.h`, and the
  current practice API.
- Local `../lm-decomp/src/Koga/{MissionMode,EnManager}.cpp` as structural
  reference, not an authenticated JP address source.
- Runner report in the workspace's
  `sd-captures/lm-0.3.35-runner-20260907/runner-report.md`.

The existing GaddWarp inventory is useful but some descriptions are broader
than the directly observed active branches. Corrections below take precedence
over its scope summaries; the old documents have not been edited in this audit.

## What the dojo actually provides

The map is the Training Room, map 3, with a custom event10 controller.

- **Custom Wave:** choose one to seven ghosts, each from twelve ghost types.
  These are seven simultaneous spawn slots, not seven saved wave presets and
  not twelve simultaneous ghosts. Names: Gold Ghost, Purple Puncher, Blue
  Twirler, White/Red/Invisible Grabber, Garbage Can Ghost, Red/Green Ghost Guy,
  Skeleton, Purple Bomber, Bowling Ghost. Duplicate types are possible.
- **Random Wave:** choose one to seven ghosts; the script randomizes among
  those twelve choices. The script asks the player to move to vary the seed.
- **Rhythm Ghosts:** up to five choices using Easy/Normal/Hard/Extreme rhythm
  parameters (`iyapoo17`--`iyapoo20`). The difficulty labels belong here, not
  demonstrably to the ordinary random-wave generator.
- **Preview:** an optional visual preview before releasing the encounter.
- **Endless Random:** repeating random encounters, explicitly no healing,
  ending on loss of HP. It must not silently inherit Infinite Health.
- **Textbox Mashing:** a separate text sequence, ready/go prompts, and replay
  flow. It is not a raw frame-perfect-input success detector.
- **Environment controls:** mice, flying fish, sparks, mansion mode, HP 100/1,
  and a selected BGM for when the lights go off.
- **Entry/exit:** custom gate/camera/control transitions, return to the lab,
  and transient mode cleanup.

A `pearldupin` branch exists and warps to the roof, but the inspected active
minigame CHOICE exposes Rhythm Ghosts / Endless Random / Textbox Mashing /
Back. No active CHOICE or jump to `pearldupin` was found. Do not advertise this
as a confirmed selectable JP 2.2 feature on label/branch presence alone.

### Concrete data dependencies

Clean map3's EnemyInfo has **zero** rows. GaddWarp's has **104**:
12 ordinary types x 7 slots, plus 4 rhythm types x 5 slots. The script selects
a ghost by killing the other predeclared alternatives, then releases gates
and observer flags. Simply calling the retail Training Room is not this mode.

Important map3 changes, compared directly:

| Member | Clean bytes | Patched bytes | Role |
| --- | ---: | ---: | --- |
| `jmp/enemyinfo` | 352 | 24,064 | 104 encounter actors |
| `jmp/characterinfo` | 3,072 | 3,424 | Retained character/spawn data |
| `jmp/generatorinfo` | 1,760 | 2,432 | 9 generator records |
| `jmp/observerinfo` | 352 | 2,432 | 10 observer records |
| `jmp/eventinfo` | 608 | 1,024 | Event activation |
| `jmp/furnitureinfo` | 448 | 1,248 | Interactions/scene objects |
| `jmp/objinfo` | 416 | 3,680 | Scene objects |
| `jmp/railinfo` | 384 | 736 | Paths/rails |
| `jmp/roominfo` | 256 | 256 | Changed content despite equal size |

Added: `jmp/iyapootable` (four rhythm definitions), four item tables,
`path/obake_path`, `path/tenjyo_path`; `path/rat1` is changed. Patched map3 also
has 94 added `j3d_effect` members totaling 753,514 bytes before archive packing.
Some are debug output; do not bundle packing artifacts or count this raw size
as the final runtime requirement. Reuse stock model/effect resources from the
user's own disc wherever byte-identical, and measure the actual mounted heap.

The five rhythm parameter files `Game/game.szp/param/ctp/iyapoo16.prm` through
`iyapoo20.prm` already exist on the clean disc. GaddWarp changes 4, 15, 17, 17,
and 16 bytes respectively; none is an entirely new 636-byte parameter format.
These are candidates for authenticated, mode-scoped data edits rather than
shipping/replacing the full Game archive. Field semantics and restoration on
mode exit still need decoding. The 104 actor rows and their referenced
resources are the larger dependency.

## Rush behavior

### Boss Rush

Entry requires the Ice Medal. Event01 clears boss flags
39/46/67/68/81/82/66 and 222, then sets mode flags 42 and 3 and requests map10.
Modified event67 uses the defeat flags to advance:

`Chauncey (10) -> Bogmire (13) -> Boolossus (11) -> King Boo (9)`

Modified event90 handles King Boo completion and the end prompt/title exit.
Individual fight replay paths additionally clear 34/1, restore control/menu
and weapon state, and reload. The current native warp plan resets at most
three boss flags; it is not a faithful copy of those replay semantics, and
the runner's idle Bowser is direct evidence that arrival is not enough.

A native rush controller should own `mode`, `stage`, and pending completion
instead of copying GaddWarp's repurposed retail flags 3/42 into ordinary
gameplay. Advance from an authenticated defeat/completion signal, not a
generic room light, actor disappearance, or map number. Suppress or replace
the normal boss-return flow only while that controller owns the encounter.

### Portrait Rush

Entry requires all three medals. Four variants are encoded by flags 84
(omit optional portraits) and 34 (omit bosses). The setup runs a long
room-specific dark/reset sequence before starting at Neville's Study.

Observed complete order, assembled from modified event101 and event67:

1. Neville -> Lydia -> Chauncey.
2. Whirlindas -> Shivers -> Melody -> Mr. Luggs -> Spooky -> Bogmire.
3. Biff Atlas -> Miss Petunia -> Nana -> Slim Bankshot -> Henry/Orville ->
   Madame Clairvoya -> Boolossus.
4. Uncle Grimmly -> Clockwork Soldiers -> Jarvis -> Sue Pea -> Sir Weston ->
   Vincent Van Gore -> King Boo.

The no-optional branches skip Luggs, Biff, Slim, Jarvis and Sue Pea. The
no-boss variant skips all four main bosses and finishes after Van Gore.
After Boolossus (or its skipped branch), the controller enables blackout and
places Luigi in the Wardrobe Room for Grimmly; Grimmly completion turns
blackout off. These stage semantics must survive retry/exit, not leak into
normal mansion play.

Event101 watches portrait-completion markers 233/234/236--240/242--247/
249--254, clears each consumed marker, and selects the next point. Those
markers are meaningful because the patched actor/observer/event data set
them. Polling the same retail flag IDs on a clean disc will not reconstruct
the mode. Each encounter needs a verified completion signal and reset recipe.

Special setup includes the Twins' activation flags, Madame Clairvoya's
Mario-item/story prerequisites, Guest Room puzzle state, Cold Storage, and
Van Gore's multi-wave flags 177--183. Existing room-clear bits alone cannot
provide those prerequisites, and the broken candle encounters demonstrate
why successful room reloads cannot be used as the only acceptance criterion.

### Added event archives are not the rush chain

Directly inspecting their internal `text/event01.txt` members shows:

| Archive | Active role |
| --- | --- |
| event102 | Lab door/camera movement into dojo, map3 |
| event103 | Movement to Portrificationizer, map4 |
| event104 | Opening request with mode cleanup |
| event105 | Prompt to enter Portrificationizer |
| event106 | Movement to Gallery, map6 |
| event107 | Dojo exit confirmation and cleanup to lab, map1 |
| event108 | Movement to Portrificationizer, map4 |
| event109 | Empty script |

Thus the older shorthand “new events102--109 implement rush stages” is not
accurate. The principal chain dependencies are modified event67/event101,
the individual encounter/defeat data, and event90 completion.

## Clean-ISO implementation plan

1. **Repair encounter prerequisites first.** Authenticate stock boss startup,
   replay, completion and return paths, plus special-room puzzle reset.
   Verify real fight activity and completion, not just map arrival. Keep
   encounter presets separate from generic room-warp destinations.
2. **Add a mod-local encounter coordinator.** Enter/await stable/start/active/
   completed/retry/exit, using the existing queued native scene loader and
   I/O gates. Do not retain game pointers across scene recreation. No
   automatic memory-card save. Decide explicitly whether and how a savestate
   can rewind the coordinator; otherwise refuse states during an active mode.
3. **Boss Rush first.** It reuses the four stock boss maps, with native
   completion interception and the coordinator replacing script chaining.
   Provide Retry Fight, Restart Rush, and Exit. Do not label four boss-menu
   entries a rush mode.
4. **Dojo data/control layer.** Create only requested enemy rows (bounded 7
   ordinary or 5 rhythm slots), provide the required paths/parameters and
   resource references, and let retail constructors initialize enemies.
   Existing shadow CharacterInfo support proves only player placement;
   EnemyInfo, observers, generators and retained ToolData lifetimes need
   their own authenticated integration. Native UI can replace event10's
   many flag/menu branches. Preserve settings in mod-owned state, not
   GaddWarp's reused story flags.
5. **Portrait Rush.** Compose verified encounter recipes and completion
   watchers, then implement the four route variants. Stage transitions that
   span blackout, boss maps or inventory requirements deserve explicit
   states and tests rather than generic flag clearing.

If asset overlays are used, load immutable mod data from the SD through the
launcher into a bounded, non-overlapping reservation and expose it only while
the target mode/map is being built. Authenticate any edits to clean archive
members. Do not stack GaddWarp's DOL loader or ship whole retail archives.
Any mod-owned resource pointer captured in a state needs stable lifetime or
explicit reconstruction; scene-local overlays cannot be silently freed while
a state still references them.

## Acceptance tests before calling these modes complete

- Each boss starts acting, takes damage, completes, and can be retried three
  times from a completed Hidden Mansion file and a fresh normal file.
- Full Boss Rush advances exactly once per defeat, in order, with no stock
  gallery/ending detour. Retry, death, abort and restart work at every stage.
- Dojo: every one of the twelve ghost types alone; duplicates; all seven
  slots; all four rhythm difficulties alone and all five rhythm slots.
- Compare rhythm speed/HP/escape behavior against original GaddWarp, not only
  the displayed difficulty name. Preview must not leave active duplicates.
- Toggle each hazard independently; verify camera, lights, music, control and
  rumble on start/finish/retry/exit. Ordinary Training Room must still work.
- Endless Random never heals silently and ends coherently on death. Explicit
  Infinite Health policy is visible. Textbox practice can abort/replay.
- All four Portrait Rush variants complete in the specified order, including
  Grimmly blackout setup, optional skips, no-boss ending and multi-actor fights.
- Reset Parlor/Astral Hall/Nursery/Guest Room/Clairvoya/Van Gore and complete
  the actual puzzle/encounter each time; test both foyer segments together.
- Enter and exit each mode without changing normal mansion event meaning,
  medal/inventory state, doors, or card data unexpectedly.
- Exercise save/load or explicit refusal at each coordinator phase; ensure a
  load cannot double-advance a chain or retain dead actor/resource pointers.
- Wii memory diagnostics at maximum simultaneous enemies/effects, repeated
  stages and a full chain; failure must be bounded and leave the prior state
  usable. Record mode/stage/attempt in the journal and crash context.
