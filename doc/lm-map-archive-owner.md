# Native map archive and cross-boot volume coverage

The 0.3.41 runner's soft-reset and full-Wii-reset loads were refused at X08,
before camera/audio/profile admission and before any restore writes. The
recorded mismatch was volume head/count only (`00000180`); both sides were
ordinary mansion map 2 / scene 2. All 21 saved GAME volumes were replaced.
The first unaccounted removed entry, index 20, was `map2`.

`MissionMode` owns that mount directly through `+0x18`; it is not represented
by a model-table row. Local decomp `Koga/MissionMode.cpp` creates the map mount
in `init()` and unmounts it in the destructor. The authenticated Japanese DOL
calls the native loader at `800B9290 → 80006808` and stores its result with
`800B9294: stw r3,0x18(r30)`. Existing native proof establishes the MissionMode
allocation size `0x24` and JKRMemArchive size `0x68` / vtable `80388D5C`.

The provided map2 sample has MissionMode `80BE4990`, wrapper `80C39910`, and
backing `80C399A0..80E6BFA0` (length `232600`). Each is a direct group-B GAME
allocation. Both archives' retained ROOT/SYS profiles agree aside from the
snapshot generation and checksum; that does not by itself prove live safety.

## Admission

`lm_map_archive.h` accounts only for the changed volume whose object equals
the already-captured MissionMode map-archive pointer. It validates:

- Captured/current MissionMode and GameMode aliases, native MissionMode vtable,
  and the exact `+18` ownership edge.
- Native JKRMemArchive vtable, GAME owner/allocator, RARC type, native backing,
  info/header/data pointers, RARC magic and bounded complete file extent.
- Full bounded reciprocal GAME used-list traversal, with exact sizes and
  group-B membership of the owner, wrapper, and backing allocation.
- No overlap between those allocations and any other listed allocation.

Saved and live images are checked independently with their own heap-list
anchors. The saved reader cannot fall back to live memory. The existing volume
census proves mount/link shape and the earlier archive gate proves mounted
state and captured backing ownership.

This is not a relaxation of mapArchive object identity, map/scene compatibility,
SYS/audio/profile identity, shared-archive companion integrity, other model or
resource ownership, or frozen-boundary checks. Other unexplained changed
volumes remain rejected. The new helper does not allocate or write game memory,
and no snapshot format or memory reservation changes are required.

Other X08 refusals in the runner's later generation-21/24 logs lack enough
identity data to name their offending volume. New immediate, critical-queue
LOAD records retain it without extending the delayed rejection bundle:
`D9.arg0` bit31 is saved/live (1/0), remaining bits the actual census index;
`D9.arg1` is the wrapper object. `DA` contains name hash and RARC backing.
They contain only already-validated census words; no new pointer is followed.
The queue remains bounded and does not overwrite unacknowledged records.
Its capability version is 2 with unchanged layout: a mixed old/new launcher
pair falls back to the latest-value channel instead of accepting phase types
the other side cannot drain. Deploy the matching launcher and mod together.

## Tests

The native helper harness tests valid independent backings, exact owner links,
RARC bindings, all allocation constraints, foreign/partial/cyclic/orphan/overlap
failure, and reader failure. Optional private 0.3.41 archived GAME graphs all
pass the same compiled helper. They are forensic fixtures, not executed game
code, and do not demonstrate a successful live cross-reboot restore.
