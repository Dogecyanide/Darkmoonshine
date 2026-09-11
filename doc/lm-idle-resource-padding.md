# Idle resource-record padding across room reloads

The 0.3.36 generation-29 journal rejects Parlor → menu-warp Anteroom → load
with X06, resource fault 2, active-ID mask `0F`, record mask `7F`, and differing
record-array layout. This isolates changed rows 4–6 with unchanged IDs, but the
journal does not contain their complete saved/live bytes. It therefore cannot
prove which individual byte caused this particular refusal.

Clean GLMJ01 revision-0 DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee` provides a definite false-refusal case:

- Constructor `8001F928–8001F96C` leaves exactly `+3`, `+4..7`, `+18..1B`,
  and `+3E..3F` unwritten in each 64-byte record.
- Cleanup's clearing tail `8001F1A0–8001F1E0` writes the identical set of bytes.
- State is a byte at `+2`; flags are a halfword at `+0`. Counts are bytes at
  `+3C/+3D`. The untouched neighboring bytes `+3/+3E/+3F` are padding.
- Reuse `8001F2B4` publishes pending state at `8001F2D8`, replaces backing at
  `8001F2DC`, and replaces callback at `8001F378` before returning.

The existing idle exception already ignored stale callback/backing. It now also
ignores those three padding bytes, which can differ after allocator relocation.
Both IDs must still be inactive, both states zero, all four auxiliary pointers
zero, and every other semantic byte equal. No pending/active/unknown state is
newly admitted. Existing allocation, owner, map, and global-I/O checks remain.

Regression tests authenticate every constructor/cleanup store and test all 64
record-byte mutations. A synthetic three-idle-row relocation matches the
journal's mask shape; this is explicitly not a reconstruction of missing tester
record bytes. If the next refusal persists, per-row semantic-byte telemetry is
needed before any further change to equivalence.
