# Japanese model-effect managers across scene reloads

Verified against clean GLMJ01 `main.dol`, SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`. Addresses below are native retail
addresses, not guesses from the current allocation layout.

## Exact capture boundaries

`801501E4` is the translation-unit static constructor. It uses base
`803CC998`, constructs three objects, and calls `801F51C8` with the object
in `r3`, destructor in `r4`, and registration record in `r5`:

| Object | Capture interval (exclusive end) | Separate registration record | Destructor |
|---|---|---|---|
| 0 | `803CC9A4..803CCC4C` | `803CC998..803CC9A4` | `8014E604` |
| 1 | `803CCC58..803CCF00` | `803CCC4C..803CCC58` | `80150324` |
| 2 | `803CCF0C..803CD1B4` | `803CCF00..803CCF0C` | `801502B0` |

Each capture footprint is `0x2A8`; the total is `0x7F8` (2040 bytes).
Object offsets are base+`0xC`, `0x2C0`, `0x574`; registration offsets are
base+`0`, `0x2B4`, `0x568`. Calls at `80150234`, `80150260`, and `8015028C`
register those exact pairs. `801F51C8` writes three words: previous global
destructor-list head, destructor function, object pointer, then publishes the
record as the new global head. These are process-lifetime runtime records,
not scene state. Do not capture the aggregate decomp symbol's `0x814` bytes:
it includes two records and four trailing padding bytes. The three equivalent
object footprints fit immediately before the records / trailing padding.

## Scene-owned graph, not live SDK handles

Scene initialization `80156A9C` calls `8014FC0C`, which initializes the three
objects through `8014E668`, `8014F9EC`, and `8014FAFC`. For each object:

- `+0` owns a private `JKRSolidHeap` created under the current GAME heap.
- `+0x5C` and `+0x60` own GAME arrays of `0xA8C` and `0xE10` bytes: 15
  controllers of stride `0xB4` and 30 entries of stride `0x78` respectively.
- Initialization calls `80134954(object+0x5C, 30, 15)` to seed linked lists.
  The active-list root is at object+`0x24C` (absolute `803CCBF0`, `803CCEA4`,
  `803CD158`). These contain scene-relative pointers, not stable identities.
- The initializer makes the private heap current, constructs `0xA0`-byte
  J3DModels, stores their pointers at entry+`0x44`, shrinks the private heap,
  and restores the previous current heap. Model-data provenance is the live
  scene owner at `*(804A1758)`, then `+0x30`, then `+0x50/54/58`.
- Scene cleanup `80156C50 -> 8014FF50` deletes both arrays and destroys each
  private heap. Native draw/update functions directly consume the BSS roots.

The bounded BSS footprints hold game effect owners and bookkeeping. They do
not include the global destructor records, OS thread/queue objects, or GPU/
audio mailboxes. Their pointed-to GAME allocations are already captured by
the whole-GAME snapshot. Restoring the GAME objects but retaining these
future BSS roots mixes allocation generations after a complete menu warp,
even when the GAME heap and mission addresses happen to be reused.

## Connection to the .32 crash: evidence versus hypothesis

Fresh report `20260906-224608/luigis_mansion_crash_b.txt`, CRC `D0E41038`,
finished LOAD `7F`, then faulted at `8012E3A4` on the first draw. `r29` was
`8129E490`; `lwz r3,0x94(r29)` returned `FFFFFFFF`, so the next read at
`r3+0x4C` faulted at address `0x4B`. This is the game's grain-particle
controller path, not evidence that the separately captured SDK JPA module
itself is missing.

The retail first-draw order is:

1. `8000BBCC -> 80156B0C -> 8014FC94` consumes the three model-effect managers.
2. Its nested lists reach entry+`0x44`, then call J3DModel update `801C2418`.
3. `8000BC7C -> 8000BA64` eventually calls grain draw from `8000BB0C`.

J3DModel update at `801C24D4..801C24E0` copies model+`0x74` to model+`0x94`
and model+`0x7C` to model+`0x98`. Thus a future model pointer aliasing a
restored grain controller can overwrite the *exact* field that later
crashed. The old BSS omission, native lifetime, ordering, and store offsets
are proven; that aliasing pointer/value in this particular Wii attempt is
not captured, so the exact writer remains a hypothesis. Do not repair
`+0x94` or suppress the failing draw as a substitute for restoring ownership.

Snapshot format 17 adds the three split ranges. This invalidates older raw
layouts rather than interpreting bytes with changed static-range offsets.
It is a targeted completeness fix, not proof of every mansion-wide case.

## Grain validation contract

Native init `8012EA48` calls `80128BF4(manager, 1500, 80)` with manager
`803CBF48`; `80125F4C` builds its lists. The following are independent of
allocation addresses:

| Field | Required meaning |
|---|---|
| manager+`0` | 80-controller pool, stride `0x1B8` |
| manager+`4` | 1500-particle pool, stride `0x54` |
| manager+`0x60` | equals manager+`0xC`, free-particle sentinel |
| manager+`0x21C` | equals manager+`0x64`, free-controller sentinel |
| manager+`0x3D8` | equals manager+`0x220`, active-controller sentinel |
| every controller+`0x94` | equals controller+`0x40`, embedded particle sentinel |

Controller next/previous offsets are `0x140/0x144`; particle next/previous
offsets are `0x4C/0x50`. Validate complete pool extents before reading nodes,
exact pool stride alignment, correct embedded sentinel, reciprocal links,
and bounded traversal (80 controllers and 1500 particles total). A list
entry must be either its own list's sentinel or a member of the correct
pool; a pointer merely inside MEM1 is insufficient. Controller+`0x94`
is initialized at `80125FB8/FBC` for every controller, including inactive
ones. It does not legitimately become a heap-selected alternate sentinel.

At minimum, check all 80 self-sentinel fields in the saved source and after
copy, before resuming native draw. This distinguishes a corrupt saved
source/copy from corruption later in the first draw. Source-check failures
reject before restore writes and preserve the previous live state. The
current post-copy check is telemetry-only: it has no rollback copy and does
not stop the already-restored state from resuming. Neither path patches the
particle list or replaces a failing pointer.

## Reproduction checks

Run `venv/Scripts/python.exe scripts/test_lm_effect_managers.py`.
The executable tests authenticate the entire clean DOL before checking the
constructor records, allocation/cleanup instructions, draw order, and exact
corruptible field. Source checks ensure the three split ranges are actually
in the snapshot table. An absent clean DOL skips only retail-evidence tests;
a present but wrong DOL fails authentication.

The 0.3.36 validator in `include/susamune/lm_grain_state.h` checks both this
80-controller manager and `803CBAF0`'s 15-controller manager, each with 1500
particles. It proves the four rounded allocation extents are disjoint, checks
all manager and controller sentinel identities, and traverses the free/active
controller lists and every particle list with reciprocal links and exact pool
coverage. Per-pool bitmaps reject duplicate membership and bound all cycles;
the two managers reuse 200 bytes of bitmap storage without heap allocation.
Run
`venv/Scripts/python.exe scripts/test_lm_grain_state.py` to compile and execute
that same helper natively. Tests cover all 95 self-sentinels, malformed links,
pool bounds/overlap, duplicate or missing membership, cycles, reader failure,
both available Dolphin memory captures, and the two later runner exports.
Exports are read-only graph fixtures, not authenticated runtime imports or
the snapshots involved in the reported crashes.

The format-20 capture also includes the separate lazy model-effect owner at
`803CC46C..803CC718`; see `lm-0.3.35-crash-a-grain-analysis.md` for its
authenticated ownership proof and the distinction between this concrete
omission and the still-unproven exact writer responsible for crash A.
