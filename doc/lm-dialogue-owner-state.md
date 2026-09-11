# Dialogue-window ownership after a state load

## 0.3.37 crash evidence

The preserved Wii reports in workspace
`sd-captures/lm-0.3.37-user-20260907-anteroom` are generations 6 and 7,
payload CRC `7B68C87A`. Both finish a state load and subsequently enter a
menu-warp cleanup. Both fail at `8003C248`, with `LR=8004339C` and
`r31=803C4130`. The failing instruction loads a virtual destructor from
`picture->vtable+8`:

| Report | Wrapper `+10` picture | Invalid vtable | DAR |
| --- | --- | --- | --- |
| A / generation 6 | `81393A6C` | `FF200091` | `FF200099` |
| B / generation 7 | `8136F5E0` | `00000000` | `00000008` |

Native `80043378` sets its base to `803C3730` and passes `base+A00` to
`8003C21C` at `80043398`. That common wrapper destructor loads `+10`, loads
the picture vtable, and reaches the recorded fault. It also destroys optional
`+14`; it does not clear either pointer. The stack continues through
`80043BA4`, `8000BE30` and the Mission loop.

These are not allocator-check crashes or proof of memory exhaustion. A pointer
into GAME remains non-null but no longer names its own restored object. The
format-22 static manifest omits this fixed wrapper array, while its controller
root and GAME allocations rewind. No corresponding full save-time snapshot
was supplied with these reports, so exact original pointers are not recoverable.

All native addresses below refer to clean Japanese revision-0 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`.

## The owner is the native dialogue manager

The six explicit resources are `/kawano/dmman/m_window1.bti`, `m_window2.bti`,
`m_window5.bti`, `m_window4.bti`, `hand.bti` and `cursor.bti`. Nineteen more
picture resources follow through a static resource-name table. This is the
native message-window/choice presenter, not the practice overlay or an
inventory-screen object.

Its lifecycle is separate from the ordinary HUD constructor/destructor chain:

- Mission setup selects group `02` at `8000C0F8..FC`, then calls
  `80043AF4` at `8000C104`. That allocates a six-byte controller through
  `801C9308` at `80043B08`, initializes it with `80041980`, and stores it at
  `r13+568 = 804A1048`.
- Mission update calls `80043B34` at `8000B9E4`; that dispatches to
  `80041E24`. Drawing calls `80043B58` at `8000BCC8`, dispatching to
  `80042DD4`.
- Cleanup calls `80043B80` at `8000BE2C`. It reads the controller root,
  invokes `80043378` to destroy its fixed picture wrappers, then deletes
  the controller. Ordinary HUD cleanup is the **following** call at
  `8000BE30 -> 8003E4C8`, explaining why a bounded audit of direct HUD
  children did not cover this sibling.

The root, cursors, animation counters and other `r13+568..5E0` globals are
already inside the captured SBSS interval `804A0CB0..804A1D10`. The six-byte
controller and all picture objects are GAME allocations.

## Exact capture boundaries

The minimum independently proven pointer-owning range is
`803C4130..803C4388`, `258` bytes: **25 wrappers of stride `18`**. Constructor
`80041980` makes six explicit calls to `8003B99C`, then a 19-iteration loop.
Cleanup `80043378` mirrors six explicit calls plus nineteen. Static initializer
`80043BEC` independently specifies those same counts and addresses.

For coherent dialogue rewind, the complete native CPU-state family is
`803C3730..803C4448`, **`D18` bytes**:

| Range, exclusive end | Size | Proven contents |
| --- | --- | --- |
| `803C3730..803C3F30` | `800` | Four 512-byte main-message buffers |
| `803C3F30..803C4130` | `200` | Four 128-byte choice-string buffers |
| `803C4130..803C4388` | `258` | Six explicit and nineteen array picture wrappers |
| `803C4388..803C43E8` | `60` | Four 24-byte colour-channel tables |
| `803C43E8..803C43F8` | `10` | Four message X positions |
| `803C43F8..803C4408` | `10` | Four message Y positions |
| `803C4408..803C4418` | `10` | Four active palette/appearance indices |
| `803C4418..803C4428` | `10` | Four pending palette indices; `-1` means none |
| `803C4428..803C4438` | `10` | Four main-message state values |
| `803C4438..803C4448` | `10` | Four choice-message state values |

The constructor sets the buffer terminators and initializes all six scalar
arrays at `80041A74..80041B7C`. It initializes the four channel tables at
`80041B8C..80041CF4`. Their bytes are consumed as RGBA channels at
`80042EBC..80042ECC`, not interpreted as pointers.

Native setter `800435F4` copies text with `strcpy@801FA484` at `80043644`
into `803C3730 + (index << 9)`. Setter `80043934` copies choice text at
`8004396C` into `803C3F30 + (index << 7)`. Update walks four main buffers,
using the captured SBSS character cursors. Draw copies the selected substring
from the main buffer into stack scratch at `80042E64`; it obtains position,
palette and state from the tail arrays. Pending palette application
`800438FC` moves the selected scalar from `4418` into `4408`, then writes
`-1` to the pending entry. `800439A8` clears the four choice-state entries.

This proves that the intervening bytes are CPU strings and scalar tables,
not a native mutex, OS queue, allocator, or global-destructor registration.
The full span is therefore justified by the native ownership and consumers,
not by a wish to fill a gap in the HUD snapshot. It ends exactly before the
already-captured Goodnight picture at `803C4448`; it must not duplicate that
wrapper. The boot-font/destructor gap `803C3388..803C3400` remains excluded.

## Independent allocation evidence

Both older private `.30` MEM1 fixtures contain all 25 primary pictures as exact
GAME used-list payloads of `17C` bytes, group `02`, vtable `802F97DC`.
Every wrapper's primary pointer changes between these two captures:

| Fixture | First picture | Last picture |
| --- | --- | --- |
| `diagnostic-capture-0.3.30` | `8136F540` | `813723C0` |
| `diagnostic-capture-0.3.30-newreport` | `81382D08` | `81385B88` |

Interpreting the later wrappers against the earlier GAME image produces zero
instead of a valid vtable for all 25 pictures. These old captures are not the
new crash's state pair, but reproduce the ownership mismatch independently.
Both also have zero optional `+14` fields; that observation alone must not
be generalized into a universal rule for every native picture wrapper.

`8003B99C` requests `17C` bytes through `801C9308`, loads the resource from the
retained parent archive, initializes the picture, assigns vtable `802F97DC`,
and stores the primary pointer at wrapper `+10`. The already-captured GAME
and shared-parent data contain the pointed-to objects and texture resources;
no additional allocation or parent-resource admission is needed for this fix.

## Integration and regression checks

Use the existing static capture, restore and cache-writeback loop for the
complete `D18` interval. Do not call destructors to repair stale wrappers,
null their fields, or skip native cleanup. Those actions lose ownership and
can leave later allocation/texture users inconsistent.

With only this addition to the `.37` layout, static size `176FC` becomes
`18414`, camera sidecar offset `17844` becomes `1855C`, and aligned GAME
offset `17B60` becomes `18860`. The aligned core grows by `D00` because
padding shrinks by `18`. A new snapshot format is required; these figures
are conditional on no other concurrent capture additions.

Useful source/host regressions are exact constructor/destructor counts,
authenticated first-fault chain, full buffer/table boundaries, no duplicate
Goodnight capture, and both private endpoint/cross-image tests. A future
portable validator can verify each non-null primary pointer against bounded
GAME used-list membership, expected native object size and vtable, reporting
the exact wrapper/index before writes. Do not weaken existing epoch checks
or add a speculative repair to admit a failed endpoint.

The focused Wii route is save Parlor, warp to Storage, load, then warp to
Anteroom and repeat. Also show and dismiss an actual dialogue/choice menu
between rewinds, then take a normal door and repeat the menu warp. A first
successful load frame does not exercise the teardown that exposed this bug.
This analysis is source/fixture evidence, not a new hardware pass or proof
that every other Mission-owned BSS object is captured.
