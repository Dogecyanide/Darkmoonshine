# Native scene-picture ownership: the 0.3.38 cleanup failure

## Exact failing chain

The preserved new Wii report A in workspace
`sd-captures/lm-0.3.38-user-20260907-anteroom` is generation 8, mod CRC
`2E6F16CD`. Report B is still the previous `.37` dialogue-owner failure;
it is not a second new `.38` crash.

The new DSI is at `800286A4`, `DAR=AFEFF007`, `r30=803C1C60`,
`r3=81248830`, `r12=AFEFEFFF`. Native cleanup loaded the first pointer of
the fixed table at `803C1C60`, then attempted `picture->vtable->destructor`
through the invalid vtable `AFEFEFFF`. The `+8` virtual slot explains DAR.
The stack returns through `80011668`, `8000BE7C`, and the Mission loop.
`80011668` is the return address after the call **at `80011664`**; the
instruction at `80011668` itself starts the next cleanup.

The constructor root table is omitted from format 23, while its pointed-to
GAME objects rewind. This is the same ownership inconsistency class as the
previous dialogue failure, but a different native subsystem reached later
in cleanup. It is not evidence that the dialogue fix regressed, or that a
new allocation ran out of space. A valid GAME heap does not establish that
every live fixed pointer still names an object in that saved generation.

All executable addresses below refer to clean Japanese revision-0 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`.

## Complete picture table and its lifecycle

`803C1C60..803C1C88` is exactly **ten four-byte picture pointers**, not a
single picture object. Initializer `80027D9C` establishes its base at
`80027DB8`, clears eight entries explicitly, then clears the remaining two
through the bounded tail at `80027DF0..80027E08`.

For mansion map `2`, it creates an eleven-element actor array, stride `388`,
using native array allocation and construction at `80027E18..80027E44`.
Its root `r13+370 = 804A0E50` and signed-halfword count at `804A0E54` are
already captured SBSS. Per-actor auxiliary allocations and the actor array
itself are inside GAME.

The initializer then creates four native J2DPictures, each `15C` bytes,
through `801C9308` and constructor `801AE840`. Stores at `80028264`,
`8002829C`, `800282D4`, and `8002830C` populate table slots 0 through 3.
The remaining six stay null. Their resources in the retained parent archive
are `/iwamoto/bath.bti`, `/iwamoto/bed.bti`, `/iwamoto/zizi_k.bti`, and
`/iwamoto/tea_k.bti`. Constructor `801AE840` assigns vtable `80386E60`.
This differs from the `17C`-byte derived pictures in the dialogue manager;
do not reuse that object's size or vtable in a validator.

The native lifecycle callers are:

| Operation | Caller and target |
| --- | --- |
| Scene initialization | `80010A68 -> 80027D9C` |
| Actor update | `800111A4 -> 8002837C` |
| Scene drawing | `800115B8 -> 80028594`; actor draw reaches `80027948` |
| Cleanup | `80011664 -> 80028610` |

Draw `80027948` selects a table entry using the actor's `+7C` index, then
reads its texture state (`+FC`, `+EC`) before rendering. Cleanup `80028610`
first frees each actor's `+128` auxiliary allocation, then deletes the actor
array rooted in SBSS. It finally loops over **all ten** fixed picture
pointers at `8002868C..800286BC`, virtual-deleting every non-null object.
It does not clear these pointers after deletion. Rewinding the table is
therefore required for drawing and later destruction, even when a load's
first frames look normal.

## Adjacent scalar state and safe capture span

The adjacent native symbol `803C1C88..803C1C98` is a three-float view origin
plus four padding bytes, not another array of heap owners. `800289F4..80028A30`
writes X/Y/Z from the current camera, with an alternate Y derived from the
current room. `80028F90..80028FA8` copies this retained vector into an actor's
position when its alternate camera path is selected. Other consumers at
`80028AA8`, `80028C70`, and `800290DC` use the same fixed vector. A consumer
does not necessarily refresh it immediately before reading it.

Recommended bounded capture: **`803C1C60..803C1C98`, `38` bytes**. This keeps
the full ten-pointer table and its nearby view scalar state coherent. The
strictly necessary picture-ownership portion is `28` bytes; the additional
`10` is independently established scalar state, not speculative pointer data.
Neither native symbol contains an allocator, OS queue/mutex, GPU FIFO, or
global-destructor registration. No new GAME allocation or MEM2 region is
needed: all pictures and their resources are already covered by GAME and
the shared-parent companion.

**Stop at `803C1C98`.** The following `430`-byte symbol is a separate
scene-inspection selector/camera family. Its position beside the picture
table does not make it part of the picture-ownership repair. The bounded
follow-up audit establishes:

- `+000..3FF` holds 256 four-byte selector records, not object pointers.
  `80029E74` rebuilds the active records from a source descriptor: byte 0
  selects a floor, byte 1 a model, and byte 3 a rendering variant; byte 2
  is padding. The source count is a byte, and the destination advances by
  four bytes. The active view update calls this builder at `800298EC`;
  consumers at `800299E8` and `80029C30` interpret the individual bytes.
- `+400..42F` holds four XYZ vectors, not heap pointers. `8002AC94`
  initializes the reference position from static scene data; `8002A09C`
  writes the default camera target; `8002A0B4` and `8002A1B4` initialize
  the current view. `8002A360` retains and integrates manual camera input
  through `+424..42F`, then smooths the current position at `+40C..417`.
  These vectors are persistent mutable scalar state, so the entire symbol
  must **not** be described as read-only or regenerated every frame.
- The actual controller/model-array ownership roots are separate SBSS
  fields `r13+3A0`, `r13+3A4`, and `r13+3C4`, already inside the captured
  SBSS ranges. Setup at `80029688` / `8002AFC8` stores those roots, and
  cleanup at `80029E08` consumes them. The selector/vector symbol is not
  the root of those allocations.

Both private `.30` fixtures have this entire `430`-byte symbol and its
subsystem's SBSS roots zero. They demonstrate an inactive subsystem only,
not correctness of an active-view rewind. Exclude this family from the
`.39` ownership repair: it is not another proved GAME destructor omission.
This exclusion does not establish complete snapshot coverage for its
persistent camera state or claim that all active view modes are tested.

The existing font/destructor exclusions elsewhere remain unchanged. Use the
normal captured-static loop, with matching restore and cache writeback before
resuming the game. Do not zero the table, invoke destructors as a repair,
or skip the faulting virtual call: those would sever legitimate ownership.

## Independent old-capture reproduction

Both private `.30` MEM1 captures show four exact GAME used-list allocations
of `15C` bytes, allocation group `01`, and vtable `80386E60`; slots 4 through
9 are null. Their table pointers are:

| Fixture | Slots 0, 1, 2, 3 |
| --- | --- |
| `diagnostic-capture-0.3.30` | `81248A90`, `81248BFC`, `81248DCC`, `81248F9C` |
| `diagnostic-capture-0.3.30-newreport` | `81248870`, `81248A40`, `81248C10`, `81248DE0` |

Combining the later roots with the earlier GAME image yields vtable words
`42480000, 0, 0, 0`. Reversing the pair yields `0, 0020F808, 00AA0014, 0`.
Neither combination is valid, although both complete endpoints are valid.
The retained camera vector also differs. These older fixtures demonstrate
the mechanism; they are not claimed to be the new Wii crash's snapshot pair.

## Regression and validation proposal

Authenticate the ten-entry initializer and destructor bound, four constructor
stores, object size/vtable, correct return-address attribution, and scalar
vector boundary. Test both complete private endpoints and their mixed-root
failure. Source tests should require the entire selected span in the single
save/restore/writeback manifest and a snapshot-format increment.

If adding a portable preflight guard, allow legitimate null entries and prove
each non-null pointer against bounded GAME used-list membership, native object
extent and expected vtable in the same saved/live image. Report exact slot and
field on failure, before writes. Do not assume all ten slots are populated or
mistake the actor count of eleven for the picture table size of ten.

The key Wii sequence remains: fresh save, menu warp, load, then another menu
warp and a normal door. Follow this with repeated loads/warps, including rooms
that use these scene textures. A successful first frame cannot validate the
subsequent cleanup. No new hardware pass is claimed by this audit.
