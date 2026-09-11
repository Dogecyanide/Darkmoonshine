# RoomInfo owner after a reboot and map reload

## Fresh .42 evidence

The user passed same-session SD import/load, soft-reset load and cold-reboot
load with the original named archive. Test 4 then crashed while loading that
Parlor state after a successful Storage menu warp, not on the following door.

The new exception is generation 9, payload CRC `9D445DA8` (.42): DSI at
`8002D470`, LR `80013C44`, DAR `A15ABD59`. Journal E generation 49 shows Storage
warp request/accepted/dispatch/arrived, followed by saved-grain verification,
accepted relocated cameras and restored-grain verification. The exception's
breadcrumbs finish all ROOT/SYS/GAME checks, LOAD `7F`, then state-load success.
The next native room-render lookup fails. This is not an out-of-memory report.

Preserved evidence lives at
`../sd-captures/lm-0.3.42-user-test4-20260908/`. Reports A and journals A/B are
stale .38/.40 files; report B and journals C/D/E are fresh .42. Archives 5/6
are identical original .42 saves. All copied files were SHA256-verified;
no SD keys were copied/read, and no SD file was changed by this analysis.

## Missing owner

The fixed native `RoomInfo` object occupies `803C2138..803C236C`, immediately
after the previously captured event-active range. Its layout is:

| Offset | State |
| --- | --- |
| `+0` | Borrowed `Koga::ToolData*` from MissionMode's GAME-owned array |
| `+4..+203` | 256 signed halfword room-to-row lookup entries |
| `+204..+230` | Twelve field indices |

JP setup `8002E654` passes this exact fixed object to initializer `8002D0B4`.
The initializer fills the index table, asks native `800B8658` for `RoomInfo`,
stores the returned pointer at `+0` and fills the field-index tail. The
object is plain lookup state, not an OS queue, retained SYS service or global
destructor record. Stop at `803C236C`, before the separate adjacent table.

Native `800B8658 -> 800B9764` obtains MissionMode through `804A17C8`, resolves
the JMP resource through its map archive, and returns a matching eight-byte
ToolData record from MissionMode `+10`/`+14`. The array is created during
Mission initialization (`800B92C4..800B934C`) using the current-heap array
allocator, an eight-byte cookie and eight-byte ToolData stride. This resource
family lives in GAME; it is not a borrowed mutable retained SYS object.

Native render `80013C14` reads the current view descriptor, passes the fixed
RoomInfo object to `8002D418`, and follows the cached ToolData pointer to its
JMP header. The crashing indexed load is exactly `8002D470`.

## Saved bytes reproduce the mismatch

In the original .42 archive, MissionMode's array is `80E6BFB8`, with 25 records
in a direct GAME allocation, tag `484D000B`, size `D0`, cookie `(8,25)`.
RoomInfo is record 21 at `80E6C060`, vtable `8034F1D4`, backing `80DA20C0`.
That backing is the exact `RoomInfo` file inside captured map RARC
`80C399A0..80E6BFA0`; its JMP header is `(72,13,AC,50)`.

After the live map's `+220` relocation, the omitted fixed owner retained
`80E6C280`. Restoring GAME turns that address into an archive link: its `+4`
is `80E6C2F0`, exactly the bogus JMP base in the crash. The indexed read adds
`2073FA69`, producing the reported `A15ABD59`. Restoring all GAME bytes cannot
repair an uncaptured fixed root that still points at the future allocation.

## Correction and verification

Capture/restore/cache-store the exact `234`-byte RoomInfo object through the
shared static-range manifest. Snapshot format advances to 26; static size is
`187C0`, camera sidecars start at `18908`, and GAME payload starts at `18C20`.
This adds 576 aligned core bytes, with unchanged MEM1/MEM2 reservations,
compressed companion, envelope and safety gates. Fresh states are required;
older archives remain inspectable but are not relabelled or imported as 26.

`test_lm_room_info_state.py` authenticates the clean JP initializer, resource
lookup and failing consumer, checks complete capture/exclusion boundaries,
and proves the actual .42 ToolData/backing mismatch. Archive parser tests cover
formats 24/25/26 and reject cross-schema relabelling. These are native-source
and host-data proofs, not a claim of a new Wii runtime pass.

Priority hardware retest: fresh state/export, repeated soft/cold-reboot load,
Storage menu warp, load original Parlor state, movement and ordinary door;
then repeat the same original load and door once more.
