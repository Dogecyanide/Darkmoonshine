# GaddWarp JP 2.2 runtime-port reference

This document records the user-facing behavior recovered from GaddWarp JP
2.2 and maps it to a clean-ISO, launcher-injected implementation. It is a
future integration reference. It does not require, redistribute, or recommend
shipping the patched disc image.

The short conclusion is that the ordinary mansion room warps, most settings,
boss/map warps, HP controls, BGM selection, and door/flag presets can be
reimplemented from the xdelta-derived data alone. The custom dojo, physical
item spawns, GBH scan interactions, and complete Boss Rush/Portrait Rush flows
also depend on modified Event/Map/Game assets and cannot be copied faithfully
as a small runtime-only patch without further reconstruction.

## Provenance and confidence

The analysis used these local artifacts:

- GaddWarpJP2.2.xdelta, SHA-256
  DD45D711A4E7F591CD82A5711109B4C6BEAAF1CD1167F4A9A10DC8DA07D0067B.
- Verified clean GLMJ01 revision-0 ISO, SHA-256
  A1B2944B95DD3A40263565775ED029CE60AF6075A9F3947DB0347A7599941DA7.
- Reconstructed patched ISO, SHA-256
  182D26DB2FDB3FE30EFDE513CB324DFF23D961445288937909C60DF1D52B7898.
- Clean main.dol, SHA-1 722005EA9C1EAB54B114F814734D8F327E5614EE.
- Patched main.dol, SHA-1 7A234EBEFCA5FD34A2C52669C142EB1E4D87D143.
- Extracted clean and patched Event, Map, Game, and static archives under
  build/gaddwarp/extracted.
- The lm-decomp source tree and Yasiki GLMJ01 symbol map beside this project.
- The public GaddWarp guide at https://www.speedrun.com/lm/guides/q6jny.

Evidence labels used below:

- Script-confirmed means the action and values are present in an extracted
  event text file.
- Data-confirmed means the value was decoded from a JMap/JMP record.
- DOL-confirmed means the address or change was verified against clean and
  patched executable bytes.
- Inferred means the purpose follows from names and control flow but still
  needs hardware validation.

The broader file and executable inventory is in
doc/gaddwarp-jp2.2-inventory.md. Clean GLMJ01 function internals are recorded
separately in doc/gaddwarp-clean-runtime.md.

## Portability decision table

| Feature family | Runtime-only feasibility | Main dependency or risk |
| --- | --- | --- |
| Room, hallway, and OoB warps | Implementable now | Embed the recovered placement table; clean map2 does not contain points 10 through 123 |
| Boss and stock-map warps | Implementable now | Apply the exact prerequisite flag preset before requesting the map |
| Mansion mode and verified standalone settings | Implementable now | Keep changes session-only until an explicit card save |
| HP, BGM, blackout, and verified door operations | Implementable now | Call revision-locked clean GLMJ01 leaves; do not expose patched-event flags as generic toggles |
| Plant and Boo requirement presets | Implementable now | Expose names that make destructive progress changes obvious |
| Clear/reset all rooms | Mostly reconstructable | Long-running sequence needs safe room-load waits and per-room validation |
| Medal and Mario-item grant | Not yet faithful | GaddWarp spawns physical pickups and adds DOL wrappers preserving two player fields |
| GBH scan reset/heal/warp interface | Partial | Behavior is in modified event scripts and room assets; a native overlay replacement is possible |
| BGM machine | Implementable now | UI only; direct stock BGM calls are known |
| Weirdboo Dojo | Asset-dependent | Modified map3 generators, paths, actors, and Game parameters are required |
| Boss Rush and Portrait Rush | Partial | Presets are known, but the full chains use modified event90/101 and new events102-109 |
| Branding/title presentation | Optional asset work | Patched res_titl.szp and ninlogo.pcm; not needed for functionality |

## Clean-ISO warp design

GaddWarp's Event01 room menu does not warp to separate map files. Every normal
mansion destination selects a Luigi appearance point and requests map 2. The
Foyer entry is the special case: it only requests map 2 and does not issue a
SETLUIGI command.

On clean GLMJ01:

- Event command interpreter: 0x80062BD8.
- WARP handler: 0x80063AE4.
- SETLUIGI handler: 0x800646C4.
- WARP globals: 0x804A0C20 and 0x804A0C24.
- SETLUIGI global: 0x804A12A4.
- Mansion-mode global: 0x804A0C40.
- _SDA_BASE_: 0x804A0AE0.

The patched map2 CharacterInfo has 244 records; clean has 131. GaddWarp adds
113 records, including the named luige placement points used by its menus.
The clean disc therefore cannot resolve most GaddWarp SETLUIGI IDs by itself.
Writing ID 10 or later to 0x804A12A4 without supplying its corresponding data
is not a complete port.

Two runtime-only approaches are viable:

1. Preferred fidelity approach: intercept the clean luige-point lookup during
   map loading and satisfy IDs 10 through 123 from the embedded table below.
   Then use the stock SETLUIGI followed by WARP(2) path. This retains the
   game's own spawn timing and should initialize camera/room ownership most
   faithfully. The clean lookup helper that returns position and yaw is
   0x800E7CEC, but the earlier map-load lookup path must also be covered; do
   not assume one hook catches both consumers until verified.
2. Simpler approach: enter map2 through a known retail-safe point, wait until
   the map and player are live, and perform the stock WARPMOVE placement steps
   using the embedded coordinates. This is enough to prototype the menu, but
   must explicitly reinitialize camera/room/door state to avoid a correct
   Luigi position with stale camera or transition ownership.

The clean WARPMOVE handler performs this core sequence:

1. Resolve a luige point with 0x800E7CEC(pointId, &position, &yaw).
2. Obtain the player with 0x800E7E7C(0).
3. Call 0x80066FCC(player, 0).
4. Call 0x80066A44(player, x, y, z), passing the coordinates as floats.
5. Store the returned 16-bit yaw in player fields +0x88 and +0x8A.

The room number in the table is useful validation metadata; WARPMOVE itself
uses position and yaw. The room loader determines the active room around the
new position.

A robust asynchronous warp state machine should reject active cutscenes and
door transitions, close/freeze the injected menu, set the requested entry,
request map2, wait for a new stable map epoch plus a live player, apply or
verify placement, reset the camera/door transition state, and only then return
control. It should time out to a visible error instead of writing into a stale
player object.

## Event01 room menu

The alternate column is selected when flag 22 (Boo-Fix Warping) is on. A dash
means the script always uses the standard point. All entries are
script-confirmed.

| Category | Menu label | Standard point | Flag-22 point | Destination room_no |
| --- | --- | ---: | ---: | ---: |
| Area 1 | Foyer | none; WARP(2) only | - | - |
| Area 1 | Parlor | 10 | 76 | 35 |
| Area 1 | Anteroom | 11 | 77 | 39 |
| Area 1 | Wardrobe | 12 | 78 | 38 |
| Area 1 | 2F Balcony | 58 | - | 37 |
| Area 1 | Study | 13 | 79 | 34 |
| Area 1 | Master Bedroom | 14 | 80 | 33 |
| Area 1 | Nursery | 15 | 81 | 24 |
| Area 2 | 1F Bathroom | 16 | - | 20 |
| Area 2 | Ballroom | 17 | 122 | 10 |
| Area 2 | Storage | 18 | 83 | 14 |
| Area 2 | 1F Washroom | 19 | - | 17 |
| Area 2 | Fortune-teller's Room | 20 | 85 | 3 |
| Area 2 | Mirror Room | 21 | 86 | 4 |
| Area 2 | Laundry Room | 22 | 87 | 5 |
| Area 2 | Butler's Room | 23 | 88 | 0 |
| Area 2 | Hidden Room | 24 | 89 | 1 |
| Area 2 | Conservatory | 25 | 90 | 21 |
| Area 2 | Dining Room | 26 | 91 | 9 |
| Area 2 | Kitchen | 27 | 92 | 8 |
| Area 2 | Boneyard | 28 | - | 11 |
| Area 2 | Graveyard | 29 | - | 16 |
| Area 3 | Courtyard | 30 | - | 23 |
| Area 3 | Bottom of the Well | 31 | - | 69 |
| Area 3 | Rec Room | 32 | 97 | 22 |
| Area 3 | Tea Room | 33 | 98 | 47 |
| Area 3 | Astral Hall | 43 | 109 | 40 |
| Area 3 | Observatory | 34 | - | 41 |
| Area 3 | 2F Bathroom | 35 | - | 45 |
| Area 3 | 2F Washroom | 36 | - | 42 |
| Area 3 | Nana's Room | 37 | 102 | 46 |
| Area 3 | Twins' Room | 38 | 103 | 25 |
| Area 3 | Billiards Room | 39 | 104 | 12 |
| Area 3 | Projection Room | 40 | 106 | 13 |
| Area 3 | Safari Room | 41 | 107 | 52 |
| Area 3 | 3F Balcony | 42 | - | 59 |
| Area 4 | Telephone Room | 44 | 110 | 50 |
| Area 4 | Breaker Room | 45 | 111 | 67 |
| Area 4 | Cellar | 46 | 112 | 63 |
| Area 4 | Clockwork Room | 47 | 113 | 56 |
| Area 4 | Roof | 123 | - | 56 |
| Area 4 | Armory | 48 | 114 | 48 |
| Area 4 | Ceramics Studio | 52 | 118 | 55 |
| Area 4 | Sealed Room | 7 | - | 36 |
| Area 4 | Sitting Room | 53 | 119 | 27 |
| Area 4 | Guest Room | 54 | 120 | 28 |
| Area 4 | Pipe Room | 49 | 115 | 66 |
| Area 4 | Cold Storage | 50 | 116 | 61 |
| Area 4 | Artist's Studio | 51 | 117 | 57 |
| Area 4 | Secret Altar | 55 | - | 70 |

Secret Altar additionally sets flag 34 before warping.

### Hallway and OoB entries

| Menu label | Point | room_no | Extra action |
| --- | ---: | ---: | --- |
| 2F Foyer hallway | 64 | 30 | none |
| 1F Foyer hallway | 65 | 6 | none |
| Laundry hallway | 66 | 6 | none |
| Ballroom hallway | 67 | 7 | none |
| Rec Room hallway | 68 | 53 | none |
| Tea Room hallway | 69 | 19 | none |
| Nana's Room hallway | 70 | 43 | none |
| Safari Room hallway | 71 | 44 | none |
| Telephone Room hallway | 72 | 51 | none |
| Cellar hallway | 73 | 49 | none |
| Bathroom hallway | 75 | 62 | none |
| Nursery chest clip | 56 | 24 | set flag 46 |
| Basement walk | 57 | 6 | none |

Point 74 exists but is not exposed by this menu. Message labels for first and
second skew also exist, but no active Event01 branch targets them.

## Event01 settings and presets

| UI action | Exact action | Port notes |
| --- | --- | --- |
| Hidden Mansion | clear flag 35; set mansion global to 1 through URALUIGI; HSAVE | Implementable |
| Normal Mansion | set flag 35; set mansion global to 0 through OMOTELUIGI; HSAVE | Implementable |
| Perfect RNG on/off | set/clear flag 4; HSAVE | Patched-event-only; clean Event07 uses flag 4 as story progress |
| Boo Spawning on | set flags 22, 73, and 75; HSAVE | Also selects alternate room entry points |
| Boo Spawning off | clear flags 22, 73, and 75; HSAVE | Implementable |
| Blackout on/off | patched script uses TURNOFF/TURNON plus PLIGHT; HSAVE | Clean global menu must call 0x80037498 only; generic PLIGHT also changes room-clear state |
| Event skipping on/off | set/clear flag 7; HSAVE | Patched-event-only; 58 modified events consume this toggle and clean flag 7 is story state |
| HP 100 / HP 1 | issue HP(100) or HP(1); HSAVE | Current HP only; no HPMAX command used |
| Fire doors on/off | on clears flag 54; off sets flag 54; HSAVE | UI semantics are inverted relative to bit |
| Observatory door on/off | on clears flag 83; off sets flag 83; HSAVE | Same inversion |
| Room traps/spike doors on/off | on clears flag 70; off sets flag 70; HSAVE | Same inversion |

At Event01 startup the script clears flags 53, 16, 71, and 34, then
synchronizes the mansion global from flag 35. A runtime menu should not repeat
that startup cleanup merely because the overlay was opened; those flags are
also used by advanced flows.

The UI contains a third-person-on message, but no exposed branch in Event01
selects it. Do not present it as a working GaddWarp setting without a separate
implementation.

### Boo requirement presets

The menu changes required-Boo progression flags according to the current Boo
count:

| Current Boo count | Requirements off | Requirements on |
| --- | --- | --- |
| 0 through 4 | set flags 2, 77, 56 | clear flags 2, 77, 56 |
| 5 through 19 | set flags 2, 77 | clear flags 2, 77 |
| 20 through 39 | set flag 77 | clear flag 77 |
| 40 or more | no toggle offered | no toggle offered |

### Boneyard plant presets

The area checks use boss/story flags 39, 68, and 82. The selected plant state
is encoded by flags 48, 78, and 79 in that order:

| Context | UI state | 48/78/79 |
| --- | --- | --- |
| Area 2 | Seed | 0/0/0 |
| Area 2 | Sprout | 0/1/0 |
| Area 3 | Flower | 0/1/1 |
| Area 3 | Bud | 0/1/0 |
| Area 3 | Seed | 0/0/0 |
| Area 3 | Sprout | 0/0/1 |
| Area 4 | Opened | 1/1/1 |
| Area 4 | Closed | 0/1/1 |
| Area 4 | Dead | 0/1/0 |
| Area 4 | Seed | 0/0/0 |
| Area 4 | Sprout | 1/0/0 |

## Boss, map, and special warps

| Selection | Flag preset | Destination |
| --- | --- | --- |
| Chauncey | clear 39, 46, 222 | map 10 |
| Bogmire | clear 67, 68, 222 | map 13 |
| Boolossus | require medal flag 45; clear 81, 82, 222 | map 11 |
| King Boo | clear 66 and 222 | map 9 |
| Boss Rush | require flag 45; clear 39,46,67,68,81,82,66,222; set 42 and 3 | map 10 |
| Portrait Rush | require 43,44,45; clear boss flags; set 58 and 53 | point 6 then map 2; modified event26 continues setup |

Stock/special map selections recovered from Event01:

- Test Map: map 0.
- Training Room: map 3. First Time clears 16 and 71; Normal sets 16 and
  clears 71; No Event sets 3 and clears 71.
- Weirdboo Dojo: clear 84,34,53,66,39,67,82,81,68,46,62,60; set 71; map 3.
- Gallery variants: map 5 (short halls), map 7 (long halls), map 8 (E.Gadd),
  and map 6 (retail gallery).
- Portrificationizer after boss: map 4. Normal leaves the standard flow;
  No Event sets 16; 3rd Person sets 71.
- End game: map 12. Event sets 53; No Event sets 16; 3rd Person sets 71.
- Credits/final-rank selection uses the stock ENDING command.
- Money/Ghost Total uses the stock ACCOUNT command.
- A Fortune-Teller warp label is present but has no active branch.

The event01 Advanced entry sets flag 53, clears 84, selects point 6, and
warps to map 2, where modified event26 provides its interface.

## Active features outside Event01

### Event08: E.Gadd panel

This is not dead text; it contains active settings and original E.Gadd route
choices.

| Action | Exact behavior |
| --- | --- |
| Instant Reset on/off | set/clear flag 76; HSAVE; described as instant soft reset after defeating a Boo |
| GBH Warping on | clear flag 42; HSAVE |
| GBH Warping off | set flag 42; HSAVE |
| Boo-Fix Warping on/off | set/clear flag 22; HSAVE |
| Training | set flag 16; map 3 |
| Normal mansion opening | OMOTELUIGI then OPENING |
| Gallery | map 6 |
| Hidden mansion opening | URALUIGI then OPENING |
| About/Credits | user-facing informational pages |

GBH Warping is another inverted UI bit. Reset Settings text exists but has no
active action branch. Event08 startup clears 53, 16, 71, and 34; its blackout
path sets flag 53 and applies lighting.

These flags are not all standalone clean-runtime features. Flag 76 is consumed
by GaddWarp's modified Event29, while its clean counterpart has no instant-reset
branch. GBH and Boo-Fix switches select behavior in patched warp/event flows;
without those consumers, exposing their bits either does nothing or changes
unrelated stock progress. They therefore remain out of the clean-ISO menu until
their native replacement behavior exists.

### Event26: Advanced

The active menu includes item spawning, all-doors unlock, room clear/reset,
and Portrait Rush variants.

Physical elemental-medal spawning checks flags 43, 44, and 45. It clears or
sets staging flags, uses point 61 at (0,0,0), then returns via point 6 and
HSAVE. Mario-item spawning checks flags 21, 24, 27, 30, and 33 and similarly
uses modified actors and point 61. These should not be reduced to flag writes
without confirming player inventory state: the patched DOL wraps a retail
spawn/create path specifically to preserve player fields +0x1184 and +0x1188.

All Doors Unlocked calls KEYUNLOCK for these IDs, then sets flag 17 and saves:

72, 69, 71, 68, 65, 63, 62, 59, 56, 53, 51, 38, 34, 33, 31, 29, 28,
27, 25, 20, 21, 42, 74, 17, 16, 15, 7, 14, 4, 3.

Clear/reset room traversal visits these placement points in order, alternating
PLIGHT state and WARPMOVE around the per-room edits:

10, 11, 12, 58, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
27, 28, 30, 31, 32, 33, 43, 34, 35, 36, 37, 38, 39, 40, 41, 44, 45,
46, 47, 3, 48, 52, 7, 53, 54, 49, 50, 51.

Additional clear-room repairs:

- Parlor: set flags 8 and 14.
- Astral Hall: set 83 and 6.
- Observatory: set 49, 50, and 52.
- Twins' Room: set 51.
- Ceramics Studio: set 40.
- Guest Room: set 18, 38, 36, and 25.
- Artist's Studio: temporarily use 144, then set 31, 177 through 183, and 55;
  finally clear 144.

Additional reset-room repairs:

- Parlor: clear 14 and 8.
- Storage Room: clear 41 and 19.
- Fortune-teller's Room: clear 13; temporarily map collected Mario-item flags
  20/23/26/29/32 to 21/24/27/30/33.
- Astral Hall: clear 83 and 6.
- Observatory: clear 49, 50, and 52.
- Twins' Room: clear 51 and 37.
- Ceramics Studio: clear 40.
- Guest Room: clear 25, 18, 38, and 36.
- Pipe Room: clear 63 and 64.
- Cold Storage: clear 28.
- Artist's Studio: temporarily use 144, then clear 31, 177 through 183, and
  55; finally clear 144.

Both sequences HSAVE after completion. A runtime port should implement these
as an asynchronous operation with a stable-room wait between each entry, not
as one frame of writes.

Portrait Rush variants use flags 84 and 34:

| Variant | Flag 84 | Flag 34 |
| --- | --- | --- |
| All portraits | clear | clear |
| No optional portraits | set | clear |
| No bosses | clear | set |
| Neither optional nor bosses | set | set |

The setup then sets 58, 42, 3, and 86; clears 89, 232, 236, and 53; and enters
the modified portrait chain. That final chain is asset-dependent.

### Event27: elemental-ghost selector

The active menu offers Fire, Water, Ice, and Exit. Fire requires medal flag
43, Water 44, and Ice 45. Before selection it clears 230, 231, 232, and 235.
Fire sets 232; Water sets 231; Ice sets 230 and 235. The physical ghost/effect
creation uses modified assets, so these flags are a preset description, not a
complete runtime implementation.

### Event55: BGM machine

The right-side lab computer exposes the following stock BGM IDs. Direct clean
calls are 0x801884E8(id, 0) to start and 0x801885E8(0) to stop.

| ID | Label | ID | Label |
| ---: | --- | ---: | --- |
| 0 | Key Ghost | 27 | Saving Mario |
| 1 | E.Gadd Debut | 28 | Silence |
| 2 | SMB1 Water | 29 | End Game |
| 3 | SMB3 Land | 30 | Portrait Ghost |
| 4 | Unused Piano | 31 | Ghost Minigame |
| 5 | Parlor Paintings | 32 | Ghost Battle |
| 6 | Telescope | 33 | Chauncey Plays |
| 7 | Dark Room | 34 | GBH Ringtone |
| 8 | Training | 35 | GBH |
| 9 | Chauncey Door | 36 | Sliding Wall |
| 10 | Foyer Debut | 37 | Water Shutoff |
| 11 | Toad | 38 | Candle Pentagram |
| 12 | Lights On | 39 | Mario Painting |
| 13 | Training Interrupt | 40 | Well Cutscene |
| 14 | Training Results | 41 | Drum Beat |
| 15 | Chauncey Intro | 42 | Well Cutscene 2 |
| 16 | Chauncey Battle | 43 | Sue Pea Debut |
| 17 | Tombstone Glow | 44 | Boo Warping |
| 18 | Bogmire Intro | 45 | Releasing Boos |
| 19 | Bogmire Battle | 46 | Boo Theme |
| 20 | Boolossus Intro | 47 | 3F Boo Check |
| 21 | SMB1 Jingle | 48 | B1 Boo Check |
| 22 | Boolossus Battle | 49 | King Boo |
| 23 | Melody Battle | 50 | Bowser Intro |
| 24 | dummy | 51 | Bowser Battle |
| 25 | Portrification | 52 | Bowser Inhales |
| 26 | Boss Clear | 53 | dummy |

The script's normal menu treats the dummy entries as non-content.

### Event77: GBH scan practice actions

Modified GBH scans provide these user-visible actions:

- Scan the ground to reset the current room.
- Scan selected furniture to heal Luigi or return to the lab.
- Scan the ceiling to clear the current room.
- In the Butler sequence, choose Reset Sequence or Just Room.
- At bosses, reset the battle; at Boolossus, advance the wave.

The implementation performs room-specific actor, flag, light, and WARPMOVE
operations. Exact scan hotspots and several actor resets live in modified
Event/Map/Room data. The equivalent clean-ISO feature should be a payload menu
action keyed by current room rather than attempting to retain patched scan
hotspots.

### Event10: Weirdboo Dojo

The active dojo interface contains:

- Random Wave with Easy, Normal, Hard, and Extreme difficulty.
- Custom Wave 1 through 7, each selecting up to twelve ghost types: Gold,
  Purple Puncher, Blue Twirler, White Grabber, Red Grabber, Invisible Grabber,
  Garbage Can, Red/Green Ghost Guy, Skeleton, Purple Bomber, and Bowling.
- Optional ghost preview.
- Rhythm Practice.
- Minigames: Textbox Mashing, Endless Random without healing, Rhythm Ghosts,
  and Pearl Dupe.
- Generator toggles for mice, flying fish, and sparks.
- Normal/Hidden mode, HP 100/1, and a smaller BGM selector.

This is not portable from event logic alone. It depends on modified map3
generator/path/actor data and custom Game parameters iyapoo16 through
iyapoo20. Treat it as a separate reconstruction project.

### Event90, Event101, and Event102 through Event109

These implement Boss Rush/Portrait Rush progression, replay prompts,
Portrificationizer/title/credits exits, and newly added chain stages. The
entry presets are documented above, but full fidelity requires reconstructing
the modified and new event assets. A payload-native state machine can replace
them later, but the xdelta alone does not turn them into a small reusable C
module.

## Clean GLMJ01 primitives available to a payload

| Operation | Address/data | Notes |
| --- | --- | --- |
| Get flag | 0x80065320(flag) | Exact clean JP function |
| Set flag | 0x80065358(flag) | Exact clean JP function |
| Clear flag | 0x80065398(flag) | Exact clean JP function |
| Start BGM | 0x801884E8(id, 0) | Called by the event interpreter |
| Stop BGM | 0x801885E8(0) | Called by the event interpreter |
| Lock/unlock key | 0x8001B030(keyId, 1/0) | Validate semantics per door |
| Player HP setter | 0x800B7BFC(player, hp) | Live current/max are signed halfwords at Player+0xFC/+0xFFC; 0x803C8364/68 are interpreter parameters, not HP storage |
| Light global | 0x80499348 | Event LIGHT command target |
| Mansion mode | 0x804A0C40 | 1 hidden, 0 normal |
| Save request | 0x804A0C44 = 5 | HSAVE also selects mode at 0x804A0C68 |

Relevant command-handler entry points inside 0x80062BD8 are HP 0x80063374,
HPMAX 0x800633EC, FLAGON 0x80063550, FLAGOFF 0x80063590, BGM 0x800637D0,
BGMSTOP 0x80063838, KEYLOCK 0x80063844, KEYUNLOCK 0x800638AC, LIGHT
0x8006391C, PLIGHT 0x80063984, WARP 0x80063AE4, WARPMOVE 0x80063B50,
ACCOUNT 0x800646BC, SETLUIGI 0x800646C4, HSAVE 0x80064C18, GALLERY
0x80064CE8, URALUIGI 0x800650D0, and OMOTELUIGI 0x800650DC.

Calling handlers in the middle of the interpreter is usually less robust than
calling the underlying functions or writing the documented request globals.

## Executable-only behavior that must not be copied blindly

GaddWarp changes 42 existing instruction words. Seventeen calls to the retail
0x800AD814 path are redirected through 0x804B0088, and two calls to
0x800ABF2C are redirected through 0x804B0040. The wrappers preserve player
fields +0x1184 and +0x1188 for a particular create/spawn case. This is likely
part of reliable item/element granting and is why physical pickups must be
treated as unresolved.

Other direct patches redirect several startup flag IDs to 210, recolor UI,
alter clock constants, change global-mode branches, and redirect flag 194 to
60. Their purpose is tied to custom event flow. None is required merely to
show a runtime menu or perform an ordinary room warp, and the clock changes in
particular should not be copied without a named feature and a hardware test.

The injected GaddWarp executable ranges around 0x804B0000 and loader at
0x8038B840 collide conceptually with Moonshine's own injection. Never chain
the xdelta loader with the launcher payload.

## Recovered luige placement data

The following CSV is machine-readable. It contains every luige point present
in patched map2 CharacterInfo, including retail IDs 0 through 9 and all added
GaddWarp IDs. Values are stored as float x/y/z and float dir_y in the JMap;
the lookup returns dir_y as a 16-bit yaw. The room_no field is metadata for
validation.

Missing IDs are 82, 84, 93, 94, 95, 96, 99, 105, 108, and 121. They have no
luige record and must not be requested.

~~~csv
id,room_no,x,y,z,yaw,role
0,2,-7.640748,0,145.1743,180,retail point
1,24,-3365.479,550,-443.7272,0,retail point
2,16,-3371.456,41.72185,-5610.0908,0,retail point
3,60,-10.8246,1803.855,-2086.915,180,retail point modified by GaddWarp
4,56,-5.10219,1127.127,-2318.781,0,retail point modified by GaddWarp
5,59,-60.97363,1100,-4497.741,0,retail point modified by GaddWarp
6,2,-5.840569,0,53.580879,0,retail point
7,36,1910,550,-2770,0,sealed room
8,48,-1880,1099.98,-210,0,retail point
9,70,2310.6001,-550.4,-6723.5,0,retail point
10,35,0,550,-1360,180,parlor
11,39,625,550,-2910,180,anteroom
12,38,-1125,550,-3360,-90,wardrobe
13,34,-1435,550,-1360,180,study
14,33,-3703,550,-1360,180,master bedroom
15,24,-3005,550,-610,0,nursery
16,20,-1625,0,-4950,-90,1F bathroom
17,10,1675,0,-1685,90,ballroom
18,14,3785,0,-2910,180,storage
19,17,-1625,0,-4165,-90,1F washroom
20,3,2050,0,-610,0,fortune teller
21,4,2675,0,-210,90,mirror room
22,5,-2675,0,-1010,-90,laundry
23,0,-3150,0,-610,0,butler
24,1,-1990,0,-432,-90,hidden room
25,21,-525,0,-4510,180,conservatory
26,9,925,0,-1810,-90,dining
27,8,-1875,0,-1810,-90,kitchen
28,11,-3800,0,-2410,180,boneyard
29,16,-3125,0,-4336,-90,graveyard
30,23,-1250,0,-5560,180,courtyard
31,69,685,-550,-5910,90,bottom well
32,22,3800,0,-5360,0,rec room
33,47,2780,550,-4510,180,tea room
34,41,3225,550,-3360,90,observatory
35,45,-1625,550,-4940,-90,2F bathroom
36,42,-1625,550,-4160,-90,2F washroom
37,46,-875,550,-4940,90,nana
38,25,-2190,550,-610,0,twins
39,12,-930,0,-3760,0,billiards
40,13,-425,0,-3220,90,projection
41,52,2400,1100,-610,0,safari
42,59,1800,1100,-2935,180,3F balcony
43,40,1675,550,-3360,90,astral
44,50,-1425,1100,-210,90,telephone
45,67,2900,-550,-1360,180,breaker
46,63,2900,-550,-610,0,cellar
47,56,1150,1100,-1360,180,clockwork
48,48,-2175,1100,-210,-90,armory
49,66,1925,-550,-1810,-90,pipe
50,61,1825,-550,-210,-90,cold storage
51,57,2175,1100,-1810,90,artist
52,55,-2400,1100,-1360,180,ceramics
53,27,2250,550,-610,0,sitting
54,28,2675,550,-210,90,guest
55,70,2300,-550,-5560,180,secret altar
56,24,-3230,550,-650,180,nursery chest clip
57,6,2325,550,-4440,30,basement walk
58,37,-2675,550,-3160,-90,2F balcony
59,7,-1250,0,-5160,180,internal custom point
60,14,3280,0,-2953,-30,internal custom point
61,2,0,0,0,180,advanced item staging
62,30,0,550,-773,180,internal custom point
63,6,1012.528,0,-4132.167,-90,GBH internal point
64,30,-925,550,-1010,90,2F foyer hallway
65,6,0,0,-810,180,1F foyer hallway
66,6,-2475,0,-1010,90,laundry hallway
67,7,-1425,0,-4950,90,ballroom hallway
68,53,1475,0,-1685,-90,rec hallway
69,19,2370,0,-4165,90,tea hallway
70,43,2170,550,-4165,-90,nana hallway
71,44,-1075,550,-4940,-90,safari hallway
72,51,1975,1150,-210,-90,telephone hallway
73,49,-1625,1150,-210,-90,cellar hallway
74,65,2475,0,-1010,90,unused hallway point
75,62,2475,-550,-210,-90,bathroom hallway
76,35,-150,550,-1160,180,parlor Boo-fix
77,39,625,550,-2710,180,anteroom Boo-fix
78,38,-925,550,-3360,-90,wardrobe Boo-fix
79,34,-1435,550,-1160,180,study Boo-fix
80,33,-3703,550,-1160,180,master bedroom Boo-fix
81,24,-3005,550,-810,0,nursery Boo-fix
83,14,3785,0,-2710,180,storage Boo-fix
85,3,2050,0,-810,0,fortune teller Boo-fix
86,4,2475,0,-210,90,mirror Boo-fix
87,5,-2475,0,-1010,-90,laundry Boo-fix
88,0,-3150,0,-810,0,butler Boo-fix
89,1,-2850,0,-432,90,hidden room Boo-fix
90,21,-525,0,-4310,180,conservatory Boo-fix
91,9,1125,0,-1810,-90,dining Boo-fix
92,8,-1675,0,-1810,-90,kitchen Boo-fix
97,22,3800,0,-5560,0,rec room Boo-fix
98,47,2780,550,-4310,180,tea room Boo-fix
100,45,-1425,550,-4940,-90,unused 2F bathroom alternate
101,42,-1425,550,-4160,-90,unused 2F washroom alternate
102,46,-1075,550,-4940,90,nana Boo-fix
103,25,-2190,550,-810,0,twins Boo-fix
104,12,-930,0,-3960,0,billiards Boo-fix
106,13,-625,0,-3220,90,projection Boo-fix
107,52,2400,1100,-810,0,safari Boo-fix
109,40,1475,550,-3360,90,astral Boo-fix
110,50,-1625,1100,-210,90,telephone Boo-fix
111,67,2900,-550,-1160,180,breaker Boo-fix
112,63,2900,-550,-810,0,cellar Boo-fix
113,56,1150,1100,-1160,180,clockwork Boo-fix
114,48,-1975,1100,-210,-90,armory Boo-fix
115,66,2125,-550,-1810,-90,pipe Boo-fix
116,61,2100,-550,-210,-90,cold storage Boo-fix
117,57,1975,1100,-1810,90,artist Boo-fix
118,55,-2400,1100,-1160,180,ceramics Boo-fix
119,27,2250,550,-810,0,sitting Boo-fix
120,28,2475,550,-210,90,guest Boo-fix
122,10,1475,0,-1685,90,ballroom Boo-fix
123,56,-5,1141,-2499,0,roof
~~~

Clean-versus-patched caveat for retail IDs: point 3 gains yaw 180 in the
patched data; point 4 changes z from -2314.781 to -2318.781; point 5 changes x
from -60.91113 to -60.97363. The CSV intentionally records GaddWarp's patched
values because this is a GaddWarp behavior reference.

## Recommended implementation order

1. Add a payload-native menu shell and room table; implement only Foyer plus
   a few representative points on different floors while the transition
   state machine is hardened.
2. Expand to all standard, alternate, hallway, and OoB points. Validate room,
   camera, door re-entry, Boo/event behavior, and Hidden Mansion on hardware.
3. Add reversible session-only settings first: HP, BGM, blackout, verified
   door switches, Boo requirements, plants, and mansion mode. Keep Perfect RNG,
   event skip, Instant Reset, GBH Warp, and Boo-Fix out until their patched
   consumers have native runtime replacements. Add explicit Save Settings only
   after users confirm desired memory-card behavior.
4. Add boss/map presets and plant/Boo-requirement presets with warning text.
5. Implement current-room clear/reset as native per-room recipes rather than
   replaying Event26's long scripted walk.
6. Treat physical item spawning, dojo, GBH hotspots, and rush modes as separate
   projects that require actor/resource reconstruction and their own test
   plans.

Every build should remain a clean-ISO launcher injection. GaddWarp's xdelta is
an evidence source, not a runtime dependency.
