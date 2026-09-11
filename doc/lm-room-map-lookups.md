# Native room/map lookup coverage

The .43 multi-warp crash identifies the fixed FurnitureInfo cache at
`803C236C`, immediately after the RoomInfo range added in .43. The bounded
adjacent-owner audit also found two omitted scalar caches. These are not
additional explanations for that crash: they keep the native room/map view
consistent with restored GAME and RoomInfo data.

All addresses below are for the authenticated retail Japanese GLMJ01 DOL,
SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee`. No retail asset or DOL is
distributed by these tests.

## Per-room byte cache: 803C2468–803C24E8

This is a 128-byte scalar cache, not another ToolData pointer or a live
hardware object. Native scene setup `8002E654` keeps RoomInfo's fixed root
`803C2138` in r31. When the native mode is 2, `8002E7E0` selects
`r31 + 330 = 803C2468`. The loop:

- Reads the room descriptor array at `804A0CF0` and room count at `804A0CF4`.
- Calls `80018AC8`, which looks up a RoomInfo field through `8002D484`.
- Stores the returned byte at `8002E7FC`, advances the room descriptor by
  `14C`, and advances the byte-cache cursor by one.

The map-classification loop `80029E74–80029F74` reads this cache alongside
the already-captured room mask at `803C2E10`. It produces map-room display
classifications rather than following pointers from the cache.

The JP symbol boundary is exactly 128 bytes. The next object at `803C24E8`
is the already-captured VR table. The preceding `803C236C–803C2468` range is
FurnitureInfo and has its own native ownership proof and capture record.

## Map scalar state: 803C1C98–803C20C8

The 1072-byte fixed block contains room classifications and map positions:

| Offset | Native use |
| --- | --- |
| `000–3FF` | Four-byte room-display records: floor, room ID, and classification bytes. |
| `400–40B` | Map reference position, initialized from native float vectors. |
| `40C–417` | Current map/camera position. |
| `418–423` | Map target position. |
| `424–42F` | Calculated/interpolated position. |

`80029E90` selects this root. Writes at `80029EBC`, `80029EC8`, and
`80029EF4/80029F54` populate individual room bytes, advancing by four at
`80029F60`. The renderer at `80029A68–80029AEC` reads those same bytes and
uses them to select floor/room presentation.

The tail's type is established by native float loads/stores, not by assuming
that arbitrary BSS words are safe: `8002AD38–8002AD4C` initializes the first
vector, `8002A104/8002A10C/8002A114` writes the second,
`8002A09C–8002A0B0` writes the third, and
`8002A4F4/8002A50C/8002A518` writes the fourth through offset `42C`.
There are no embedded resident pointers, destructor links, or OS queues in
this proven layout. The block ends exactly where the already-captured
event-active bytes begin at `803C20C8`.

## Scope and verification

The three additions (FurnitureInfo, per-room bytes, map scalar state) are
separate exact capture records in snapshot format 27. Together with existing
ranges they close `803C1C60–803C26C8` without widening capture into unrelated
BSS. A scan of all 21 direct calls to native `getJmpResource` found RoomInfo
and FurnitureInfo as the fixed persistent descriptor roots; other inspected
uses were temporary lookups or GAME-owned objects. This bounded scan is not
a claim that all game state has been exhaustively audited.

`scripts/test_lm_room_map_lookup.py` authenticates the retail instructions,
checks the exact scalar boundaries and single capture of each cache, and
checks that the shared capture/restore/writeback loops cover them. It also
keeps the live DVD request at `803C8460` and boot render owner at `803989E0`
excluded. It intentionally does not hard-code the whole snapshot's offsets.
These are native-data and source-integration tests, not Wii runtime acceptance.
