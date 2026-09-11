# Japanese shared resource archive: retained payload proof

This supplements `lm-sys-resource-audit.md` with authenticated GLMJ01 native
lifecycle anchors. It authorizes no blanket SYS heap restore and makes no
cross-reboot claim. `scripts/test_lm_shared_archive_native.py` checks the
anchors against clean DOL SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee`.

## Creation and lifetime

Boot's loader at `8000E3C4` requests the Japanese `/Game/game.szp`, setting
callback `8000E338` and group 16. The transport request is made with the GAME
heap current; `8000E408..8000E40C` then makes SYS current again. The callback:

1. Reads the decompressed, 32-rounded length through `80007098`.
2. Allocates a separate 32-aligned destination through `801C8EA4` at `8000E360`.
3. Decompresses the DVD block into that destination at `8000E374`.
4. Releases the temporary DVD block at `8000E37C`.
5. Allocates a separate `0x68`-byte `JKRMemArchive` owner at `8000E380..384`,
   constructs it with backing-free flag zero, and publishes it at `8000E3A8`.

The setter `80066328` writes `r13+7D0`, global `804A12B0`; getter `80066330`
reads it. The clean text has one direct store to this SDA slot and one direct
call to the setter, from that boot callback. Mission cleanup calls `80066324`
at `8000BE68`; this routine is just `blr`. This path retains the parent through
Mission replacement rather than reopening its data for every room or menu warp.
Runtime owner/allocation checks remain necessary: absence of another direct
SDA setter is not a general proof against every possible aliasing write.

The two captured SYS used lists independently establish the callback's final
allocation ownership. The resource is one used SYS allocation, not a subheap:

- Resource payload: `807BAFE0..80BD4AE0`, exclusive end, `0x419B00` bytes.
- Its `CMemBlock`: `807BAFD0..807BAFE0`, **excluded**.
- Separate mounted owner: `80BD4AF0..80BD4B58`, **excluded**.
- The owner's allocation header starts at `80BD4AE0`, **excluded**.

These addresses are fixture observations, not hardcoded universal addresses.
The payload's complete top-level paths and lengths match the user's clean
Japanese disc archive. OS threads/stacks/message queues, the GX FIFO/XFBs,
ARAM owners and audio arena are different SYS allocations; see the parent
audit's full 37-allocation inventory. The `0xC0000` audio arena is explicitly
allocated independently at `8000E694`. Boot UI ARAM transfers read another
archive global, `r13+1C4 = 804A0CA4`, not `804A12B0`.

## Borrowed model ownership

The model loader at `800617A4` gets the retained parent, then calls its
`getResource` vtable entry at `800617B8`. It allocates a new `0x68` GAME wrapper
and passes that returned file pointer directly to `JKRMemArchive` at
`800617EC`. Consequently a new GAME archive handle can describe the same SYS
resource bytes without the SYS payload being freshly allocated.

The parent constructor passes free flag zero; nested constructors pass one.
Do not require all nested wrappers to have `+64 == 0`. Their disposer heap is
GAME while their archive backing heap is SYS; these are distinct ownership
fields. Rewinding GAME captures nested owners/disposer state, not the retained
SYS owner's disposer link. Its resource-volume links are handled only by the
existing validated volume-list repair.

`JKRMemArchive::open`, `801CEC88..801CED30`, derives info/table/string/data
pointers from the supplied RARC. `fetchResource`, `801CED34..801CED64`, writes
`dataBase + fileOffset` to the entry's cached data pointer; it creates no I/O
request or live OS object. The fixture has 898 nonzero caches, all pointing to
their own archive's file bytes. Full payload capture must include these outer
tables as well as the 18 nested archives. Capturing only the nested span misses
mutable outer door matrices and other shared resource state.

## Required runtime proof and restore order

Before permitting a changed GAME wrapper whose backing is SYS:

1. Validate saved/live SYS heap identity and the actual used allocation,
   including allocator signature, size, bounds and list membership. The payload
   extent must exactly describe the retained parent data allocation; no adjacent
   allocator header or owner object is copied.
2. Validate saved/live parent pointer, memory-archive vtable, mounted state,
   type, backing identity, allocation/disposer heaps, mount mode and free flag.
   Its header/info/table/data/name endpoints must still describe that same
   retained allocation. Save-time metadata belongs to the authenticated state.
3. Verify each changed wrapper itself is GAME-owned and all claimed SYS backing
   bytes are wholly within the proven captured parent payload. Names alone,
   unchanged list head/tail, or a 64-byte RARC hash are insufficient. An unknown
   SYS object or buffer outside this domain still fails closed.
4. Complete all rejection checks before writing. Require resource/DVD loading
   quiescence and GPU completion. Restore the payload before any restored GAME
   model/scene consumer runs, then perform CPU writeback and GX vertex/texture
   cache invalidation before rendering resumes. Keep live OS/transport machinery.
5. Reapply current colour/settings after restoring mutable resource bytes, and
   invalidate mod-side pointer/cache assumptions that may refer to the old scene.

The shared payload is intentionally mutable: Luigi morph output, material and
animation state, door-node matrices and resource caches all live in it. Comparing
the entire payload for equality is safe as a refusal rule but cannot substitute
for capture if ordinary pose changes must rewind. Conversely, loading pristine
disc bytes would discard initialized pointers and runtime rendering state.

The current raw snapshot plus the full parent exceeds the original raw slot by
roughly half a MiB before all bookkeeping. Storage bounds, authenticated format,
compression/fallback capacity and exclusion maps must be updated together.
No overflow may fall back to a partial payload restore.

## Hardware checks after integration

- Same-room save, walk/rotate/change pose, load repeatedly.
- Save, same-room menu warp, then load; repeat after saving a new state.
- Parlor -> Anteroom menu warp/load/re-enter and the reverse direction.
- Foyer stairs, then natural cross-floor and several-room routes.
- Save in map 2, enter a boss/map 9 destination, attempt load and return; either
  a complete restore or a clean identity refusal, never a partial rewind.
- Trigger transformations, changing door animation/ghost state and costume
  colours before loading to exercise mutable parent bytes and render caches.
- SD round-trip, intentionally incomplete/corrupt archive, old build/session,
  and forced capacity exhaustion must preserve the previous valid slot.

These tests have not been run on hardware by this analysis task.
