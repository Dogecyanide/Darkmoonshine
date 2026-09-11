# Same-map native scene reload ownership

## 0.3.34 Wii evidence

Original logs are preserved at workspace `sd-captures/lm-0.3.34-20260907`.
The current payload CRC is `6D85A0FF`. Crash files are older; these attempts
were compatibility refusals, not newly recorded exceptions.

- Attempt A, generation 24: native menu warp to Parlor arrived at sequence
  252180. Load 252218 rejected at X06 (resource proof), mask `100`, volume
  count 24 to 24, four removals/four additions and three changed model rows.
- Attempt B, generation 25: menu warp to Anteroom arrived at 253662.
  Loads 257024 and 264064 rejected at X08 (model shape), with the same
  24-to-24, four/four, three-row shape. Both identities are map 2, scene 2.
- Load during the door action at 262184 was safely refused, gate 39.

The old model rule required `changed model rows == removals + additions`
and accepted only loaded-to-empty or empty-to-loaded rows. A native Mission
reload rebuilds some *same* model IDs: loaded-to-loaded is one changed row
with two independently owned archive endpoints. Thus the old arithmetic
cannot admit this observed shape. X06 did not include its exact failed field;
same-ID resource record replacement is a justified case to add, not a claim
that this particular A record's byte difference has been measured.

## Separate VR archive and missing output table

Evidence uses clean GLMJ01 DOL SHA1
`722005ea9c1eab54b114f814734d8f327e5614ee`.

`8002F508` selects a per-map scene VR resource. At `8002F56C` it starts a
memory-archive load and publishes its object to `804A0F10` at `8002F570`.
The completion callback `8002F0C0` creates a private solid heap from the
current GAME allocator (`8002F100`), switches to it (`8002F10C`), and builds
the J3D scene model and animations. The heap owner is `804A0F0C`.

The fixed BSS table at `803C24E8` contains three 20-entry pointer arrays at
offsets `0`, `A0`, `140`, paired with float-animation arrays at `50`, `F0`,
`190`. `8002F1CC`, `8002F220`, `8002F274` publish the pointers. Scene setup
zeros the scalar arrays; frame update `8002F618` consumes the same table.
Its exclusive end is `803C26C8`, precisely the existing animated-owner
range's start. Cleanup `8002F9A4` destroys the private heap, unmounts the
archive through its virtual `+C` entry, and clears the archive global.
There are no OS queues/threads in this fixed table.

The prior private Dolphin map-2 capture has 36 mounted volumes: `vrball_m`
is the one GAME-backed scene archive owned by `804A0F10`, not a model-table
row. The latest Wii four-volume/three-model shape is consistent with this
extra owner, but the journal does not contain the four archive names.

Snapshot format 19 adds exactly this `1E0`-byte fixed table (480 bytes).
Its archive global/private-heap roots were already in captured SBSS, and
their allocations are already inside the GAME snapshot. The SYS heap and
its persistent game archive remain live. This is not a whole-SYS rewind.

## Admission proof

Every changed archive still requires a valid mounted JKRMemArchive object
and complete backing range inside GAME, the same retained long-lived tail,
and an exact common ordered subsequence. Every changed model row is checked
on *both* sides. Loaded state 3 must bind the primary and registry handle to
the exact changed archive, with optional model/registry roots inside that
archive. Empty state 0 must have zero handles/roots. States 1/2 still reject.
Unknown model-table/registry field changes reject. Each changed archive must
be matched once, either by a model endpoint or the explicit VR archive root;
unknown/unaccounted archives cannot pass merely because their count fits.

Retail model ownership is established by `800615AC` (primary archive),
`80061070` (ready state 3), `80061430` (MDL root), `80185CF4` (registry
archive), and cleanup at `80061620..2C` / `80185DB0..B4`.

For a room-resource record whose active ID is unchanged but bytes changed,
the new exception requires both endpoints to be complete (byte `+2 == 2`),
the exact room callback `80011B28`, matching slot index/room ID/backing,
zero temporary archive `+8`, and auxiliary arrays `+C/+10/+14` with their
complete used extents inside GAME. Counts are bytes `+3C/+3D`, strides are
`7C/24/4`, and constructed arrays retain room for their eight-byte headers
(`8001F440` / `8001F62C`). A nonzero count cannot have a null pointer.
Reserved words `+24..+38` must be zero.
The callback unmounts/clears `+8` at `80011BB8..BC0`, then completion sets
state 2 at `8001F100..104`. Existing idle-record handling, table/bulk bounds,
map hash, empty wanted queue and backing-table validation remain.

The extra D7 journal record reports resource mismatch masks, wanted counts,
layout/map differences and backing faults. D8 reports the failing resource
slot mask, model index or unmatched archive index when available. They add
no synchronous disk writes or waits to the frozen restore path.

## Verification limits / next hardware route

Compiled helper and authenticated retail tests exercise rebuilt resources,
loaded-to-loaded endpoints, empty endpoints, bad pointers, pending states,
unknown field changes and the exact VR lifecycle. They do not establish
successful hardware resume.

Make new format-19 states. Test save -> same-room menu warp -> load -> walk
and use a door; Parlor -> Anteroom in both directions; and Foyer -> Storage
Room / distant floors, followed by normal movement and a door. Repeat the
round trips and test making a *new* state after a warp too. Failed loads
must retain a playable current room and produce the extended refusal bundle.

Map/scene, system/root heap, audio and camera ownership gates are unchanged.
Boss-map changes and cross-reboot SD portability are not made safe by these
same-map fixes. Broader shared-resource capture remains separate work.
