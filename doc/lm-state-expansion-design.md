# One-slot mansion-wide storage: measured design constraints

This is a capacity/proof audit, **not implemented restore support**. Naming work
was paused before any editor or label-format source changes when the priority
changed to one reliable mansion-wide state.

## Measured capture

Measurements use the private 0.3.30 Dolphin MEM1/fake-VMEM capture and the exact
production miniz bridge (128 probes, zlib checksum). The capture is not a
redistributable test fixture.

| Data | Raw bytes | Deflated bytes |
| --- | ---: | ---: |
| Existing snapshot plus 6,400-byte census trailer | 12,774,112 | 5,218,488 |
| Entire SYS data range, capacity upper bound only | 6,995,824 | 2,829,426 |
| Concatenation of both | 19,769,936 | 8,047,280 |
| Same concatenation, independent 64 KiB blocks plus 16 bytes/block | 19,769,936 | 8,160,016 |
| Same, independent 256 KiB blocks plus 16 bytes/block | 19,769,936 | 8,076,706 |

The sampled SYS heap is `0x805384C0`, with data
`0x80538550–0x80BE44C0`. **Its entire contents are not safe to restore.** The
range contains persistent game resources alongside potentially live SDK,
renderer, audio and threading objects. Capacity numbers confer no permission to
rewind those objects or the allocator that owns them.

### Narrowed candidate: the boot game-resource archive

The SYS audit identified `0x807BAFE0–0x80BD4AE0` (4,299,520 bytes) as decompressed
retail `Game/game.szp`, with all 782 member paths and sizes matching the source
archive. The separate `JKRMemArchive` object is at `0x80BD4AF0` (0x68 bytes).
Its live data contains relocations and cached resource pointers, so matching
the original archive header does **not** make all live bytes immutable.

| Data | Raw bytes | Deflated bytes |
| --- | ---: | ---: |
| This resource archive alone | 4,299,520 | 2,092,602 |
| Existing snapshot plus this resource archive | 17,073,632 | 7,311,453 |

This narrower candidate changes the storage conclusion: raw plus a complete
compressed rollback **does fit** the currently owned regions, with 1,824,931
bytes left in the measured case. A bounded two-bank raw layout can use the
16,580,608-byte primary payload region and place its 493,024-byte overflow at
the start of the validated file-patch suffix. Temporary compressed rollback
then fits across the 3,866,368-byte Sega/DIMM temporary region and the remaining
5,270,016-byte file-patch suffix. Metadata/alignment must still be budgeted, and
nonzero file-patch counts reduce that margin.

No large compressed-resident rewrite is needed merely to store this candidate.
The decision to restore it still depends on proving its mutable fields,
relocations and archive-object ownership; the storage measurement does not
establish those invariants.

## Actual owned capacity

After preserving the snapshot's upper 128 KiB and the Sega/DIMM area's mailbox
and 320 KiB codec workspace:

| Region | Available bytes |
| --- | ---: |
| Snapshot payload reservation | 16,580,608 |
| GLMJ-only Sega/DIMM temporary region after workspace | 3,866,368 |
| Validated file-patch suffix with zero patch entries | 5,763,040 |
| Total | 26,210,016 |

The file-patch suffix remains conditional on the published, bounded patch
count. Its aligned count-and-entry prefix and immutable mod staging cannot be
reclaimed. ISO/RealDI, live DI scratch, configuration/crash mailboxes, ARAM and
other games' runtime buffers are not additional free memory.

A 19,769,936-byte raw snapshot fits, but leaves only 6,440,080 bytes. Its measured
8,047,280-byte compressed rollback **does not fit simultaneously**: the shortage
is 1,607,200 bytes. Expanding the old contiguous import limit would also run into
configuration/kernel memory. Current raw-address arithmetic must never be
silently stretched over the memory-map gap.

## Two bounded implementation directions

### First preference: capture only proved missing SYS data

Identify the actual missing allocation classes and lifecycle before changing
storage. The existing raw reservation has roughly 3.8 MB of unused payload tail
in this capture. The required game-owned subset may fit without broadening to
all 6.7 MiB of SYS or replacing the existing restore transport.

For each added allocation require a stable owner/type and full pointer closure:
every saved reference must point into another restored region, an immutable
retail resource, or a deliberately reconstructed live object. Heap metadata,
mutexes, queues and hardware handles are not ordinary payload. Restoring a few
payloads while rewinding unrelated SYS free/used lists is not safe.

### If coverage really needs most of SYS: one compressed logical snapshot

Independent deflate blocks are a credible storage design. The measured 64 KiB
block representation costs only about 1.4% more than whole-stream deflate.
Two compressed generations of the sample use 16,320,032 bytes, fitting the
primary payload reservation with 260,576 bytes left; the verified extra regions
remain available for workspace and capacity variation. This is a measurement,
not a guarantee that all rooms will compress to the same size.

The active logical snapshot would stay compressed instead of keeping a full raw
copy. A bounded block directory and access layer would replace raw-address
assumptions for header/static/heap reads. Restore would first decode and validate
every block into scratch, enforce total length and the complete schema/resource
proof, then copy approved game-owned ranges at the established frozen boundary.
No file-supplied destination address would become a write target. The codec
workspace, guard census and compressed source must never be restored over.

For SD imports, retain the current immutable compressed generation while reading
and validating a candidate into separate free blocks. Only commit its directory
after exact size, checksum, region schema and compatibility admission succeed.
A short read, bad checksum, invalid pointer proof or missing capacity leaves the
old snapshot selected. The ARM mailbox must name fixed transport buffers or
validated owned-bank spans, not arbitrary addresses from the archive.

This needs a genuine format/reader/restore refactor, not just a larger constant.
Existing direct `kSnapshotBase + offset` accesses and contiguous invalidation,
checksum and copy calls must all be routed through the logical representation.

An alternative raw-expanded design can use an atomically written and verified
SD rollback journal. It can preserve the old snapshot on disk, but if the card
vanishes during replacement, the in-memory state may be unavailable until that
card returns. That is weaker availability than the compressed-generation design
and must be reported honestly; the retail running game would remain untouched
until a later explicit load.

## Across-reboot archive admission is a separate problem

Persisting the process key, removing the session check, or matching pointer
numbers alone does not establish that a new boot has compatible live objects.
Current archives remain same-process only.

A portable archive needs at least:

1. Exact retail revision/DOL and mod format identity; strict bounded schema,
   checksums and canonical padding rather than trusting arbitrary bytes.
2. Saved map, mansion/progression context and resource identities sufficient to
   recreate the target using native initialization before any restore writes.
3. A proved allocation/object graph. Recreated types, ownership, sizes and
   immutable resource hashes must match. If addresses differ, relocation must
   use explicitly typed pointer fields or symbolic IDs, never scanning for
   integers that merely resemble pointers.
4. Live SDK/audio/GX/thread objects must be kept live or reinitialized through
   their APIs. No restored game object may retain a stale reference to them.
5. Preflight after reconstruction must admit the complete graph before the
   first game-memory write, and failures must keep a usable native scene.

File admission and live compatibility are distinct gates. A conservative new
fixed-address profile could record the complete retained SYS/root allocation
descriptors (address, size, group and proved type/owner), native initialization
recipe and normalized retail archive identities. After native reconstruction,
those descriptors and the existing scene/volume/model/camera/audio checks must
all match. Legacy archives cannot gain this missing proof by dropping their
session field. A CRC detects file damage, but neither it nor a whitelist of
restore destination ranges validates arbitrary embedded game-object pointers
or vtables after execution resumes.

The session authentication guard should be replaced only when that constructive
proof exists. More MEM2 solves missing storage, not missing object lifetime or
cross-boot identity information.

## Smallest practical change map: separate RARC companion extent

This plan deliberately leaves the established GAME representation contiguous.
It is the preferred bounded implementation direction after the 0.3.31
floor/hotkey changes are verified, not a change present in that build.

### Keep all established GAME offsets

Keep `SnapshotHeader` at 256 bytes and keep `totalSize` meaning exactly the
existing GAME/core byte count. Keep its strict `totalSize == kHeapDataOffset +
heapSize` invariant. Do **not** append RARC bytes by inflating that field:
`snapshotChecksum`, `loadState` cache invalidation and several copy operations
currently assume it is physically contiguous below the settings block.

These existing paths can remain unchanged:

- `sSavedModelCensus`'s model-table/registry pointers into the core;
- `cameraObjectRecordAddress`, camera capture and camera restore offsets;
- `captureStaticRanges` and `restoreStaticRanges`;
- the GAME heap and allocator-metadata source offsets;
- `snapshotChecksum`'s current treatment of magic/checksum fields;
- the core layout portion of `basicHeaderValid` and the original primary-bank
  capacity checks.

Store the RARC companion as a second raw extent at the validated file-patch
suffix's start. Do not overwrite the patch prefix or staged mod. In the measured
case a 64-byte companion descriptor plus the complete RARC uses 4,299,584 bytes
of its 5,763,040-byte capacity, while core plus census stays at its existing
12,774,112-byte location.

The fixed, versioned companion descriptor should contain its kind, byte length,
source/owner identity, generation, core checksum, clean-retail provenance and
own content checksum. Its kind is a compiled-in `GameResourceArchive`, not an
arbitrary restore command. Any stored source address is evidence to compare,
not a destination to trust. Resolve the actual destination from the currently
validated live allocation and require exact source/owner compatibility.

The companion must be bound to the same core generation and checksum. A valid
core with a missing, stale or mismatched mandatory companion must be refused.
Use a new snapshot/companion format version for that requirement; retaining core
offsets does not mean accepting older incomplete snapshots as the new format.

### Preserve rollback in three temporary spans

Remove the inactive resident slot from this mode. The combined state compresses
to about 7.31 MB, so the old `appendPackedSlot` operation cannot copy rollback
into the single 5.5 MiB cache. Keep rollback where the compressor produced it:

1. The existing Sega/DIMM temporary region after mailbox and codec workspace.
2. The unused primary raw tail after the larger of old and incoming core+census.
3. The unused file-patch suffix after the larger of old and incoming companion.

For the sample plus a 64-byte descriptor those spans total 9,136,320 bytes.
All boundaries must be aligned to separate cache lines. The codec workspace
and guard census remain outside every span. Store the rollback's sizes,
codec ID, checksum and span plan in private mod state. On a failed import,
validate that complete packed stream and restore both raw extents before
re-enabling loads. Capacity failure must occur before either raw extent changes.

### Exact source areas to change

| Source / functions | Required bounded change |
| --- | --- |
| `lm_state.cpp`: `saveState` | Preflight companion owner/range/capacity before invalidating the old core. Copy mutable RARC bytes inside the same existing freeze as GAME, not later in `savedSlotCommitted`. Publish the core commit word only after both extents and their binding/checksums are complete. |
| `lm_state.cpp`: `loadState` | Validate/invalidate the companion through its own bounded extent before quiescing or writing GAME. Recheck live ownership inside the freeze. Restore approved RARC bytes before post-copy volume/transport repair helpers and resume. Preserve existing GPU/audio/epoch checks unless a specific proved rule supersedes them. |
| `lm_state.cpp`: `initializeSlot`, Clear, size reporting | Invalidate both commit records on initialization/Clear. Report core and companion bytes honestly. Use one selectable logical slot. |
| `lm_state_storage.inc`: `rawSlotValid`, `savedSlotCommitted`, `copySlotTrailer` | Keep the existing census at core+coreSize. Add companion validation and matching generation/core-checksum binding; no live SYS capture after freeze ends. |
| `lm_state_storage.inc`: `flushRawSlot`, import invalidation | Walk exactly two known raw spans. Never flush or invalidate a combined length beginning at `kSnapshotBase`. |
| `lm_state_storage.inc`: `packCurrent`, `backupImportTarget`, `serviceStorage` | Replace inactive-slot append/compaction with a three-span rollback record kept in place. Protect both old and candidate extents and restore both on failure. |
| `lm_state_storage.inc`: `beginStorage`, `importedArchiveValid` | Use a canonical logical payload order: core, existing census, companion descriptor, companion data. Authenticate/checksum that exact order and both lengths. |
| `lm_state_deflate.cpp` / `lm_state_deflate.h` | Deflate reads two source spans sequentially using the same `tdefl` state (`NO_FLUSH`, then `FINISH`); output callback supports at most three destination spans. Inflate accepts up to three source spans and emits to two raw spans through its existing private 32 KiB ring, retaining history across the physical output gap. |
| `lm_state_auth.h`, `storageCrc` | Add exact concatenation over bounded spans. CRC state carries across spans; SipHash carries its partial eight-byte word and total length. Do not authenticate only CRC32 values as a substitute for authenticating bytes. |
| `lm_state_storage.h` | Version the archive and transport separately; include explicit core/census/companion lengths and layout kind. Old versions must be refused before body transfer. Preserve independent PPC request, ARM response and immutable ownership cache lines. |
| `launcher/kernel/LmStateStorage.c`: `ValidHeader`, Phase 2/3 | Validate both raw lengths against independently computed owned-bank bounds. Map logical file offsets to the primary bank or published file-patch suffix. Limit each 16 KiB slice by remaining bytes in that bank, so no slice crosses a gap. Flush only the bytes actually read. File bytes never choose physical addresses. |

The core save/load offset assumptions therefore remain intact; the refactor is
concentrated in companion capture/restore, archive transport, streaming
checksum/authentication and the existing storage codec adapter.

### Candidate-size preflight is mandatory

The current importer starts reading body bytes after ARM validates its header.
That is insufficient when rollback occupies the unused tails of the candidate's
two destination banks: a larger candidate could overwrite the rollback before
PPC sees its header at final acknowledgement.

Two safe choices are available:

- **Conservative initial profile:** require an already stable native scene and
  bind the import request to its known exact GAME/core size and the proved fixed
  RARC companion size. ARM checks the file's lengths against those PPC-supplied,
  independently bounded expected lengths before the first body read. Plan
  rollback beyond those exact extents. This is appropriate while normal loads
  already require matching game-heap geometry; it must not advertise arbitrary
  cross-layout imports.
- **General profile later:** ARM opens the archive and returns a header-only
  response while retaining the file handle, without writing raw bytes. PPC
  validates metadata, plans staging beyond both candidate and current extents,
  prepares/validates rollback and then explicitly authorizes body transfer.
  Cancellation or reset closes the handle and acknowledges before any new
  process touches those banks. No safe staging capacity means refuse the import.

Both profiles keep unique numbered filenames, `.tmp` creation, sync/close and
rename semantics. No existing archive is overwritten. Neither profile changes
same-process authentication into a claim of reboot portability.

### Minimum regression tests before hardware

- Every split around a 16 KiB ARM slice, an eight-byte SipHash word, a 32-byte
  cache line, primary-bank end and companion-bank end; poisoned gaps untouched.
- Exact deflate round trips for two inputs/three packed spans/two outputs,
  including full private-ring prevalidation and a larger target forcing rollback.
- CRC and SipHash equal the contiguous concatenation for every tested split;
  companion bytes, lengths, kind, generation and binding are covered.
- Wrong legacy archive/transport versions, overflowed or mismatched lengths,
  unknown companion kind, wrong core binding and altered payload are refused.
- Wrong larger header is rejected before body writes in the conservative
  profile; header-only negotiation never overwrites current state in the general
  profile. Card removal and reset during every phase preserve/recover rollback.
- Insufficient published suffix, nonzero/invalid patch counts and maximum state
  sizes fail before writes and preserve patch prefix, staged mod and guard data.

Finally, verify RARC owner/relocation restoration with live transitions. Passing
transport tests establishes byte preservation, not correct resource lifetimes.
