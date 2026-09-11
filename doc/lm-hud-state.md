# Japanese LM scene-owned HUD snapshot roots

## Why the .33 load crashed

The Wii capture `sd-captures/20260907-004034/luigis_mansion_crash_a.txt`
reports mod CRC `A9D47118`, completed `LOAD 7F`, then a DSI on the first draw:

- PC `8003BB3C`, DAR `0000013F`.
- Wrapper `r31 = 803C3298`; its picture pointer is `8135F448`.
- The picture's word at `+EC` is `00000101`.
- Retail executes `lhz r7,0x3E(r6)`, explaining `101 + 3E = 13F` exactly.
- The journal contains successful earlier loads, then a native menu reload to
  the Anteroom before this final load.

The native caller at `8003D224` draws the wrapper at `803C3238 + 60`. Its
resource is `/kawano/base/cgbk_kage.tim`: the Game Boy Horror shadow, not a
particle or fountain model. The `.33` snapshot restored the GAME heap without
restoring this BSS ownership table. A native reload can replace the picture
allocation, leaving the newer wrapper pointing into an older, restored heap.
That is the narrow mismatch supported by the crash and retail lifecycle.
The report does not contain the saved version of that omitted table, so it
cannot prove the exact old picture address or rule out every other defect.

## Two bounded ranges

| Range, exclusive end | Size | Native ownership |
| --- | ---: | --- |
| `803C3238..803C3388` | `150` | Nine Game Boy Horror picture wrappers, four screen roots, pane aliases and fade-controller records |
| `803C3400..803C3730` | `330` | Twenty-one digit/display picture wrappers followed by thirteen health-display wrappers |

Total: `480` hex, **1,152 bytes**. Snapshot format 18 includes these ranges;
older layouts must be refused by the existing exact-format checks. GAME heap
addresses and live OS/GX/DVD/audio state are not widened or reset by this fix.

The first range breaks down as follows, relative to `803C3238`:

- `000..0D8`: nine `18`-byte picture wrappers. Each contains three scalar
  words, an RGBA word and two CPU object pointers.
- `0D8..0E8`: four heap-backed `J2DScreen` roots.
- `0E8..108`: two arrays of four pane pointers returned by screen lookup.
- `114..150`: five `C`-byte fade-controller records containing a pane pointer,
  two halfword counters and a byte target; the interval includes their native
  alignment/padding. The first controller is at `114`, followed by four at
  `120`, `12C`, `138` and `144`.

These are CPU-side scene/UI objects and scalar animation state, not OS
message queues, GPU FIFO ownership or asynchronous device handles. The normal
existing render barrier still applies. Their pointed-to picture/screen memory
must remain covered by the GAME snapshot and its existing heap checks; copying
the tables alone is not a general method for preserving arbitrary resources.

## Authenticated Japanese retail evidence

All addresses and opcodes below are checked against the clean GLMJ01 DOL,
SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee`.

### Creation, ownership and the failing draw

`8003B99C` allocates a `17C`-byte custom picture through `801C9308`, constructs
it through `8003AAF4`, then publishes the result at wrapper `+10`.
`801C9308` uses the current heap pointer at `r13+1514` and its allocation
virtual function. The custom picture's final vtable is `802F97DC`.

`8003CA7C` loads six GBH wrappers at offsets `48..C0`; `8003D418` loads the
first three and calls that function. `8003E67C` separately constructs the
nine static wrappers with stride `18`. `8003BAF8` checks only the wrapper's
picture pointer for null before reading picture `+EC`, then texture dimensions
at `+3C/+3E`. This exactly matches the .33 fault.

`8003D418` allocates the four `F4`-byte screen objects and stores them at
`D8/DC/E0/E4`. It looks up two groups of four panes and initializes their
fade controllers through `8003B0E8`. `8003E4C8` destroys those screens, clears
their four roots, and calls `8003C21C` for every picture wrapper. The picture
cleanup destroys non-null objects at `+10` and `+14`; it does **not** clear
those two pointer fields itself. That makes restoring their generation with
the heap particularly important.

The scene initialization wrapper calls the HUD constructor at `80037B4C`;
the native scene cleanup calls the HUD cleanup at `8000BE30`.

### Digit and health tables belong to the same lifecycle

`8003D418` calls `8003F200` at `8003DA5C`. It constructs two ten-element
picture arrays at `803C3400` and `803C34F0`, plus one picture at `803C35E0`.
Cleanup `8003F38C`, called at `8003E59C`, destroys those same 21 wrappers.
The static constructor at `80040128` independently proves the two arrays'
element counts and stride.

The thirteen wrappers at `803C35F8..803C3730` are initialized by the static
constructor `80041900`: seven explicit wrappers plus six array elements.
The HUD constructor allocates its health-display controller and calls
`80041854 -> 80040870`, which creates the scene's pictures and clears the
dynamic wrapper roots. HUD cleanup calls `8004188C`, which destroys that
controller and its owned picture wrappers. Other entries are display aliases
and scalar state within the same typed wrapper table.

### Deliberately excluded neighboring state

`803C3388..803C3394` is a **12-byte global destructor registration record**.
`8003F1C4` constructs the font at `803C3394` and passes the record address to
`801F51C8`, which links it into the runtime destructor list. It must not be
rewound with a room.

`803C3394..803C3400` is the persistent `6C`-byte font object. Initialization
`8003E70C` is called from boot initialization at `8000E728`, not the scene HUD
constructor/cleanup pair. Its existence next to the HUD tables is not a reason
to restore it as scene-owned state.

The preceding four wrappers at `803C31D8..803C3238` belong to a different
initialization/cleanup path (`8000EBF0/EBFC` and `8000ED70`). They are not added
by this change. Nor does this audit authorize capturing the remainder of
nearby BSS, shared SYS archives, or unrelated menu inventory buffers.

## Regression coverage and remaining verification

`scripts/test_lm_hud_state.py` has nine tests covering the exact integrated
source ranges, excluded gap, constructors, cleanup, counts/strides, current-
heap allocation, the failing draw and the exact resource string. Retail tests
refuse a DOL with a different digest and skip only when no clean DOL is present.
They validate the reverse-engineering evidence, not real-time Wii execution.

Run `venv/Scripts/python.exe -m unittest scripts.test_lm_hud_state -v`.
All nine passed on the authenticated DOL with the format-18 source integration.

Hardware validation still needs the original pattern: save in the Parlor,
native/menu reload to another room, load, then move and use the door repeatedly.
Success here is not a claim of arbitrary-room or across-reboot SD portability.
