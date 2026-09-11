# Scene-owned depth targets and the 0.3.36 post-load heap failure

## What the new report establishes

The first 0.3.36 Wii capture is preserved in workspace
`sd-captures/lm-0.3.36-user-20260907-first`. Its new crash is report B,
generation 5, payload CRC `692195EB`, `PC=801CA6E4`, `DAR=00000007`.
Report A is an older `A9D47118` HUD failure and is not a new regression.

The new crash is the retail GAME heap check following a bad free-list link:
`r6=812A23C0`, its payload size and next pointer are `FFFFFFFF`, and the
instruction attempts `next->previous` at `FFFFFFFF+8`. The immediate
post-restore ROOT, SYS and GAME checks all returned true (`75..7A`), followed
by load completion. Journal A, generation 30, continues through eight tracked
post-load updates and ends at presenter milestone `87`, before heap sampling
returns. The periodic diagnostic check runs every 60 presentations; its crash
is detection of corruption, not proof of which earlier instruction wrote it.

This establishes a post-return writer rather than invalid free links already
present immediately after the raw copy. It does not establish out-of-memory.

## The missing native owner

Fixed BSS object `803C4B6C..803C4C80`, size `114`, was absent from the format-21
static snapshot. It owns GAME-allocated graphics targets that can move when a
menu warp reconstructs Mission state. It is not the boot-owned SYS XFB pair.

| Owner offset | Native purpose |
| --- | --- |
| `00..5F` | Two affine texture matrices |
| `60..7F` | GXTexObj sampling the full-screen depth target |
| `80..9F` | GXTexObj sampling the 256x256 lookup/ramp texture |
| `A0`, `A4` | Draw/setup scalar state |
| `A8` | Full-screen target pointer; exact allocation size `96000` |
| `AC` | Lookup/ramp pointer; exact allocation size `20000` |
| `B0..10F` | Two associated affine matrices |
| `110..113` | Trailing bytes within the native object symbol |

The preceding `803C4B60..803C4B6C` is a 12-byte global-destructor registration
record, **not part of the snapshot**. Static initializer `8005D3FC` passes
`803C4B6C` and destructor `8005C720` to `__register_global_object` at
`801F51C8`, using the preceding record as its link. The following `803C4C80`
symbol is a separate object and must not be accidentally included.

## Allocation and teardown proof

All addresses below are authenticated against the clean Japanese revision-0
executable SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee`.

1. Mission setup `8000C0C8..8000C0D0` selects heap group `0D`, then calls
   `8005CE40`, which passes the fixed owner to initializer `8005C798`.
2. That initializer requests `GXGetTexBufferSize(640,480,0x13,0,0)` at
   `8005C7EC`. Format `13` is Z16: the native tiled size is `96000` bytes.
   `JKRHeap::alloc(size,32,nullptr)` at `8005C80C` uses the current heap and
   stores the allocation in owner `+A8`. `801C8EA4..801C8F08` explicitly
   resolves a null allocator argument through `JKRHeap::sCurrentHeap`.
3. `8005C8DC` requests a 256x256 IA8 texture, allocates its `20000` bytes at
   `8005C918`, then stores owner `+AC`. Initialization fills this lookup
   texture and creates its descriptor at `+80`.
4. `8005CEA0` frees both targets (`8005CEC0`, `8005CECC`). Mission cleanup
   calls it through `80060614`. It does not itself clear the pointers;
   later initialization replaces them. The global destructor also frees both
   and clears the pointer fields. A stable-state validator must not accept a
   target merely because this stale pointer is non-null.

The complete private `.30` MEM1 fixtures independently establish actual GAME
used-list membership, exact allocation size and group, not only address range:

| Fixture | `+A8` target | `+AC` lookup | Allocation tag |
| --- | --- | --- | --- |
| `diagnostic-capture-0.3.30` | `81293E00` | `81329E20` | Both `484D000D` |
| `diagnostic-capture-0.3.30-newreport` | `81293C40` | `81329C60` | Both `484D000D` |

These are older, distinct captures, **not the source and destination of the
new `.36` crash**. The crash node `812A23C0` lies inside the first fixture's
depth-buffer interval, which supports the writer hypothesis but cannot identify
the exact new-run destination by itself.

## Why it can overwrite a valid restored heap

Normal Mission drawing reaches `8005DD68` through `8000BCB8`. When its captured
enable scalar at `804A1228` is nonzero, it obtains owner `+A8` through getter
`8005D00C`, then passes that address to `GXCopyTex` at `8005DDEC`. This copy
uses a full-screen depth format (`11`, Z8). A second path,
`8005CF60 -> 8005CC48 -> GXCopyTex@8005CCD8`, also obtains the destination from
`+A8`, using Z16. Depth clear values can plausibly explain all-one words;
the crash alone does not prove their origin.

With GAME memory rewound but this owner left in the destination epoch, those
GPU copies continue writing the future scene's target address. That address
can now be a saved free-list header or another saved allocation. The restore's
heap check can therefore pass before the first new draw damages it. The same
stale owner would also free the wrong allocation during a later scene cleanup.

Copying only the two raw pointers is insufficient: GXTexObj `+0C` embeds the
physical address shifted right five bits. Restoring the whole fixed owner
keeps those descriptors, matrices, flags and pointers in the same epoch as
the already-captured target allocations. Do not allocate replacement targets
after restoring or null-skip the copy as a substitute for owner consistency.

## Bounded endpoint proof and integration

`include/susamune/lm_render_targets.h` provides:

```
LmRenderTargetsValidate(context, readWord, heapStart, heapEnd,
                        usedHead, usedTail, faultAddress, faultValue)
```

It makes no writes and uses fixed stack space. The reader and list anchors must
refer to the **same saved or live image**. Both endpoints validate independently;
there is no requirement that their buffer addresses match and no relaxation of
the existing scene/epoch gates.

The helper requires both pointers to be 32-byte-aligned complete GAME ranges,
with non-overlapping allocator/header extents. A reciprocal used-list walk,
capped at 8192 nodes, proves each is an exact payload start with the native
size and group `0D`. Fabricated interior `HM` signatures are not ownership.
Alignment/tail flags are not mistaken for the allocation group.

For both embedded GXTexObj descriptors it checks dimensions, IA8 sampling
format `3`, image-base encoding, tile count, load format and non-mipmap flags.
`GXLoadTexObj` changes the high-byte BP register IDs, so those bytes are not
fixed identity. The image base is the **low 21 bits** of descriptor `+0C`,
equal to `(backing >> 5) & 0x1FFFFF`; dimensions and format occupy the low 24
bits of descriptor `+08`. This follows native `GXInitTexObj` and the SDK source.

Format 22 adds the `114` owner bytes, raising the aligned core header/static
prefix by `120` (288) bytes. No additional pixel allocation or MEM2 reservation
is required: both target payloads were already inside the GAME snapshot.
Keep the existing completed-draw boundary, I/O quiescence, scheduler freeze,
writeback and texture-cache invalidation. Validation belongs before save,
against the saved image before any restore write, and at the frozen live
boundary; it must not turn an invalid post-copy state into a harmless refusal.

`scripts/test_lm_render_targets.py` covers the native lifecycle instructions,
synthetic valid endpoints, forged/nonmember allocations, group/size/overlap,
bad links and read failures, descriptor address/dimension/format mismatches,
and both private native endpoints. Mixing the later complete owner with the
earlier GAME image is rejected even though each original endpoint validates.
This is host/source evidence, not a new Wii pass or a claim that no other
uncaptured graphics owners remain.
