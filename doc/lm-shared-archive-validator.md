# Japanese shared-resource archive identity validator

`include/susamune/lm_shared_archive.h` validates the live, boot-owned
`Game/game.szp` allocation. It neither snapshots nor restores any memory and
does not authorize arbitrary SYS addresses supplied by a state file.

## API

`LmSharedArchiveValidate(context, readWord, out, fault, value)` uses a bounded
32-bit reader. It returns one on success and a **zeroed output descriptor** on
every refusal. No allocation or recursive traversal is used.

The descriptor is exactly 40 bytes, with ten 32-bit fields:

`owner, base, size, systemHeap, systemStart, systemEnd, parentBlockTag,
ownerBlockTag, usedSignature, freeSignature`.

`LmSharedArchiveSameIdentity(saved, live)` compares all those fields and requires
the compiled Japanese resource size. Callers must validate the current live
descriptor again at the frozen restore boundary; descriptor equality does not
authenticate an archive or make an unvalidated descriptor trustworthy.

`LmSharedArchiveContains(parent, base, size)` tests a 32-byte-aligned nested
RARC span against the validated parent's **file-data area**. It excludes the
parent metadata/header, allocation headers, separate owner, and all other SYS
allocations. It proves containment, not that an arbitrary contained byte range
is itself a valid mounted RARC. Existing volume/type/header checks still apply.

## Exact accepted resource kind

The Japanese clean DOL is SHA1
`722005ea9c1eab54b114f814734d8f327e5614ee`. The boot callback is
`8000E338`: it obtains decompressed size, allocates an aligned payload, decodes
it, and separately allocates a 68-byte `JKRMemArchive`. Publication through
`80066328` is the only store to global `804A12B0`; its only retail caller is
`8000E3A8`. Mission cleanup calls `80066324`, an inert `blr`, so it retains this
parent resource. `test_lm_shared_archive_native.py` authenticates these retail
instructions independently.

The validator resolves SYS from `804A0B94` and the retained owner from
`804A12B0`, rather than using the fixture's absolute allocation addresses.
It requires:

- JKRExpHeap vtable `8038886C`, aligned and internally consistent SYS bounds.
- Complete, bounded traversal of both used and free lists, with valid block
  bounds and reciprocal links. Maximum 128 blocks per list; cycles cannot
  produce an unbounded read. Both endpoints are rechecked after traversal.
- The payload and separate owner actually occur in the used list with exact
  sizes `419B00` and `68` and group `10`. A coincidental interior `HM` word is
  not allocation membership. No other used/free block may overlap either.
- Archive vtable `80388D5C`, SYS disposer and archive heaps, proper embedded
  disposer/list identities, RARC type, mounted state, mount count one, mode one,
  and the parent's no-free-backing flag. Intrusive link neighbours are not
  rewritten or included in the descriptor.
- Every owner table pointer is the expected offset from the parent base.
- All 64 bytes of the clean Japanese RARC header/info are unchanged.
- Directory, file-shape and string metadata retain compiled fingerprint
  `94D08ED2` (word-wise FNV-1a). The fingerprint covers 25 directory records,
  the `2960`-byte string table, and the first 16 bytes of all 856 file records.
  Mutable fetched-resource caches at file-record `+10` and resource payloads
  are deliberately excluded. This is a compatibility fingerprint, not archive
  authentication or a semantic validator for every model/animation byte.

The immutable offsets are: info `+20`, directory table `+40`, file records
`+1E0`, strings `+44C0`, owner name `+44C5`, and file data `+6E20`.
The actual clean disc's decompressed metadata and both private MEM1 captures
produce the same header and fingerprint. The optional clean-ISO test verifies
the disc's DOL hash before extracting/decompressing this single resource.

The descriptor's used/free fingerprints cover every traversed allocation-header
address and word, retaining the conservative allocator-identity check. They do
not authorize copying or rewinding those allocator headers.

## Capture extent and capacity

In both private Dolphin captures and both supplied format-19 census fixtures,
the exact parent payload is `807BAFE0..80BD4AE0`, size `419B00` (4,299,520 bytes).
The preceding allocation header is `807BAFD0`; the following header at
`80BD4AE0` and separate owner at `80BD4AF0` must not be copied. This payload is
decompressed resource data, not the SYS heap's OS threads, stacks, queues,
audio working arena, GX FIFO, or framebuffers.

All 18 nested model archives form one contiguous union
`809132E0..80BBDDA0`, size `2AAAC0` (2,796,224 bytes). Their smaller span does
not cover mutable outer archive caches, door matrices, and other shared
resource output. Luigi's nested bytes alone differ by 5,501 bytes between the
two captures, including clothing edits; complete parent bytes differ by 5,648.

For the measured GAME size `C16BE0` and format-20 core prefix `17A40`, including
the existing `1900` census trailer:

| Capture | Total raw bytes, hex | Margin under primary `FD0000` limit |
| --- | --- | --- |
| Core plus 18 nested archives | `EDA9E0` | `F5620` bytes free |
| Core plus complete parent | `1049A20` | `79A20` bytes too large |

Previously measured production compression reduces the parent alone to
2,092,602 bytes. A bounded compressed companion can therefore fit the measured
primary tail. Compression, transport, authentication, rollback, prevalidation
and restoration are separate caller responsibilities, not features of this
identity helper. Keep same-session authentication and current map/scene/audio
gates; retained resource identity is not proof of cross-reboot compatibility.

## Tests

`test_lm_shared_archive.py` compiles and executes the production helper. It
tests descriptor size/equality, containment edges, null/bad readers, complete
SYS/owner extents, allocator membership, groups/sizes, list bounds/reciprocity,
cycles, overlap, every class of failed read, exact headers/table roots,
metadata changes and deliberately mutable resource bytes. Both MEM1 fixtures
have identical descriptors despite differing resource content.

Full valid-graph fixture tests skip when private MEM1 assets are absent;
unconditional native boundary/containment tests still run. The clean-disc
metadata test similarly skips when the optional ISO is absent. No retail
payload or snapshot is embedded in or executed by these tests.
