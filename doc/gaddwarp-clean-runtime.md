# Clean GLMJ01 runtime reference

This is an implementation reference for reproducing GaddWarp behavior from
the launcher payload without patching the ISO. It covers both future room
warps and the stock leaves used by the runtime-only practice menu. No warp
code is enabled by this document.

All addresses below are for the verified revision-0 Japanese executable:

- game ID: `GLMJ01`
- clean `main.dol` SHA-1: `722005EA9C1EAB54B114F814734D8F327E5614EE`
- entry point: `0x80003100`
- `_SDA_BASE_` / `r13`: `0x804A0AE0`

The SDA base is proven by the startup instructions at `0x80003200` and
`0x80003204`. Do not transplant the US build's `r13`-relative addresses.

## Event commands and request globals

The event interpreter is `0x80062BD8` (size `0x2548`). Its fixed-width command
name table begins at `0x8032EDAC` and its command jump table at `0x8032FFF0`.
The relevant clean-JP handler bodies are:

| Command | Index | Handler body |
| --- | ---: | ---: |
| `WARP` | 30 | `0x80063AE4` |
| `WARPMOVE` | 91 | `0x80063B50` |
| `SETLUIGI` | 88 | `0x800646C4` |

These are labels inside one large interpreter function, not normal C-callable
functions. They rely on `r31` being the interpreter object and on scratch
buffers in the interpreter's `0x5C8`-byte stack frame. A payload must call the
underlying leaf functions or write the request globals; it must not branch to
these handler labels.

`SETLUIGI(id)` parses the integer and stores it at `r13 + 0x7C4`:

| Meaning | Address | Evidence |
| --- | ---: | --- |
| pending Luigi appearance point | `0x804A12A4` | `stw r3,0x7C4(r13)` at `0x80064724` |
| leaf setter | `0x80061B4C` | `void set(u32 id)` |
| destructive getter | `0x80061B54` | returns the word, clears it to zero |

`WARP(map)` parses the integer and performs these exact writes:

```text
0x80063B44: *(u32 *)0x804A0C20 = 0
0x80063B48: *(u32 *)0x804A0C24 = map
```

The surrounding transition state is:

| Meaning | Address |
| --- | ---: |
| scene/reload request | `0x804A0C20` |
| requested map | `0x804A0C24` |
| main-loop exit flag | `0x804A0C28` |
| normal/hidden mansion mode | `0x804A0C40` (`0` / `1`) |
| current map (`sCurMapNo`) | `0x804A0C48` |
| main-loop mode | `0x80398A40` |
| main-loop pending scene | `0x80398A44` |
| current scene pointer | `0x80498B18` |
| `MissionMode *` | `0x804A17C8` |

`0x8000B378` is the request consumer. At `0x8000B390` it reads
`0x804A0C20`, compares it with `0x80398A44`, and starts the transition on a
mismatch. At `0x8000BEA4-0x8000BEA8` it copies requested map
`0x804A0C24` into current map `0x804A0C48` before rebuilding MissionMode.
MissionMode initialization at `0x800B9254` then opens `/Map/map%d.szp`.

For an injected request, publish the map first and the scene/reload word last
from one normal game-thread callback. This is deliberately the reverse of the
two adjacent retail stores: it prevents the main loop from observing a new
request before its argument has been prepared. CPU data stores require no
instruction-cache operation.

## How `SETLUIGI` is consumed

`EnManager::loadCharacterInfo` at `0x800E2DBC` attaches the map archive's
`CharacterInfo` JMap and calls `0x800E3A1C`. That setup routine:

1. calls the destructive getter at `0x800E3A40`;
2. calls the lookup at `0x800E3B28` for the selected appearance ID;
3. seeds the player appearance slot from the matching row's position and
   retained `ToolData` row reference.

The retail lookup searches for the literal **`luige`**, at `0x8049C958`, and
then compares the row's `appear_point`. The nearby US decomp source currently
spells this `luigi` in one function; clean GLMJ01 machine code uses the same
`luige` string in both lookups.

There is a second lookup at `0x800E3BE8`. Its CodeWarrior ABI is effectively:

```cpp
struct ToolDataRef { void *table; s32 row; };
ToolDataRef *findLuige(ToolDataRef *out, EnManager *self, s32 appearPoint);
```

It returns `{nullptr, -1}` when no row matches. The public convenience wrapper
at `0x800E7CEC` has the ordinary signature:

```cpp
bool resolveLuige(s32 appearPoint, Vec3f *position, u16 *yawOut);
```

It obtains the live EnManager from `*(MissionMode **)0x804A17C8`, resolves the
row, reads `pos_x`, `pos_y`, and `pos_z`, and optionally reads `dir_y`.

### Why clean `SETLUIGI` is insufficient for GaddWarp IDs

Clean map 2 `CharacterInfo` contains 131 total rows and ten `luige` rows,
covering IDs 0 through 9. GaddWarp's patched table contains 244 total rows and
114 `luige` rows. Its added rows provide IDs 10 through 123 with some gaps;
the complete recovered destination table lives in
`doc/gaddwarp-jp2.2-feature-port.md`.

Therefore, storing a GaddWarp ID such as 17 and requesting map 2 is not a
complete clean-ISO port. The clean lookup fails, and the appearance-slot setup
falls back to an invalid/zero entry.

The highest-fidelity future implementation is to present a complete shadow
`CharacterInfo` table to `EnManager::loadCharacterInfo` before `0x800E3A1C`
runs. That preserves the retail spawn path, including the full row reference
used by player construction. Merely hooking `0x800E3B28` is not sufficient:
its return value must still index a real row in the attached `ToolData`.

## Stock post-load placement path (`WARPMOVE`)

The clean `WARPMOVE(id)` handler at `0x80063B50` provides a useful fallback
model. `POS` is command index 31, but it is an `ACTOR` subcommand and its
top-level dispatch lands on the common no-op path at `0x800650F4`; it must not
be confused with this handler. `WARPMOVE`'s core sequence is:

1. call `0x8000826C`, which writes `4` to transition/UI word `0x803A8770`;
2. resolve the `luige` row with `0x800E7CEC(id, &position, &yaw)`;
3. obtain player slot zero with `0x800E7E7C(0)`;
4. call `0x80066FCC(player, 0)`;
5. call `0x80066A44(player, x, y, z)`;
6. store the 16-bit yaw to `player + 0x88` and `player + 0x8A`;
7. set interpreter state `8` and return to the interpreter.

Useful callable signatures are:

```cpp
void *getPlayerMoveObj(s32 slot);                    // 0x800E7E7C
void setPlayerMode(void *player, u32 mode);          // 0x80066FCC
void setPlayerPosition(void *player, f32 x, f32 y,
                       f32 z);                       // 0x80066A44
```

`0x80066FCC` updates `player + 0xC4` and runs the associated reinitialization
only when the value changes. `0x80066A44` writes the new coordinates to the
three position copies at `+0x44`, `+0x50`, and `+0x5C`; it then calls the room
resolver at `0x80017738(position, 0)` and stores the result at `player + 0xB4`.
Consequently, the payload should let this function derive the room rather than
writing a room number by hand. The decoded record's room number is valuable
validation metadata, not an input used by `WARPMOVE`.

There are two important cautions:

- `WARPMOVE` does not check `resolveLuige`'s Boolean result before consuming the
  output. Never invoke the handler or wrapper with an ID absent from the clean
  table.
- `dir_y` is a float field in the map-2 JMap, but `0x800E7CEC` reads it through
  the unsigned-value overload at `0x800BB190`. With the retail field mask this
  resolves to zero. A direct implementation based on the embedded GaddWarp
  table must deliberately convert/validate its yaw rather than assuming this
  wrapper reproduces nonzero patched-row headings.

For IDs 10 and above, the practical fallback is therefore: enter map 2 through
a valid retail appearance point, wait for the replacement MissionMode and
player to stabilize, and execute the callable placement steps with embedded
coordinates. This moves Luigi and recomputes room ownership, but it does not
by itself prove that camera, door-transition, event, or cutscene ownership is
coherent. The shadow-table/retail-spawn approach remains preferable for a
finished warp feature.

## Non-warp practice primitives

These leaves are callable from injected C++ only on the normal game thread and
only after the MissionMode, player, room, and relevant manager have stabilized.
The event handler labels themselves are not callable functions.

### Player HP

`HP(n)` at `0x80063374` parses an `s32`, stores it in the event-command
parameter block at `0x803C8364`, then sends player command 14 through
`0x800ABECC(0, 14, 0x803C831C)`. The nearby words at `0x803C8364` and
`0x803C8368` are command parameters, not live HP.

The live fields are signed 16-bit values:

| Meaning | Player offset |
| --- | ---: |
| current HP | `+0x00FC` (`s16`) |
| maximum HP | `+0x0FFC` (`s16`) |

The safe semantic setter is:

```cpp
void *getPlayer(s32 slot);             // 0x800E7E7C
void setPlayerHp(void *player, s32 hp); // 0x800B7BFC
```

`setPlayerHp` sign-extends its argument to 16 bits, clamps it to zero through
the player's live maximum, stores current HP, and performs the retail HUD,
sound, rumble, heal/damage, and death side effects. Use it rather than a raw
halfword store, and null-check the player returned for slot zero.

### Boo count and requirement buckets

`0x800ABD1C(slot)` gets the player and returns `player + 0x80C`; it does not
null-check before adding that offset. The current Boo count is the unsigned
word at stats `+0x74`, equivalently `Player + 0x880`. `CHECKTELESA` at
`0x80064C54` reads it with `lwz` and compares it unsigned.

The GaddWarp requirement presets use buckets 0--4, 5--19, 20--39, and 40 or
more. Their exact flag recipes are in
`doc/gaddwarp-jp2.2-feature-port.md`. A payload should get and validate the
player first, then read `Player + 0x880` directly.

### Hidden save (`HSAVE`)

The clean `HSAVE` handler at `0x80064C18` performs exactly these writes:

```text
*(u32 *)0x804A0C44 = 5; // main-loop request
*(u32 *)0x804A0C68 = 1; // hidden-save mode
```

This is asynchronous. Main-loop consumer `0x8000CB2C` sees request 5 and, if
busy word `0x804A0C6C` is zero, disables normal presentation/input, plays the
save cue, prepares card state, calls `0x8005452C(mode)`, and sets the busy word
to one. Later frames poll `0x80054688`; completion enters `0x8000CBD8`, which
destroys the save object, restores presentation/audio state, and clears both
the request and busy words.

An injected action must require request == 0 and busy == 0, prepare mode 1,
then publish request 5 once. Reversing the two retail stores is intentional:
publishing the request last prevents a main-loop observer from seeing an old
mode. Never issue it automatically, reissue it while busy, or run it during an
event, transition, DVD/ARAM operation, or another CARD operation.

### BGM

```cpp
void startBgm(u32 id, s32 transition); // 0x801884E8
void stopBgm(s32 mode);                // 0x801885E8
```

`BGM(id)` masks the parsed value to eight bits and calls `startBgm(id, 0)`;
`BGMSTOP` calls `stopBgm(0)`. The start function indexes the table at
`0x80383BA4` without a bounds check. That table is `0xD4` bytes, so only IDs
0 through 52 are valid. GaddWarp has a label for dummy ID 53 but never calls
it. Starting a track should call `startBgm` directly; do not stop first,
because the retail start path already changes the active track.

### Door key state

```cpp
void setDoorKeyState(u32 id, s32 locked); // 0x8001B030
```

`KEYLOCK(id)` masks the ID to `u8` and passes one; `KEYUNLOCK(id)` masks it and
passes zero. The leaf checks the loaded key-table count (`u16` at
`0x804A0D88`), uses the 256-by-two-byte ID map at `0x80399710`, toggles bits
`0x4400` in each of one or two mapped door records, and updates the persistent
door state through `0x80037A08` (lock) or `0x80037A44` (unlock).

Although the map has 256 entries, unused entries are `0xFF` and the function
does not validate the first mapped entry before dereferencing it. Arbitrary
IDs are unsafe. GaddWarp's verified unlock-all whitelist is:

```text
72, 69, 71, 68, 65, 63, 62, 59, 56, 53, 51, 38, 34, 33, 31,
29, 28, 27, 25, 20, 21, 42, 74, 17, 16, 15, 7, 14, 4, 3
```

Call these only after the key table is loaded and its count is nonzero.

### Blackout versus room lighting

The canonical live blackout setter is:

```cpp
void setBlackout(s32 on); // 0x80037498
```

An argument equal to one sets flag 61; other values clear it. The function
then calls `0x800B8598(on)`, which switches the live lighting-object sets and
commits the change. It dereferences the live MissionMode/manager without a
null guard, so use it only in a stable loaded room.

The `LIGHT(n)` event command at `0x8006391C` merely stores a lighting-engine
mode word at `0x80499348`; it is not the blackout toggle.

GaddWarp's menu script follows `TURNOFF`/`TURNON` with a two-frame delay and a
`PLIGHT` call. That sequence must not be copied into a global in-mansion menu:
on map 2, `PLIGHT` mutates room-clear progression as well as presentation.
`setBlackout` already performs the live object swap and is the safe standalone
toggle.

### Current-room clear and reset

```cpp
void markCurrentRoomLit(void);  // 0x800197D8, PLIGHT(1)
void markCurrentRoomDark(void); // 0x8001989C, PLIGHT(0)
void *findActor(const char *name); // 0x800E7F08
void killActorIndex(u32 index);    // 0x800E7EDC
```

Both PLIGHT leaves derive the room from the high byte of
`*(u32 *)(*(u32 *)0x804A0CF8 + 0x0C)` and index the `0x14C`-byte room-state
array at `*(u32 *)0x804A0CF0`. They contain no null or range guards.

`markCurrentRoomLit` no-ops while blackout flag 61 is set. Otherwise it sets
room byte `+6` mask `0x80`, sticky room word `+0x14` mask `0x00000008`, and,
on map 2, the room's persistence-table bit 1. `markCurrentRoomDark` clears byte `+6` bit
`0x80` and the map-2 persistence bit, but it does not clear the sticky word,
recreate actors, or undo room-specific flags. It is therefore not a complete
room reset.

The retail `ACTOR name KILL` path resolves the actor through `findActor`, reads
the actor index at returned object `+0x38`, and calls `killActorIndex`. A
no-patched-assets current-room clear can reproduce a verified GaddWarp recipe
as actor kills, room-specific flag edits, then `markCurrentRoomLit`. There is
no single generic complete clear function. Likewise, complete reset requires
a room reload and room-specific reconstruction; `markCurrentRoomDark` alone
is not sufficient.

`GHOSTRESET` is unrelated to room reset. Its player command 20 zeros 71 words
from `Player + 0x11D8` through `Player + 0x12F0`.

## Clean-runtime support matrix

The flag leaves themselves are ordinary clean-JP functions:

```cpp
u32  getEventFlag(u32 id);   // 0x80065320
void setEventFlag(u32 id);   // 0x80065358
void clearEventFlag(u32 id); // 0x80065398
```

All three mask the ID to eight bits and access the bit array at event-system
base `0x803C7CA0 + 0x659`. Consequently, writing any ID from zero through 255
is memory-safe. It does **not** follow that a GaddWarp feature implemented as
"flag N" exists in the clean game: many of those flags only have meaning
because GaddWarp replaced the scripts or map actors that consume them.

| GaddWarp feature | Clean-ISO classification | Runtime-only recommendation |
| --- | --- | --- |
| HP 1/100 | Exact and safe while the player is stable | Call `0x800B7BFC(player, hp)`; do not write the event parameter block |
| BGM start/stop | Exact and safe while the audio/game state is stable | Call `0x801884E8(id, 0)` for IDs 0--52 or `0x801885E8(0)`; do not pre-stop before start |
| Blackout | Exact live toggle in a stable room | Call `0x80037498(on)` only; do not follow it with `PLIGHT` |
| Hidden/normal mansion | Exact state preset, but progression-affecting | Set/clear flag 35 and update `0x804A0C40`; use explicit save/reload semantics |
| Boo requirement presets | Exact GaddWarp flag recipes, but progression-affecting | Read `Player + 0x880`, then change only flags 2/77/56 for its documented bucket |
| Boneyard plant stage | Exact GaddWarp flag recipe | Change flags 48/78/79; expect the visible actor to reconcile on a room reload rather than assuming an in-place rebuild |
| Boo spawning | Partly stock and progression-affecting | Flags 73/75 are stock Boo-release/tutorial state; flag 22 is GaddWarp-only. The three-flag recipe is reproducible, but should be labelled destructive and may require a reload |
| Fire/observatory/trap-door options | Exact flag recipes, but persistent world state | Flags 54/83/70 may not rebuild already-instantiated doors; apply only from a stable room and treat save as explicit |
| Unlock all doors | Exact with a strict whitelist | Call `0x8001B030(id, 0)` only for the verified IDs and only after the key table is loaded, then set flag 17 if matching GaddWarp |
| Save settings | Exact asynchronous request | Set mode 1 at `0x804A0C68`, then publish request 5 at `0x804A0C44`, once, while idle |
| Current-room clear | Table-driven partial support | Stock actor kills, documented room flags, then `0x800197D8` can reproduce a verified room recipe; it is permanent progression, not cosmetic |
| Current-room reset | No generic clean primitive | Do not expose `0x8001989C` as reset; it does not recreate actors or undo sticky/room-specific state |

### Settings that are not implemented by their flag on a clean disc

| UI label / flag | Clean-versus-patched evidence | Decision |
| --- | --- | --- |
| Perfect RNG / flag 4 | Clean event07 sets flag 4 when the key-ghost demo finishes, while clean events95--100 choose their generator randomly without checking it. GaddWarp replaces event07 and adds flag-4 branches to event03, event29, and events95--100. | Reject the toggle on a clean disc. It does not optimize clean RNG and clearing it corrupts a stock story-completion flag. A faithful port needs native RNG/generator hooks or replacement event logic. |
| Event Skipping / flag 7 | Clean flag 7 is an existing first-time/event-completion marker (events02, 08, 26, and 28). GaddWarp adds checks to dozens of other event files to create its skip paths. | Reject the global toggle. Setting it on clean data cannot add the missing branches; clearing it can replay stock first-time dialogue. |
| Instant Reset / flag 76 | No clean Event script reads or writes flag 76. GaddWarp event08 stores the option and modified event29 checks it after Boo communication, then executes `TITLE`. | Flag 76 alone is inert on the clean disc. A native implementation needs a verified Boo-defeat/end-of-event watcher plus a safe soft-reset request; do not advertise the flag write as working. |
| GBH Warping / flag 42 | Clean event32 uses flag 42 to select the Toad/save-room conversation and sets it after the first interaction. GaddWarp replaces that event and makes modified event77 consume the option for its GBH scan menu. | Reject the flag toggle. Its clean effect is unrelated and can damage Toad/save-room progression. A native overlay/scan implementation is a separate feature. |
| Boo-Fix Warping / flag 22 | No clean Event script references flag 22. GaddWarp events01/08 use it as a saved preference, event13/69 seed it, and event77 plus the added map-2 `luige` rows select alternate placement IDs. | The clean flag is inert. A future runtime warp should store this as mod-local configuration and directly select the alternate coordinate table, not rely on retail flag 22. |
| Elemental-ghost selector / flags 230, 231, 232, 235 | No clean Event script references these flags. Modified event27 sets them and modified room actors/assets create the requested ghost/effect. | Flag writes alone are inert. Reconstruct an actor/generator spawn path before exposing this menu. |
| Element medals / flags 43--45 and Mario items / flags 21, 24, 27, 30, 33 | GaddWarp does not grant these by setting the final flags. Modified event26 warps to added point 61 and reveals added physical pickup actors. The patched DOL also redirects create paths through `0x804B0040`/`0x804B0088`, preserving Player fields `+0x1184/+0x1188` around a specific command-19, parameter-3 case. | Do not substitute flag writes for acquisition. They can advance story gates without producing coherent player inventory. Physical spawning/granting remains asset- and ABI-reconstruction work. |
| Full room reset | GaddWarp event77 combines `PLIGHT(0)`, room-specific flag repairs, alternate added placements, weapon resets, and a map reload. | Not available as a single clean call. Implement only as an asynchronous reload plus a verified per-room reconstruction recipe. |
| Third-person events | GaddWarp has user-facing text but no active Event01 action branch for this setting. | Do not expose it as a working feature without a new implementation. |

The patched-only classifications above are based on behavior, not merely on
whether the flag bit can be stored. A menu may use its own configuration bit
for a future native reimplementation, but persisting the same numbered retail
flag does not import GaddWarp's patched consumers.

## Safe asynchronous request outline

Never request or finish a warp from an interrupt, crash callback, draw hook,
or while the map archive is streaming. Use the ordinary update callback and a
small state machine:

1. Require several consecutive stable Mission frames. At minimum verify
   `0x804A0C20 == 2`, `0x80398A40 == 2`,
   `0x80398A44 == 0x804A0C20`, `0x804A0C28 == 0`, a non-null
   `0x804A17C8`, and the existing LMState DVD/ARAM/CARD idle gate.
2. Reject active door/cutscene/event transitions. The event interpreter lives
   at `0x803C7CA0`; its script cursor is the pointer at `+0x0C`. A null cursor
   is a useful no-script check, but it is not sufficient on its own.
3. Set the appearance point or select the retail-safe fallback, write requested
   map, then publish the reload request.
4. Wait for a new map epoch, `sCurMapNo == 2`, a replacement MissionMode, a
   live player, and stable I/O. Do not retain a player pointer across the map
   rebuild.
5. If using fallback placement, run it once, verify the room derived at
   `player + 0xB4` against the destination metadata, repair/reseed camera and
   transition ownership, then wait for stable frames before returning control.
6. Time out to a visible error. Never continue with an invalid row, stale
   player pointer, or partially initialized MissionMode.

This sequencing is also why the embedded interpreter handlers are unsafe
entry points: parsing a command successfully is not the same as satisfying the
game's asynchronous lifetime constraints.
