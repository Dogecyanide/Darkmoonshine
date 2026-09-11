# 0.3.35 crash B: missing element-meter and sibling HUD owners

This is a read-only diagnosis, not a claim of a runtime fix. The supplied
`luigis_mansion_crash_b.txt` reports generation 7, mod CRC `8B437156`, completed
load phase `7F`, then a DSI at `8003BBB0`, DAR `0000003E`. The separately
exported SD archives are not established as the snapshot that caused this
crash; no causal pairing or cross-session load is assumed here.

All retail instructions below were checked against clean GLMJ01 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`. The seven executable evidence tests
in `scripts/test_lm_hud_siblings.py` pass. They authenticate the lifecycle and
boundaries, not Wii behavior.

## Exact failing ownership chain

- `r29 = 803C4850` is the final element-meter wrapper.
- `8003BBA0` loads wrapper `+10`: crash register `r3 = 81364738`.
- `8003BBAC` loads picture `+EC`: crash register `r6 = 0`.
- `8003BBB0` executes `lhz r7,0x3E(r6)`, explaining DAR `3E` exactly.
- The caller `80045DCC` passes `803C4718 + 138 = 803C4850` to `8003BB5C`.
- Construction at `80045840..48` loads that same wrapper from the resource
  `/kawano/emeter/meter_base.bti`, whose string is at `802FB430`.

This is the Poltergust element-gauge background, not the Game Boy Horror
shadow fixed in 0.3.34. The instruction is a read of a null texture pointer;
there is no evidence here of a new out-of-memory allocation failure. The
snapshot restored the controller/SBSS and GAME allocation generation while
leaving this fixed owner array in the later scene generation. Native scene
reload destroys and reallocates these picture objects. That ownership gap
is independently proven even though the crash does not contain the omitted
save-time array, so the exact original picture address cannot be recovered.

Merely skipping the null texture draw would hide one consumer of the stale
pointer; the native cleanup later calls the object's destructor through that
same pointer. The correction belongs in the ownership snapshot, not that
draw instruction.

## Minimal complete sibling extension

The audit followed the children of scene HUD construction `8003D418` and
cleanup `8003E4C8`, then the immediately related mission Goodnight picture.
Four native picture tables are missing, representable as three exact ranges:

| Proposed range, exclusive end | Bytes | Derivation |
| --- | ---: | --- |
| `803C4448..803C4628` | `1E0` / 480 | One Goodnight picture plus 19 timer pictures, all `18`-byte wrappers |
| `803C4718..803C4868` | `150` / 336 | Ten gauge levels, three element symbols, one gauge background |
| `803C49C0..803C49D8` | `18` / 24 | One Boo-radar picture |

Total new static state: **840 bytes**. The already-captured room-name table
`803C4628..803C4718` sits between the first two ranges and must not be copied
twice or silently included in the existing room-name validation count.

With only these additions to the current format-19 layout, static payload
`16FF4` becomes `1733C`, camera sidecar offset `1713C` becomes `17484`, and
aligned heap offset `17440` becomes `177A0` (hex throughout). Integrating the
layout needs a format bump and corresponding packing/size assertions. These
figures are conditional on no other concurrent new capture ranges.

### Element gauge: fourteen wrappers

`8003DAB4..DACC` allocates a `20`-byte controller, constructs it through
`80045724 -> 800457A8`, and publishes it at `r13+620 = 804A1100`.
`800457A8` allocates the ten wrappers at `803C4718`, three at `803C4808`,
and one at `803C4850`, using the common picture constructor `8003B99C`.
The tables contain 10 + 3 + 1 elements of stride `18`.

Static construction `80045F04` separately proves the same array counts,
stride, and final wrapper. Scene HUD cleanup `8003E5FC` calls
`80045754 -> 80045E74`, which destroys all fourteen wrappers. The root and
scalar fields `804A1100..804A1110` are already in captured SBSS; the controller
and picture bytes are in GAME. Only the BSS wrappers were omitted.

### Native timer: nineteen wrappers

The HUD creates a `54`-byte controller at `8003DA60..DA78`, rooted at
`r13+5F8 = 804A10D8`. Constructor `80043E40 -> 80043EC4` creates three
explicit pictures at `803C4460`, `+18`, `+30`, then sixteen from `+48`, all
through `8003B99C`. Their resources are under `/kawano/newtime/`.

Cleanup `8003E5A8 -> 80043E70 -> 800447CC` destroys the same three plus
sixteen. Static constructor `80044840` verifies that exact layout. Thus
`(3 + 16) * 18 = 1C8`, ending exactly at the room-name table `803C4628`.
The timer may be hidden in normal gameplay but its pictures are still
created; avoiding the visible crash does not repair later cleanup ownership.

### Boo radar: one wrapper

The HUD creates an `18`-byte controller at `8003DAEC..DB04`, rooted at
`r13+6B0 = 804A1190`. Constructor `8004FFFC -> 80050080` creates the single
wrapper `803C49C0` from `/kawano/teresar.bti` at `8032ADB8`. Cleanup
`8003E614 -> 8005002C -> 800504E8` destroys it. Static constructor
`80050510` initializes the same `18`-byte wrapper.

### Goodnight picture: one wrapper in the same mission lifecycle

Mission setup `8000BFF4 -> 80043C64` creates wrapper `803C4448` from
`/kawano/goodnight/Gnight.bti` at `802FA798`, publishes a small controller
through `r13+5E8 = 804A10C8`, and initializes its scalar animation fields.
Mission cleanup `8000BE64 -> 80043DD4` destroys the same picture. Its static
constructor is `80043E18`. This is not a boot-persistent font. The wrapper
ends exactly at the native timer table, permitting the first combined range.

## Coverage of the other direct scene HUD children

- GBH wrappers, screen roots, pane aliases, and fade controllers are already
  covered by `803C3238..803C3388` plus the existing SBSS range.
- Digit and health-display pictures are already covered by
  `803C3400..803C3730`; their controller root at `r13+564` is in SBSS.
- The room-name presenter owns the already-captured
  `803C4628..803C4718`; controller root `r13+600` is in SBSS.
- `80045F78 -> 80045FFC` constructs a six-byte scalar controller, rooted at
  `r13+630`; its cleanup `8004604C` is empty. It adds no fixed picture table.
- `800552AC` is conditional on map `0C`, creates a heap controller, and
  publishes roots in `r13+6F8/+6FC`. Those globals are captured already.
  Its asynchronous setup is not permission to weaken existing I/O gates.
- `80056448 -> 8005544C` creates a heap controller and a J2DScreen with roots
  at `r13+70C/+708`. Pane aliases and saved rectangles are stored in that
  heap controller, not a separate fixed BSS owner array. The current GAME
  and SBSS snapshot covers these roots. Cleanup is `80056564`.

This is a bounded HUD-child audit, not a claim that every unrelated mission
subsystem has complete cross-reload coverage.

## Bounds, validation, and cache pitfalls

`8003B99C` allocates `17C` bytes from the current heap through `801C9308`,
constructs the picture with vtable `802F97DC`, and stores it at wrapper `+10`.
Each wrapper is plain CPU state: coordinates/scalars, RGBA, and picture
pointers `+10/+14`. It is not a GPU FIFO or hardware texture object. Native
cleanup `8003C21C` destroys non-null `+10/+14` but does **not** clear those
fields, so pointer generation has to rewind with the heap.

Use the existing capture/restore/flush loop for the added ranges so cached
CPU owners and the copied GAME heap become coherent before returning to the
game. Preserve the existing draw barrier and texture-cache invalidation;
an extra texture invalidation cannot repair an incorrect CPU owner pointer.
Do not run native scene destructors after copying the saved heap: their
live-generation roots would destroy the wrong allocations.

The useful preflight check is bounded picture ownership in both saved and
live address spaces (full picture range, expected vtable, texture dimensions
readable), with an explicit diagnostic on rejection. A null root can be
legitimate, but a non-null wrapper whose picture has a null texture is not
drawable by these native consumers. If adding such validation, authenticate
any stronger assumption about optional `+14` owners before requiring zero.
The current room-name checker alone only verifies ten room-name wrappers.

Stop the element range at `803C4868`. The following `88`-byte block belongs
to another subsystem: `80046050` fills it with scalar vectors while creating
and switching a separate solid heap (`801CADDC` / `801C8E94`). Its following
`803C48F0` symbol is a `C`-byte registration-sized record. The element table
does not establish that these neighbors are safe to capture. Similarly stop
the radar wrapper at `803C49D8`, before the next independent BSS objects.

Keep the existing font/destructor exclusion `803C3388..803C3400`, and do not
capture the unproven inventory/text buffers `803C3730..803C4448` simply to
make a visually contiguous HUD block.

## Focused regression route

Use fresh format states. Save, use the room-warp menu, load; then move,
switch/consume elements, trigger the Boo radar, show any native timer,
open GBH and a room door, and repeat. Include the runner's stronger failure
sequence: first refused pre-warp state, save a new state in the new scene,
warp again, load, then use a door. A successful first frame is not enough:
the same owner tables also participate in the following cleanup.
