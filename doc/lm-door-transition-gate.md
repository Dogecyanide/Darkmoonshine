# Native Japanese door-transition gate

Clean GLMJ01 DOL SHA-1: `722005ea9c1eab54b114f814734d8f327e5614ee`.

The narrow native predicate is:

`owner = *(player + 0x7E4); busy = owner[0x310] == 2 && owner[0x314] == 0x20`

These are byte offsets read using **32-bit loads**. Retail player code at
`800AD8B8..800AD8D0` performs those exact loads/comparisons. `800B807C`
uses the same door predicate, alongside a separate ladder state `0x21`.
The proposed guard blocks doors only; it does not claim to cover every
cutscene, death, boss warp, or ladder transition.

## Command IDs are not stored state IDs

The name table at `80347E10` labels command `0x0C` DoorAutoMove and commands
`0x14..0x19` DoorWait, DoorOpen, DoorStopOpen, DoorOpenHalfwayOpen,
DoorOpenHalfwayClose, and DoorOpenDummyEnd. Those are inputs to `80072AB8`,
**not** the value stored at owner+`0x314`.

Its command-`0x0C` jump-table entry at `80346C18` targets `80072F98`.
That path sets initial door substate 1 and invokes `80073DC8` with state
`0x20`. The setter immediately writes mode 2 at `+0x310`, state at `+0x314`,
and the door-specific arm copies substate to `+0x31C`. Door commands then
advance `+0x31C` while the outer state remains `0x20`.

`80078194`'s door update checks mode/state, dispatches substates through
`80347008`, and handles opening/crossing/half-open/dummy-door phases.
Common action completion at `80074318/80074324` writes mode 1/state 0;
the old-state-`0x20` continuation at `80074548` re-enters the door state
for intermediate phases (including 4→5 and 9→10). The stop-open route
`80072C10..80072C24` also leaves door state explicitly. Therefore do not
unblock merely because a single animation finished or the room byte changed.

## Read-only player lookup

The public getter `800E7E7C(0)` reads mission singleton `804A17C8`, then
mission+8. `800E3D94` reads manager+`0xE08` (slot stride `0x1C` for a second
player), then `80067EC4` looks that actor index up in `803C8490[index]`.
The public getter subsequently performs RTTI conversion through `801F58F8`.
A mod-side read-only chain should bounds-check before every read instead of
calling that unbounded native lookup with possibly invalid transition data:

1. Validated mission must cover `+8` in GAME.
2. Manager must cover `+0xE08` in GAME.
3. Actor index must be below the table's capacity 128.
4. Player must cover `+0x7E4` in GAME and have vtable `8034EE50` before
   treating an arbitrary actor as a player.
5. Controller must cover `+0x314` in GAME. A null/foreign controller is not
   evidence of an idle door and should be reported as unavailable/busy.

In the captured `.30` MEM1, the chain is mission `80BE4990` → manager
`80C167D0` → index 0 → player `81391510` → controller `813B7E50`.
The player vtable is `8034EE50`, controller+`0x44` points back to the
player, and mode/state are 1/0. These addresses are fixture observations,
not hardcoded runtime allocations. The retail predicate uses the player
owner pointer directly and requires no heap allocation, RTTI call, or waits.

The type check and lack of actor-to-player pointer bias are independently
proved by retail code/data. Constructor `800ABF2C` passes unchanged `this`
to the base constructor, then installs `8034EE50` at `800ABF60..800ABF68`.
That vtable has RTTI `8049B844`, name `Player`, and a zero top adjustment
at `8034EE54`. The public getter's target RTTI `8049C9A8` also names
`Player`. `__dynamic_cast` reads the top adjustment at `801F5928`, adds it
at `801F5930`, then returns immediately on matching names at
`801F5984..801F598C`. Therefore this exact vtable produces the same
pointer as the public getter. Other vtables should be rejected, not treated
as equivalent through speculative pointer arithmetic. Controller+`0x44`
is only a fixture observation and is not required as a runtime invariant.

## Placement and remaining scope

Before this addition, `actionIdentity` checked GAME identity, idle I/O,
cached stable frames, and unchanged identity. None establishes that Luigi
is outside the native door action: a door can animate while all I/O is idle,
and same-map door crossings retain mission/heap addresses. A null event
interpreter cursor does not establish idle door state either. Door-global
`804A0D8A` (`r13+0x2AA`) controls all-door/trap behavior, not this lifetime.

Place the fresh door check in `buildIdentity` after mission/game-mode
validation. It then runs at request/preflight, again after audio quiescing,
and under scheduler/interrupt freeze before save/restore writes. Do not rely
solely on the previous presenter's cached `readyForAction` result. When the
native door action ends, use the existing I/O/stability requirements before
accepting another operation. This makes a narrow *current-transition* guard;
it does not explain or fix a distinct crash after a completed door crossing.

The pure `lm_door_state.h` helper provides the predicate and overflow-safe
GAME range checks. `scripts/test_lm_door_state.py` executes it natively,
checks the real MEM1 lookup chain, and authenticates the retail instructions.
