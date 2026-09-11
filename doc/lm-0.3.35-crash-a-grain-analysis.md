# 0.3.35 runner crash A: grain graph and missing lazy model owner

Read-only analysis, 2026-09-07. No runtime patch, build, SD write, snapshot
import, or session-authentication bypass was performed for this analysis.
Retail evidence uses clean GLMJ01 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`.

## What the report proves

`luigis_mansion_crash_a.txt` is generation 8, build CRC `8B437156`.
The load reached phase `7F` and event 257; root, SYS, and GAME heap checks
all returned 1. A DSI then occurred at `80129DA8` (`lha r4,0x24(r29)`),
with `r29=FFFFFFFF`, hence DAR `00000023`. This is an invalid read through
a grain-particle link, not direct evidence of exhausted memory.

The native call chain is:

1. `8000BB10 -> 8012B1D0`: renderer selects fixed manager `803CBAF0`.
2. It follows manager+`3D8` and controller links at `+140`.
3. `8012B234 -> 80129D20`: selected controller is `r28=81274D30`.
4. `80129D60..D70` fetches controller+`94`, then sentinel+`4C`.
5. The loop draws particles and advances through particle+`4C` at `80129F40`.
6. Fault `80129DA8` uses the next particle's `+24` field.

The LR is `80129F40`, the return site of the display-list submission at
`80129F3C`. That establishes this loop already submitted a particle before
advancing into `FFFFFFFF`; it is not the earlier .32 crash where the
controller's own sentinel pointer was immediately invalid. The prior particle
address and exact offending `+4C` storage address are not in this dump.

## Existing validation does not cover this graph

`LmGrainValidate` currently checks only manager `803CBF48`, its 80-controller
pool bounds, and each controller's `+94 == controller+40` self-sentinel.
It does not validate manager `803CBAF0`, any particle pool, or any next/previous
links. Therefore the successful LOAD/6B record does **not** prove Crash A's
graph was valid immediately after copy.

The first manager's constructor is `8012A890`, called from scene setup at
`8000C0B8`. It allocates `0x19E0` bytes for 15 controllers of stride `1B8`
and `0x1EC40` bytes for 1500 particles of stride `54`; it calls common list
initializer `80125F4C` with `(1500,15)` at `8012A904`. Scene cleanup calls
`8012B280` from `8000BE50` and deletes both arrays. Manager `803CBF48`
uses the same list implementation with `(1500,80)` at `8012EAA4`.

Both managers' fixed data are already within captured `803CBAF0..803CC460`,
and their pools belong to the copied GAME range. No larger grain BSS capture
is justified merely from this fault.

## A concrete omitted owner found before the grain renderer

A **fourth**, lazy model-effect manager is omitted from the current snapshot:

| Part | Address / meaning |
|---|---|
| Destructor registration | `803CC460..803CC46C`, exclude |
| Object footprint | `803CC46C..803CC718`, 0x2AC bytes |
| Initialized flag | object+`2A8` |
| Private GAME child heap | object+`0` |
| Controller pool | object+`5C`, 15 x `B4` = `A8C` bytes |
| Entry pool | object+`60`, 50 x `78` = `1770` bytes |
| Active-list root | object+`24C` |
| Entry model pointer | entry+`44`, 0xA0-byte J3DModel |

The static constructor at `801348EC` constructs object+`C` relative to
`803CC460` and registers only the preceding 12-byte record at `80134938`.
It initializes the extra flag at `80134928`. The following fixed object
starts at `803CC718`; do not capture the preceding registration or following
objects as part of this footprint.

Lazy setup `80134754` tests the flag, invokes `80133B9C`, then sets the flag.
`80133B9C` allocates both pools and a private solid heap beneath the current
heap. It populates entry+`44` with models from
`*(804A1758) -> +30 -> +5C`, then shrinks that heap. Cleanup
`80156C84 -> 80134838 -> 8014E944` deletes pools, destroys the child heap,
and clears the flag; cleanup explicitly clears the flag again at `80156C8C`.

First draw calls `80156BA4 -> 801347D0 -> 8014E798` **before** the faulting
grain draw. These routines use the omitted flag and list roots, descend to
entry+`44`, and invoke J3DModel update `801C2418` at `8014E834`.
That update writes through model pointers; for example `801C24D4..24E0`
copies model+`74` to model+`94` and model+`7C` to model+`98`.

This is a proven scene-ownership omission and a plausible pre-grain corrupter
after full scene reload. It is **not** proven to be the exact writer in crash
A: the report does not record this manager's flag, roots, or traversed models.
The lazy manager can legitimately be inactive, in which case its stale unused
pool fields must not be treated as a live ownership failure.

## Bounded correction and regression coverage

1. Add only the separate `803CC46C..803CC718` footprint to the static snapshot
   table, preserving its initialized flag together with its pools. Bump the
   raw layout version and every size/offset assertion. Authenticate retail
   constructor, lazy allocation, cleanup, and draw words in executable tests.
   This is a completeness correction, not a confirmed complete crash fix.
2. Validate **both** grain managers before save and before any restore writes.
   Bound pool extents and exact strides; validate manager sentinel identities;
   walk active/free controller lists and every controller's particle list plus
   free-particle list. Require reciprocal links, no foreign nodes, duplicates,
   cycles, or cross-list membership, and exactly 15/80 controllers and 1500
   particles per manager. A pointer merely inside MEM1 is insufficient.
3. Keep the post-copy check, expanded to the same graph. Record both the bad
   field address and actual link value. A post-copy failure cannot safely be
   called a refusal because live GAME has already been overwritten; a failure
   policy needs either rollback or a controlled stopped diagnostic, not silent
   continuation or list repair.
4. Tests should mutate every manager root, every controller sentinel, first and
   last next/previous links, a loop with no sentinel, duplicate particles across
   two controllers, pool overlap/misalignment, boundary extents, and callback
   failure. Add a regression with valid controller self-sentinels but particle
   `next=FFFFFFFF`: old helper passes this shape; complete validation must fail.
5. Test menu warp with the lazy manager inactive and active at both endpoints,
   then save again and warp/load. Preserve the passing natural-door/cross-floor
   cases and repeat using doors after each load. If valid-copy graphs still
   fail, instrument a bounded first-draw interval around the model updates to
   distinguish an omitted owner from invalid source data; do not guess-capture
   all SDK/static memory or suppress the failing renderer.

## Later exported states: useful fixtures, not crash snapshots

The supplied exports fit the report's SD round-trip tests 44/45, but their
exact pairing with a test or crash is not established. The read-only helper
`build/gaddwarp/inspect_runner_grain_035.py` verifies their archive payload CRC
and format bounds, then inspects both complete grain graphs as byte offsets.
It does not make them trusted or portable for runtime import.

| Export | Snapshot generation | First manager active particles | Second manager active particles |
|---|---:|---:|---:|
| `archive_00000001.lms` | 9 | 0 | 36 |
| `archive_00000002.lms` | 11 | 0 | 468 |

Both have complete valid graphs with 1500 total particles per manager. Their
first controller pool is `8128C4A0`, second is `81283B00`; crash A's controller
`81274D30` belongs to neither. These are good read-only positive fixtures, not
evidence that the crashed source was intact or that either export caused it.
