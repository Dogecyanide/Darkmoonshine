# Japanese LM persistent SYS allocation audit

Read-only audit of private Dolphin captures:

- `build-lm-emu/diagnostic-capture-0.3.30/mem1.bin`
- `build-lm-emu/diagnostic-capture-0.3.30-newreport/mem1.bin`

Both are 24 MiB MEM1 images with offset zero corresponding to `0x80000000`.
The second was captured later in the same process during ordinary map 2
exploration, not after a reboot or a map 9 transition. Addresses below are
observations, not unconditional runtime constants.

## Conclusion

Do not snapshot the complete SYS heap. It contains live OS thread records,
thread stacks, ARAM ownership, audio working memory, the GX FIFO and both XFBs.
There are **37 used allocations**, not seven.

One specific allocation, the `0x419B00`-byte shared `Game/game.szp` RARC, is a
reasonable additional snapshot domain if its allocation and mounted owner are
validated before every restore. It contains retail resource data and mutable
rendering output, not the live OS/audio/GX machinery listed below. Capturing
it avoids assuming that every mutable model field will be regenerated before
its next consumer. This is not yet proof of cross-reboot compatibility: the
archive's absolute base and retained owner must match, or all captured incoming
and internal pointers must be relocated/reconstructed by a separately proven path.

## Used-list verification

`JKRExpHeap::CMemBlock` is 16 bytes: `u16 HM`, flags byte, group byte, allocated
payload size, previous block, next block. SYS is `0x805384C0`; its payload bounds
are `0x80538550..0x80BE44C0`, vtable `0x8038886C`, size `0x6ABF70`. The used-list
head/tail are heap `+0x7C/+0x80`; traversal in both captures has 37 valid headers
with identical allocation addresses, sizes, groups, previous/next links.

Provenance: `lm-decomp/libs/JSystem/include/JSystem/JKernel/JKRExpHeap.hpp` and
the corresponding `JKRExpHeap.cpp`. Class identities below are additionally
confirmed by the live vtable's RTTI name, not guessed solely from group IDs.

| Payload address(es) | Payload size(s), hex | Group | Purpose / capture policy |
| --- | --- | --- | --- |
| `80538560` | `98` | `05` | JUTGamePad, global `804A0BF8`; keep live input owner |
| `80538608` | `88` | `03` | JKRAram, global `804A2020`; keep live |
| `805386A0`, `8053C6C0`, `8053C9F0` | `4000`, `320`, `40` | `03` | ARAM thread stack, OSThread, 16-message backing buffer; never rewind |
| `8053CA40` | `44` | `03` | JKRAramHeap; keep live ARAM allocator |
| `8053CA94` | `24` | `03` | Initial JKRAramBlock/list owner; keep live |
| `8053CAC8` | `60` | `03` | JKRAramStream, global `804A2028`; keep live |
| `8053CB40`, `80540B60`, `80540E90` | `4000`, `320`, `40` | `03` | ARAM stream stack, OSThread, message backing buffer; never rewind |
| `80540EE0` | `60` | `03` | JKRDecomp, global `804A2048`; keep live |
| `80540F60`, `80544F80`, `805452B0` | `4000`, `320`, `40` | `03` | Decompression stack, OSThread, message backing buffer; never rewind |
| `80545300` | `80000` | `01` | GX command FIFO, global `804A0BA0`; never rewind |
| `805C5320`, `8065B340` | `96000` each | `02` | 640x480 YUYV XFBs; leave to renderer/VI |
| `806F1350` | `18` | `0A` | JUTDirectPrint, globals `804A0BB0/804A2088`; framebuffer owner, keep live |
| `806F1378` | `64` | `0A` | JUTException, global `804A2070`; keep live crash owner |
| `806F1400`, `806F5420`, `806F5750` | `4000`, `320`, `40` | `0A` | Exception stack, OSThread, message backing buffer; never rewind |
| `806F57A0` | `5000` | `09` | JMath sin/cos lookup table, global `804A1FE8`; boot-generated, no snapshot needed |
| `806F13EC`, `806FA7B0`, `806FA7C4` | `4`, `4`, `1C` | `08` | JUTResFont WID1/GLY1/MAP1 pointer arrays; immutable retail font pointers |
| `806FA7F0` | `5C` | `08` | J2DPrint, global `804A0FE8`; UI formatter, not gameplay owner |
| `806FA85C` | `400` | `08` | J2DPrint shared text buffer, global `804A1FB8`; scratch, keep live |
| `806FAEC0` | `C0000` | `04` | Audio working arena allocated by bootScene::loadStaticData; never raw-rewind |
| `807BAED0`, `807BAF04`, `807BAF38`, `807BAF6C`, `807BAFA0` | `24` each | `03` | JKRAramBlock objects for boot-transferred UI resources; keep live |
| `807BAFE0` | `419B00` | `10` | Shared Game/game.szp resource bytes; candidate bounded snapshot domain |
| `80BD4AF0` | `68` | `10` | Shared JKRMemArchive owner, global `804A12B0`; preserve live object, repair only verified resource-list links |

Thread allocation sequence is explicit in `JKernel/JKRThread.cpp`: stack,
OSThread, then message buffer. JKRAram, JKRAramStream, JKRDecomp and
JUTException constructors identify the four owners. GX/XFB allocation sizes
and groups are explicit in `Unsorted/80005EB8.cpp` and `LMDisplayUtil.cpp`.
`bootScene.cpp` allocates the `0xC0000` audio arena and transfers res_titl,
gameboy, guidemap, res_paus and res_list to ARAM. Those decomp function labels
can be from another region; JP layout/address claims here use the live image.

The font owner at `803C3394` points through `+50/+54/+58` to the three small
arrays. Their contents resolve to retail WID1 (`80328C00`), GLY1 (`802FBBE0`)
and seven MAP1 blocks, matching `JUTResFont::setBlock`/allocation code.
Retail `801C8974` builds the `0x5000` sin/cos table once and publishes
`804A1FE8`; it is byte-identical between these captures.

The three live camera pointers at `80399BE0` are `80E6CB20`, `80E6CC1C`,
`80E6CD18`: all within GAME, already covered by the GAME snapshot. No separate
uncaptured SYS camera allocation appears in this fixture.

## Shared archive identity and provenance

The allocation header is `807BAFD0`: `484D0C10`, payload size `00419B00`.
The exact payload interval is **`807BAFE0..80BD4AE0`**, exclusive upper bound.
The separate owner begins at `80BD4AF0`; its header at `80BD4AE0` is not part
of the archive payload and must not be included in an archive byte copy.

Decompressing the user's clean Japanese ISO `Game/game.szp` produces exactly
`0x419B00` bytes. All 782 top-level file paths/sizes and the complete first
`0x40` archive bytes match the captured archive. It contains 18 nested RARCs;
recursive traversal visits 1,267 files. All 898 nonzero live `SDIFileEntry::mData`
values equal their own archive's `dataBase + fileDataOffset`, with no external
cached data pointers in this fixture. The cache writes are explained exactly
by retail `JKRMemArchive::fetchResource` at `801CED34..801CED64`.

The mounted JP owner has these exact fields (JP differs from some decomp
header comments; `801CEC88..801CED30` verifies the offsets):

| Offset | Meaning | Observed value |
| --- | --- | --- |
| `00` | JKRMemArchive vtable | `80388D5C` |
| `04`, `38` | disposer/allocation heap | `805384C0` |
| `18..27` | intrusive volume-list node | preserve/repair with validated census, not OS list rollback |
| `28` | volume-name pointer | `807BF4A5` |
| `2C` | type | `52415243` (RARC) |
| `30` byte | mounted | `01` |
| `34` | mount count | `1` |
| `3C` byte | memory mount mode | `01` |
| `40` | buffer-based archive identity | `807BAFE0` |
| `44` | archive info | `807BB000` |
| `48` | directory table | `807BB020` |
| `4C` | file-entry table | `807BB1C0` |
| `50` | string table | `807BF4A0` |
| `5C` | archive header | `807BAFE0` |
| `60` | archive file-data base | `807C1E00` |
| `64` byte | free-backing-on-unmount flag | `00` |

The owner object is byte-identical between the two same-process captures.
This proves retention during that tested interval, not across all maps or
reboot. The SYS used-list header equality alone is insufficient identity:
the same allocation can have different resource bytes or mounted contents.

The live Luigi MDL at `80AA2800` comes from nested
`Game/game.szp:model/luige.arc:model/luige.mdl` (size `37CA9`). Its on-disc
counts/offsets already match the live variant. The standalone
`model/luige.szp` MDL (`36C85`) is different; do not attribute that difference
to runtime compaction or to savestate damage.

## Demonstrably mutable resource bytes

- MDL header and texture-table pointers are relocated in place by `80060EBC`.
  KEY/PTH/TMB/TXP resources likewise contain loader fixups/initialized flags.
  These are resource-lifetime state, not necessarily per-frame state.
- Luigi vertex positions are **runtime morph output in the shared archive**.
  `8005B7E0` loads actor `+4` -> MDL `+48` at `8005B820`; indexed Vec3 values
  are cleared, accumulated (`8005B9F4/BA08/BA1C`) and normalized
  (`8005BA68/BA74/BA80`). `8005BAB0..BAC0` flushes `positionCount * 12`.
  The live position array is `80AC7548`, count `6D2`, size `51D8`.
  The first capture differs from retail in 1,119 non-pointer MDL words,
  including this vertex output and material state. The second capture changes
  14 more position bytes and three bytes within the material region.
- The 14 `iwamoto/door/door_XX.bin` resources and `saku.bin` have runtime
  scenegraph matrices in their file bytes. BIN node size is `8C`, with a
  48-byte affine matrix at node `+54`. Retail recursion `8001CC10` reads a
  model wrapper's node pointer `+3C`, copies the parent matrix to
  `node + index*8C +54` (`801DC66C` calls at `8001CC50/CCA4/CD34`) and
  composes the node's transform through `8001D5C8`/`801DC6A0`.
  Example door_01 is `807D2FA0`, length `20C0`, scenegraph file offset `1F00`;
  its three matrix spans start `807D4EF4`, `807D4F80`, `807D500C`.
  They are zero on disc and contain affine transforms in the captures.
  These three particular matrices do not change between the two captures.
- `kt_static/suikom_new1.btk` has animation-table/index changes relative to
  disc; `wplight.mdl +1180` changes from `1` to `3`. Do not classify these as
  immutable merely because a two-frame comparison happens to match.
- The clothing mod changes the two `2000`-byte CMPR image spans at
  `80AB6B40` and `80AB8FC0`. Reapplying the current colour after restore remains
  appropriate. Between the two captures these account for 5,484 of the 5,648
  changed shared-archive bytes; other runtime resource writes remain real.

## Reconstruction constraints

1. Before raw restore, prove the SYS allocation covers exactly the expected
   archive extent, the retained object is the expected mounted memory archive,
   and its header/table/name pointers describe that same allocation. Validate
   clean-disc resource provenance separately from mutable live byte hashes.
2. A raw archive restore is safe from OS-thread/audio-owner overwrite only
   because it is limited to this single payload, excluding both CMemBlock
   headers and the JKRMemArchive object. Keep the SYS heap/OS/GX/audio machinery
   live, and use the existing validated volume-list repair for resource links.
3. The saved GAME contains pointers into SYS resources and to the mounted
   owner. Identical resource names with different base/owner addresses are
   insufficient. Require matching live addresses for direct restore; otherwise
   reconstruct/rebase the captured pointer graph, not just the archive header.
4. Loading pristine bytes alone is not enough: relocated model/animation
   pointers, resource-entry caches, and any required derived rendering state
   must agree with the restored GAME/static model census before a draw, event,
   or collision consumer runs. The proved native writer routines are not yet
   a proved complete or correctly ordered reconstruction procedure.
5. Require DVD/resource loading quiescence and GPU completion, then perform
   appropriate CPU writeback plus GX vertex/texture cache invalidation. Larger
   snapshot extent also needs storage/trailer bounds and format version changes;
   this audit adds no runtime copies and does not assert the current raw buffer fits.
6. Obtain paired native map 2 -> 9 -> 2 and fresh-boot captures before claiming
   stable allocation/owner identity across those boundaries. Neither current
   fixture establishes that. A mismatched owner/base must cause a clean refusal
   or a verified retail reconstruction, not a blind SYS restore.
