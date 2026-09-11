# Bounded same-map scene-reload admission

This separates two conservative source-code restrictions from the cause of a
particular tester refusal. It does not establish that a complete post-menu-warp
restore is safe, or that every mutable scene owner is now covered.

## Evidence and scope

The clean GLMJ01 DOL is SHA1
`722005ea9c1eab54b114f814734d8f327e5614ee`.
`scripts/test_lm_state_roots.py` authenticates it before checking the instruction
anchors below. The portable helpers in `include/susamune/lm_state_roots.h` use a
bounded reader; neither the helper nor its tests execute an archived state.

The runner's report distinguishes a first state that repeatedly refuses after a
menu warp from a new state saved after the warp that later crashes. The exported
format-19 archives have snapshot generations 9 and 11 and were made during later
storage tests. They are valid graph fixtures, not established crash snapshots.

A later captured 0.3.35 journal (generation 26, CRC `8B437156`) records same-map
2/scene-2 menu warp followed by an epoch refusal: `VOLH`, 26 to 26 volumes,
24 removed and 24 added, 23 changed model entries. Its guard stops at **X05,
archive ownership**, before resource validation. Its D7 record additionally
reports record mask `7F`, changed resource layout, unchanged resource map, and
zero backing faults. There is no per-archive offender descriptor in that journal.
Consequently the resource-address restriction is a second demonstrated barrier,
not proof of the archive check's specific failing condition.

## Scene roots

The prior guarded path admitted volume HEAD/COUNT drift only. Both the initial
epoch comparison and the later `headerMatchesLive` required equality of several
GAME-owned object addresses. A later `savedPointerCompatible` check could not
override either earlier rejection. Retail Mission-to-Mission reload destroys and
recreates these objects even for an unchanged map.

| Owner | Global | Complete bytes | Vtable | Native creation evidence |
| --- | --- | --- | --- | --- |
| MissionMode / GameMode alias | `804A17C8` / `804A17B0` | `24` | `8034F080` | `800B9220` allocation; `800B9238` publication; `800B8430..8434` publishes create's return as GameMode |
| SimpleModeler | `804A17D0` | `C10` | `8034F0CC` | `800B9D04..9D48` |
| MapCol | `804A17D8` | `14` | `8034F180` | `800BA1D0..A214` |
| EnTypesManager | `804A17E8` | `8` | `803560A8` | `800D90D8..9134` |
| EnManager, reached through Mission `+8` | not independently admitted | `E48` | `80358D68` | `800B9030..9054`, `800E2C90..2CA0` |
| Map archive, reached through Mission `+18` | **must remain identical** | `68` | `80388D5C` | `80006770..6778`, `801CEA54..EA5C` |

Mission `+4` must equal the EnTypesManager root. EnTypesManager `+4` points to a
constructed array of 381 entries of `218` bytes (`31DB8` data bytes plus its
8-byte header). `800D9110..9130` allocates and publishes it; `801F5528..5534`
writes its stride/count prefix. All these distinct object/array extents must be
fully inside the same captured GAME range, aligned, non-overlapping, and match
their exact native vtables or array metadata. GameMode must alias MissionMode,
not merely point somewhere else inside GAME. Header roots must also equal their
captured static globals.

`LmStateGameRootsValidate(context, readWord, roots, heapStart, heapEnd,
faultAddress, faultValue)` validates **one side**. Run it independently on saved
bytes and current bytes. A saved reader must reject unmapped addresses, never
fall back to live memory. The public record is six 32-bit fields in this order:
`missionMode, mapArchive, gameMode, simpleModeler, mapCol, enTypesManager`.

Only bits `MISSION_MODE | GAME_MODE | SIMPLE_MODELER | MAP_COL | EN_TYPES`
(`7C00`) are newly eligible. `LmStateGameEpochMaskAllowed` admits these together
with the old volume HEAD/COUNT bits. `LmStateGameRootOnlyEpoch` distinguishes a
nonzero root-only mismatch. Neither admits map/archive/scene/audio, volume-tail,
heap, SYS, or root-heap drift. No helper result bypasses quiescence, graph,
allocator, camera, resource, or model validation.

## Resource allocation addresses versus schema

Retail `80012470..12478` calls resource initialization with seven slots of
`70800` bytes. `800116E4..11714` creates seven constructed `40`-byte records,
including an 8-byte array prefix. `80011718..1172C` allocates the `313800`-byte
bulk with 32-byte alignment. The globals are `804A0D08`/`804A0D0C`, and the
per-slot backings are `80398ECC + 4*i`. Cleanup `800141AC` frees the bulk at
`800141C4` and destroys the record array at `8001423C`. These addresses may
legitimately relocate while the schema remains unchanged.

`LmStateResourceRootsValidate` uses the same bounded reader contract. Its record
is `recordBase, bulkBase, slotCount, slotSize`. It checks the exact seven-slot
schema, both complete non-overlapping GAME extents, bulk alignment, the record
array prefix, all global identities, and all seven backing roots. Call it for
both saved and current resource roots before treating an address-only layout
change as eligible. The existing record helper already accepts separate saved
and live record/bulk bases and must continue validating settled records.

This does not admit a changed resource map, pending requests, schema change,
invalid backing, incomplete record, out-of-GAME allocation, or model-owner
mismatch. It is not a reason to bypass X05 archive ownership.

## Runtime integration contract

1. Keep both snapshot authentication and the same-map/scene/audio/SYS/root-heap
   checks. Broaden only the documented five GAME-root mismatch bits, and only
   after both bounded root graphs validate.
2. Collect and diff complete volume/resource/model censuses even for a root-only
   mismatch; the old volume-bit-only census trigger would leave this path without
   a volume proof.
3. A root-only mismatch may have zero removed/added archives. In that case prove
   every descriptor, order, tail, count, current volume and current directory
   unchanged. Do not equate unchanged list endpoints with unchanged contents.
   Continue the same resource/model checks; this is not an early success path.
4. Replace exact equality of the five validated roots in every later
   header-versus-live admission check with the same bounded policy. Keep the
   before-versus-live freeze-boundary stability check exact: a graph that changes
   during admission is still unsafe.
5. Separate resource schema changes from address changes. Only the latter can
   qualify through both resource-root validators and existing record proofs.
6. Retain the generation commit checks. No evidence shows a menu warp resetting
   saved census generations; save and slot transfer own those updates.
7. Keep exact mapArchive identity. A relocated map archive needs its own complete
   mounted-owner/backing and model/volume coverage proof, not the root mask above.

## Regression coverage

The native tests exercise original and independently relocated graphs; every
mask bit; null, aligned, overflow and partial extents; GameMode aliasing;
captured-global disagreement; all native vtables; child links; allocation
overlap; array metadata; callback failures; resource schema and all seven
backings. Both optional full Dolphin MEM1 captures and both format-19 exports
pass the actual helpers when supplied. Export CRC/format/layout are checked
before mapping any bytes as a forensic fixture.

Runtime testing still needs: native doors as a control; fresh-boot save then
same-room menu warp and load; save again after that warp and repeat; same-map
different-room and cross-floor menu warp; subsequent door traversal and several
minutes of play after each success. Preserve the first refusal's complete
telemetry and an indexed archive-ownership failure. A new root or resource
admission must not turn a deliberate cross-map, active-I/O or bad-graph refusal
into a partial restore.
