# Runtime room and boss warps

The runtime menu exposes 68 destinations: GaddWarp's complete recovered main
room and hallway list, its two out-of-bounds practice placements, the roof,
and the four boss maps. Labels are independent of the destination data. Room
numbers remain the game's zero-based IDs rather than invented sequential IDs.
An optional Boo-safe placement selects GaddWarp's alternate coordinates without
writing its patched-event-only flag 22.

## Native spawn sequence

`LMWarp::request` accepts a choice only in a stable Mission on maps 2, 9, 10,
11, or 13, with the existing savestate I/O gate ready, no active event cursor,
and no card request in progress. Both admission and the armed `tick` use
`LMState::readyForActionNow()`: the established stability count is retained,
but the live heap identity and I/O readiness are checked again at the point
of action. A readiness result from the preceding presenter is not enough.
The menu must suppress
savestate hotkeys and practice changes while `LMWarp::active()` is true.

### Queued scene handoff (0.3.32)

The transition uses the Japanese retail `BOOL queueScene(u32 scene)` at
`0x8000B20C`, with scene 2. Disassembly verifies that it sets the pause request
at `0x804A0C08`, then returns the result of nonblocking `OSSendMessage` to
the one-entry queue at `0x80398A48`. Native callers at `0x8000EEA4` and
`0x8000EEC0` publish the requested map at `0x804A0C24` before queuing scene 2.
This is a native Mission-to-Mission reload, including when the current scene
is already 2: the receive path does not require a different scene number.

The consumer at `0x8000B378` checks native DVD, card, and audio readiness before
receiving the message. Its successful receive path publishes the scene,
starts the fade, and requests the outer loop to exit. The old Mission's
destructor (`0x8000BE04`) performs effect teardown before copying the requested
map into the live map at `0x8000BEA4`; the next Mission is constructed afterward.
The mod does not write the current scene ID directly. The former direct
scene-ID mismatch route bypassed these native receive-path readiness checks.

Before the send, the mod publishes the requested map, appearance, and only the
destination's prescribed flag bits. `LmWarpPublishAndQueue` preserves their
exact old values and the pause request. A failed nonblocking send restores
all of them and reports `SCENE QUEUE BUSY`; it must not leave half of a warp
published. Successful submission enters `Queued`, not an assumed completed
scene change. The appearance hook accepts either `Queued` or `Loading`, since
no intermediate presenter between queue consumption and new scene setup is
required by the implementation.

There is no speculative queue cancellation or direct queue-counter edit.
After 30 seconds a submitted request reports `LOAD STALLED` but stays active,
so a delayed native consumer cannot receive an abandoned appearance-240
request. A later legitimate initialization can still finish the warp.

The hook at `0x800E2E08` replaces the call from
`EnManager::loadCharacterInfo` to appearance setup (`0x800E3A1C`), after the
clean CharacterInfo table has been attached but before player construction.
The verified original instruction is `0x48000C15`; the caller has the
EnManager in r3 and its ToolData in r4.

For a requested mansion warp, the hook validates the clean table schema
(131 rows, 23 fields, 292-byte header, 184-byte rows) and the retail `luige`
appearance-0 row. It allocates a 484-byte block from the explicit game heap,
containing a ToolData object, the original field descriptors, and one complete
copy of that row. Only position, yaw, room number, and appearance ID change.
The original ToolData vtable is retained. The replacement row uses reserved
appearance ID 240 in its own one-row table.

Only the appearance-setup call receives this small shadow table. The
EnManager's original CharacterInfo remains intact for every other actor and
retail appearance lookup. Retail appearance setup stores a full ToolDataRef
to the shadow row; normal player construction, room startup, and camera
startup consume that reference. There is no post-load coordinate teleport.

The shadow belongs to the scene's captured game heap, so a savestate can
restore its contents and references, and the next scene teardown reclaims it.
There is no persistent mod-static copy that a later warp can overwrite under
an older savestate. Allocation or table-validation failure selects retail
appearance 0 and reports an error, ensuring the loader still receives a
valid row.

Boss destinations use their stock map and appearance 0. Their flag presets
match recovered GaddWarp Event01: Chauncey clears 39/46; Bogmire clears 67/68;
Boolossus requires ice medal 45 and clears 81/82; King Boo clears 66. All four
clear temporary flag 222. Secret Altar sets 34, and Nursery Chest Clip sets
46. Warps do not issue a card save.

The request waits for the initialization hook, the selected map, a live
player, and twelve stable frames. Mansion warps compare the live player room
with the recovered placement metadata. A mismatch or timeout remains visible
through `statusText()`; `actualRoom()` exposes the observed ID. Boss intro
events retain normal retail control.

Warp request, acceptance, dispatch, appearance setup, preparation, settling,
arrival, and rejection are emitted as action-4 lifecycle records. These use
the bounded nonblocking diagnostic lane on Wii; the emulator retains a local
debugger-visible lane but does not provide the Wii ARM/SD journal consumer.

## Data verification

`scripts/gen_lm_warp_points.py` generates the placement constants directly
from the recovered CSV in `gaddwarp-jp2.2-feature-port.md`. It embeds no retail
assets. `scripts/test_lm_warp.py` checks every destination and alternate, boss
map assignments, generated-data freshness, and (when maintainer assets are
present) the actual clean-DOL hook and every exposed GaddWarp row against the
clean template. Those comparisons prove the complete create-row scalars are
preserved. GaddWarp's hallway rows contain a `(nulll)` typo in a string field;
the runtime clone deliberately retains the clean `(null)` sentinel.

`scripts/test_lm_warp_transition.py` compiles and exercises the transaction
helper with a fake native queue. It checks publication before the send,
scene-2 dispatch, exact flag changes, complete rollback on failure (including
the native pause side effect), and rejection of an oversized flag plan. The
source integration checks cover fresh admission, queued appearance setup,
non-abandoning timeout behavior, and lifecycle records. These are contract
tests, not evidence of a successful in-game transition.

## Runtime limitations and reproduction status

The 0.3.31 Wii report contains a rejected Foyer-to-Storage menu warp and a
Parlor-to-Anteroom menu warp crash; two same-room savestate loads worked in
the reported session. The crash is in the effect renderer at `0x801717E0`,
where a fountain-effect model pointer is null. Native effect initialization
and destruction use a private solid heap and are distinct from the shadow
appearance allocation. The queued handoff fixes a verified transition-path
and stale-readiness defect; it does **not** yet prove whether that null model
was caused by incomplete initialization, exhausted allocation, or an earlier
restored resource state. No null-model skip or blanket guard bypass is used.

A fresh isolated Dolphin profile with an emulated keyboard controller and
the supplied Japanese completed save has been prepared. The attempted
baseline launch did not expose a game window to the available UI tool, so
no additional automated gameplay result is claimed. The next bounded test
requires a visible game window, followed by the same two menu warp routes
first without a savestate load and then after one same-room load. After each
successful warp, using the destination's door and checking the lifecycle
records are necessary; appearing in the destination alone is insufficient.

These checks establish the data and call-site contract. Mansion-wide runtime
coverage still requires Dolphin and Wii testing, including using doors after
each warp and loading states made before and after warps.
