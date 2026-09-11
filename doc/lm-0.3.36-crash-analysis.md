# 0.3.36 post-menu-warp crash: overwritten GAME free-list node

Analysis and bounded diagnostic helper, 2026-09-07. No runtime save import,
SD modification, native-memory repair, or authentication bypass was used.

## What the fresh Wii data proves

Preserved data is in the workspace sibling
`sd-captures/lm-0.3.36-user-20260907-first`. The current crash is **B**,
generation 5, mod CRC `692195EB`, code size 151096, 58 hook writes.
Crash A in that directory is older and must not be attributed to this build.
The reported sequence was save in Parlor, menu warp to Storage Room, load
Parlor, then crash after gameplay returned.

The current DSI is `SRR0=801CA6E4`, `DAR=00000007`,
`r3=r4=FFFFFFFF`, `r6=812A23C0`, `r30=80BE44D0` (GAME).
Authenticated clean Japanese DOL SHA1 is
`722005ea9c1eab54b114f814734d8f327e5614ee`.

`801CA61C` is `JKRExpHeap::check`. Its free-list loop does:

| Instruction | Meaning at this crash |
| --- | --- |
| `801CA6C4: lwz r6,0x74(r30)` | Begin at GAME free-list head |
| `801CA6CC: lwz r3,0x0C(r6)` | Current node's next pointer |
| `801CA6D0: lwz r4,0x04(r6)` | Current node's payload size |
| `801CA6E4: lwz r0,0x08(r3)` | Read next node's previous link |

Thus both node `812A23C0` fields `+4` and `+C` were `FFFFFFFF`.
The next-pointer dereference wraps to address 7. This is a corrupted free-list
record, **not evidence of allocator exhaustion**. The diagnostic heap walker
is where the corruption was detected, not an established writer.

Immediate load phases `76`, `78`, and `7A` all returned one for ROOT, SYS,
and GAME. Phase `7F` and event 257 record completed restore. The restored
free-list graph therefore passed the same native walker before later becoming
invalid. This does not prove that every other saved owner was valid, or even
that the eventual bad node already belonged to the free list at `7A`.

The final `ndebug.log` sequence records post-load frame counters 2 through 8,
then phase `87` and the crash report. The user's brief visible return and the
journal's sample cadence should not be treated as an exact two-frame writer
timestamp. `sampleHeapChecks` runs every 60 valid presenter samples and does
not reset that counter on load; the crash enters it after the initial eight
fully traced post-load frames. The last log entry does not establish when the
first overwrite happened.

## A concrete retained render-destination lead

The matching .36 state was not exported: `D:/lm_states` is empty. Old private
`.30` MEM1 captures are comparative evidence, not the crashed saved heap.
In the first old capture, address `812A23C0` falls inside allocation:

- Allocation header `81293DF0`: tag `484D000D`, payload size `96000`.
- Payload `81293E00`, size 640 x 480 x 2 bytes.
- The sole aligned pointer to this payload in that complete MEM1 image is
  at fixed address `803C4C14`, outside the .36 static snapshot ranges.

Independent render-owner audit identifies this as fixed owner `803C4B6C`
plus `A8`. Native scene setup `8000C0D0` in group `0D` calls `8005C798`,
allocating the `96000` destination and a `20000` companion at owner+`AC`.
The GX copy path consumes the retained destination. Recreating GAME during
a menu warp can relocate these allocations while leaving the excluded owner
in the destination epoch; raw GAME rewind then pairs that owner with the
saved layout. A later GPU copy into memory now used by saved allocator
headers is consistent with healthy post-copy heap checks followed by bulk
`FFFFFFFF` corruption. The old capture alone does not prove the exact .36
destination address; the native ownership omission is the correction's basis.

The separate render-owner validator/native tests own the exact object span,
texture descriptors, allocation membership, and capture integration. Do not
copy the whole renderer/SDK area, infer allocation ownership from an interior
`HM` tag, or force a stale destination to an arbitrary free block.

## Safe diagnostic correction

`include/susamune/lm_exp_heap.h` provides:

`LmExpHeapValidate(context, readWord, heap, faultAddress, faultValue)`

It performs no allocation, locking, mutation or repairs. Its bounded callback
reads validate complete header and block extents **before** following a link.
It checks JKRExpHeap type/bounds, both list heads/tails, reciprocal previous
links, valid used/free magic, aligned block sizes and padding, sorted
non-overlapping free ranges, and native total-byte accounting. Native used
accounting includes `(header byte +2) & 0x7F`; the high tail-allocation bit is
not padding. Zero-payload free headers are legitimate and accepted.

There are at most 8192 nodes per list; a private gameplay fixture has 1306
used and 34 free nodes. Complexity is linear and stack space is bounded.
This intentionally does **not** add an expensive all-pairs proof that every
unsorted used allocation is disjoint. It is a safe structural diagnostic,
not a replacement for resource/model ownership validation or authentication.

Use a quiescent boundary. The helper is not a synchronization primitive and
cannot make a subsequent unchecked native traversal safe from concurrent
heap mutation. On a bad field it reports that field's storage address and
value, rather than dereferencing the corrupt pointer. A bad aggregate total
reports heap+`38` and the computed total. A failed read reports its attempted
address and zero. Never repair a list or continue gameplay after post-copy
corruption; preserve the first fault and enter a controlled diagnostic stop.

The concrete crash-shaped regression first accepts a synthetic healthy heap
at the actual GAME/free-node addresses, then rejects `812A23CC=FFFFFFFF`
without any foreign read. With both captured bad fields changed, it rejects
the earlier size field `812A23C4=FFFFFFFF`.

The checked `8B` boundary is the end of `8000B248`, a scene **draw** wrapper:
it loads matrices, calls scene+`1C` at `8000B35C`, and resets projection.
The caller at `8000B538..544` enters it only in main-loop mode 2. Full scene
cleanup is scene+`20` at `8000B724`, outside that inner loop after exit global
`804A0C28` breaks it at `8000B644..658`. Therefore ordinary teardown does not
interleave inside this draw boundary. For the presenter check across a new
scene/startup cycle, gate on main-loop mode `80398A40 == 2` and exit == 0;
do not suppress a failure merely because the heap pointer or shape is bad.
This retains detection of malformed GAME data in an otherwise live scene.

The helper's PPC maximum nested stack is 192 bytes (64 validator + 96 list +
32 expected-read), excluding its callback/caller. Failure telemetry uses
POST_LOAD `F4`; old audio checkpoints already use `F0`/`F1`. Do not label or
flush ordinary audio tracing as a heap failure.

## Regression requirements

1. Authenticate the exact clean-DOL free-loop, padding, reciprocal-link and
   accounting instructions; run the production native helper, not a Python
   approximation. Mutate null/foreign/misaligned pointers, sizes, previous and
   next links, endpoints, cycles, free overlaps, totals, callback failures and
   the traversal cap. Both private captures' ROOT/SYS/GAME heaps must pass.
2. Restore the complete audited render-destination owner and its embedded
   texture metadata with the same GAME epoch. Validate saved and live
   destination allocation membership and non-overlap before writes.
3. Repeat Parlor save -> menu warp Storage -> load, staying idle past the old
   failure, moving the camera, opening doors, and repeating save/warp/load.
   Include same-room menu reload and normal door travel as controls.
4. Run safe GAME validation at the frozen post-copy boundary and bounded
   early post-load draw/presentation boundaries. Retain the first failing
   field and phase. If corruption persists, compare the validated saved/live
   render destinations and completed GPU-copy boundaries; do not treat a
   diagnostic refusal or an absence of the native DSI as a gameplay fix.
