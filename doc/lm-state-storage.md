# LM state storage: format 28 / protocol 6

Snapshot format 28 has **one resident logical state**, prioritizing complete
captured resource coverage over a second state. New version-2 SD archives can
carry a restricted **same-build, same-setup reboot profile** for ordinary
Japanese mansion map/scene 2. Version-1 archives are still same-process only.
Different builds/formats, boss maps, arbitrary consoles/configurations and
modified heap layouts are not supported by that profile. Matching filenames,
rooms or heap addresses alone never establishes compatibility. The new
cross-boot path passed the user's .42–.44 ordinary mansion tests, but remains
guarded and is not a verified general solution. Fresh RC4 archives need
the included reboot retest.

## Snapshot contents and memory ownership

The contiguous GAME/static core is followed by a mandatory 64-byte descriptor
and the compressed `0x419B00`-byte shared Japanese `Game/game.szp` resource
payload. The packed stream is rounded to 32 bytes with zero padding; saved
volume census, resource census, model metadata and a 1,440-byte retained-owner
profile follow it. The descriptor
binds snapshot generation/core checksum, compressed stream and validated
retained resource owner. See `lm-shared-archive-lifetime.md` and
`lm-sys-resource-audit.md` for native provenance and exclusions.

Only the resource allocation's payload is captured. Its allocator header and
separate SYS archive owner remain live, as do OS threads/queues, ARAM ownership,
audio arena, GX FIFO and XFBs. This is not a SYS heap rewind. A changed GAME
archive wrapper can borrow bytes from this captured parent only after live
parent identity, ownership, file extent and existing model/resource proofs pass.
Unknown SYS objects/backings still refuse.

The core header's `totalSize` remains the GAME/static extent. Storage `rawSize`
includes core, descriptor and aligned compressed companion; the census trailer
begins at that combined extent. The SD envelope authenticates all those bytes.
Old body-only format assumptions are invalid for the current format.

The combined state stays in the dedicated `0x91F00000–0x92EF0000` reservation.
Its upper 128 KiB is excluded from imports/payloads to protect live census
scratch. A further 8 KiB below the payload ceiling is reserved for the saved
census/profile trailer. The new profile fits that existing reserve; no MEM2
reservation increases. Capacity checks use actual encoded output; failure never
silently omits shared bytes.

The GLMJ01-only workspace is `0x91300000–0x91700000`, after Nintendont's first
`DIinit` has cleared the otherwise unused SegaBoot/DIMM region. LM never enters
Triforce mode. Disc cache below and DI scratch above remain reserved.

- First `0x400` bytes: mailbox reservation; protocol 6 occupies 928 bytes.
- Next `0x50000` bytes: heapless codec workspace, shared between the bounded
  LZ4 blocks and miniz's private decode ring; never used concurrently.
- Remaining `0x3AFC00` bytes: temporary shared-payload compression or import
  rollback's second segment. Those operations cannot run concurrently.

The file-patch handoff is **not dead at runtime**. `PatchGame` rereads its count
and `(address, value)` prefix after in-session resets. ARM validates the count,
preserves `align32(4 + count * 8)` bytes and publishes only the unused suffix of
`0x11900000–0x11E7F000` for compression storage. Invalid counts grant no suffix.
PPC independently checks the publication. The immutable staged mod beginning at
`0x11E7F000` remains untouched. This suffix is rollback capacity, not an inactive
resident slot.

Dolphin uses `0x70000000` for the state, `0x71400000–0x71800000` for mailbox/
workspace/staging and `0x71800000–0x71D7F000` for the packed pool, within fake VMEM.
It has no ARM filesystem worker; SD actions report `SD: WII ONLY`.

## Save and load transaction

Save validates live ownership, waits for the existing resource/audio/GPU-safe
boundary, then freezes consumers before compressing the shared parent into
separate staging. Actual encoded size must fit staging and the prospective
primary-state tail before the old core is invalidated. Core, companion and
saved proof censuses are committed together. A too-large companion preserves
the previous valid snapshot.

Load validates core, descriptor and packed checksum, zero padding, generation
and the complete LML4 or zlib stream before writing live game memory. Dry decode uses
the private ring, not a game destination. The live parent allocation, mounted
owner, SYS identity and immutable resource metadata must still match. These
checks repeat at the frozen boundary before companion and GAME/static state
restore. CPU writeback and GX vertex/texture invalidation precede resumed
rendering. Existing scene/audio/camera/model/allocator gates remain active.
Every profile-bearing resident state also repeats its retained-profile and
audio-normalization checks, including after an in-process game reset; this is
not limited to states whose last action was SD import.

Format 28's companion kind 2 permits either the distinct `LML4` container or
legacy zlib. The fast path uses pinned LZ4 1.10.0, independent 128 KiB blocks,
a 32 KiB hash table, raw fallback per incompressible block and a complete
Adler32 trailer. It is not byte-compatible with Moonshine's `MSL4` container.
If fast output exceeds capacity, pinned miniz 3.1.0 with 128 dictionary probes
and a zlib checksum retries more densely. Rollback records carry `LZ41` or
`DFL1` for the actual encoding. Both codecs count past output exhaustion so
refusal can report exact required capacity. All CRC passes remain, now using
the equivalent 1 KiB immutable table. Historical raw/packed measurements are
examples, not mansion-wide maxima or Wii latency results.

All three measured complete dense inactive states exceed the best-case safe
file-patch suffix by about 1–1.2 MB. Shared staging is needed by every save and
by rollback, so it cannot double as a second resident slot. See
[compression measurements](lm-compression-benchmark.md). No memory reserve grew.

`Clear slot` only clears the resident state, not SD archives. Import fills the
resident state without moving Luigi; Load/D-pad Right performs the separately
guarded game restore.

## SD export, import and browser

Export creates `/lm_states/archive_00000001.lms` and increasing decimal IDs on
the launcher's storage device. Existing archives are never overwritten. ARM
writes a new `.tmp`, syncs/closes it and renames only after the complete file
succeeds. Failure may leave an incomplete `.tmp`, not replace an earlier archive.

Before import, the complete previous state and census are compressed directly
into two segments: the admitted file-patch suffix, then codec staging. Neither
overlaps any possible ARM import destination. The backup stays in place; it is
not appended or copied elsewhere, and **no raw-snapshot tail is borrowed**.
CRC and full dry-decode validation finish before ARM receives the request. If
backup capacity is insufficient, no request starts and the previous state is
unchanged. An empty slot needs no backup.

ARM checks the outer header and full file length before any payload read, then
transfers at most one 16 KiB slice per DI-idle service pass. Files never choose
destination addresses; writes are bounded to the snapshot reservation. PPC
validates build/layout/provenance, complete payload CRC, SipHash-2-4
authentication, companion/core, census generations and any retained profile.
I/O or validation failure
restores and revalidates the backup. Rollback is not successful import; feedback
says whether the old slot was kept or empty.

The 64-byte outer envelope has two explicit kinds; both use snapshot format 28
and the combined `rawSize` meaning above:

- **Version 1:** `session` is the fresh process session; the payload uses the
  per-process authentication key. Reserved fields must be zero. These files
  remain useful for the current process and are not upgraded by ignoring the
  session field.
- **Version 2:** `session` is the durable SD key ID; `reserved0` identifies the
  `LMP1` profile and `reserved1` is the launcher configuration ID. A
  domain-separated authentication calculation binds every envelope byte
  except the two authentication words, then authenticates the entire payload.
  Build, format, configuration, generation and profile cannot be substituted
  independently while retaining an old valid tag.

The **transport** still uses a new process session after every injected-process
startup. It cancels old transfers and waits for acknowledgment before using
their memory. A durable file key never becomes a transport-session bypass.
The MEM2 commit word is still cleared on a fresh payload startup; import must
reconstruct and validate the resident state explicitly.

### Durable SD key and boot setup

Two matching 64-byte records, `lm_states/archive_key0.bin` and
`archive_key1.bin`, preserve the local provenance key. With both absent, PPC
requests initial creation from the new process seed; ARM writes/syncs temporary
files and renames without overwriting existing keys. One valid copy plus one
missing copy can be used. Corrupt, conflicting or unreadable records refuse;
they are not silently regenerated. Back up these records together with the
archives. Losing the only valid key prevents version-2 import. The key is local
file provenance/integrity, not protection against someone who possesses and
deliberately edits both the archives and key.

The independently computed boot configuration ID covers Nintendont/config
versions, relevant launch flags, video mode/scale/offset/progressive choice,
language, pad configuration, card mode/size/timing, disc ID and region.
Cheat/debugger-enabled configurations are excluded from the initial persistent
profile. The key alone does not authorize a different setup. Dolphin has no
ARM filesystem worker and does not create portable SD states.

Only a state successfully captured with an available key and valid retained
profile is exported as version 2. Export feedback distinguishes `REBOOT READY`
from `THIS BOOT ONLY`; the browser distinguishes `REBOOT ARCHIVE` from
`SAME SESSION`. If profile capture fails, the existing in-session save remains
available rather than falsely labeling its export portable.

### Retained profile and cold-boot admission

The 1,440-byte profile contains its magic/version/generation/checksum,
configuration ID, ROOT/SYS identities, up to 48 complete used-allocation
records and 64 reserved anchor words. It compares native pad ownership and
idle callback/rumble policy; FIFO geometry; the fixed XFB pair and render mode;
the five boot ARAM descriptor addresses/sizes; audio data/table identity and
configured counts; and fixed audio-camera endpoints. Dynamic input values,
FIFO cursors, frame alternation and stale completed ARAM commands are excluded.

Before restore, the destination runs the existing native audio drain. A bounded
postcondition requires no active gameplay SE/stream/extra sequence handles and
the correctly rebound bootstrap sequence. The profile is compared once after
that normalization and again inside the existing freeze barrier, before any
shared/GAME restore writes. Existing full SYS free/used fingerprints and object
ownership checks remain active. GAME camera addresses may differ only when
both sets are proved complete GAME-owned allocations in their respective
images. No SYS bytes, thread queues, DSP state or numerical pointer scans are
imported as a repair. See [retained-owner proof](lm-persistent-owner-proof.md).

The browser lists actual regular files named exactly `archive_########.lms`,
eight sorted IDs per page. ARM scans at most 16 directory entries per service
pass using fixed storage. The exclusive last ID is the next-page cursor.
Temporary files, malformed names and directories are ignored, not deleted.
Listings read the 64-byte envelope and optional name records, never snapshot payloads. Headers
provide size/compatibility hints; import repeats full validation. Old-build/
session/format/setup/key files stay visible but are not importable. Manual ID selection
remains a fallback. Display names persist separately; archive filenames do not change.

Transport protocol 5 preserves the first 160 bytes and the separately owned
ARM receipt. Each of eight catalog entries is 64 bytes, including a 32-byte
name. The PPC-owned request-name line starts at offset 736, beyond the ARM
catalog writeback range. ARM's key publication occupies bytes 768..831; PPC's
seed request occupies 832..863. Both codec workspaces start at the shared `0x400`
reservation boundary. Request sequence, operation, requested ID and
session must match the response, with exact transfer length/result ID. ARM owns
response/receipt cache lines; PPC owns requests. Catalog progress is separate
from import/export feedback so refreshing cannot erase an error. Install the
matching `boot.dol` and payload together; older protocol launchers do not support
the naming/key/profile transport.

## Persistent display names

Export opens a heapless Moonshine-style controller keyboard. Names contain
up to 31 printable ASCII characters. In the SD browser, X renames the selected
archive; Y refreshes. Empty names display the numeric archive ID. Renaming an
old-session or old-build archive is allowed, but does not authorize its import.

Names are two alternating 64-byte metadata records beside each archive:
`archive_########.name0` and `.name1`. Each contains its archive ID, generation,
canonical zero-padded name, CRC and the checksum of the immutable archive
header. A stale name belonging to a different archive is ignored. Corrupt or
missing metadata falls back to the ID, not an unreadable state. Back up the
whole `lm_states` folder to retain names with their archives.

Rename writes, syncs and closes a temporary record before replacing only the
inactive metadata generation. The `.lms` is never rewritten. Interrupted or
failed writes leave the previous valid generation available; host fault tests
do not establish physical power-loss guarantees. Empty exports write a blank
record too, preventing inheritance of an orphaned label. If the state commits
but its name cannot be saved, feedback says `EXPORTED; NAME NOT SAVED` with the
actual new ID. The usable archive can be renamed later.

Names surviving reboot do not establish snapshot compatibility. Display-name
records are independent of the archive version, authentication and retained-
owner gates, so a readable old name may correctly accompany an unusable state.

## Verification

- `scripts/test_lm_state_transactions.py` executes the production client include,
  ARM worker, codec and authentication together. Coverage includes successful/
  failed/empty-slot imports, different core/companion sizes, authenticated bad
  descriptors, payload-ceiling imports, two-segment rollback, capacity refusal,
  old sessions, malformed receipts and browser pagination/error paths. Game
  snapshot/companion ownership is a fixture in this harness.
- `scripts/test_lm_state_storage.py` and `scripts/test_lm_state_deflate.py` cover
  codec bounds, malformed/truncated streams, exact lengths, zlib interoperability,
  authentication vectors, file-write failures, import limits, session cancellation
  and file-patch prefix publication.
- `scripts/test_lm_shared_archive_native.py` authenticates JP boot/cleanup,
  separate transport/payload/owner allocations, nested mounting and resource
  cache behavior. Portable owner/guard tests exercise identity/extent refusals.
- `scripts/test_lm_persistent_profile.py` runs the actual profile helper against
  synthetic malformed cases and, when available, private native MEM1 fixtures.
  It verifies stable owner identity without comparing live FIFO/input state.
- `scripts/test_lm_audio_idle.py` checks the actual bounded postcondition,
  rejects active gameplay owners, and authenticates native drain/release sites.

Host tests do not model Wii cache coherency or certify mansion-wide restoration.
Hardware coverage must include post-menu-warp loads, natural cross-room/floor
routes, repeated door entry after restore, boss boundaries, same-session SD
round-trips, full-reboot version-2 imports/loads, old version-1 session refusal,
and failed imports while the previous state remains loadable. Only supported
map/scene/setup cases are candidates for portable success. Private retail
captures and SD keys are never packaged.
