# Japanese retail inactive room-resource slots

Evidence from the user's live 0.3.30 Dolphin session, captured without restarting
the game in native checkpoint `GLMJ01.s03` on 2026-09-06. Extracted files are in
`build-lm-emu/diagnostic-capture-0.3.30-newreport/`; `inspection.json` records the
snapshot/live comparison. The retail addresses below were disassembled from its
MEM1 dump, not inferred from symbol names.

## Observed rejected load

The selected snapshot and current game share GAME/SYS/root heap owners, scene,
mission, map 2, and audio scene 0. The epoch difference is only volume head/count
(`0x180`, count 26 to 36). The cross-room guard stopped at Resource (`X06`):

- 7 resource slots, unchanged record/bulk addresses and slot size.
- Saved active IDs: `2, 30, 6, -1, -1, -1, -1`.
- Live active IDs: `34, 25, 30, 33, 29, 24, -1`.
- Active mismatch mask `0x3F`; 64-byte record mismatch mask `0x7F`.
- Both wanted queues empty; both censuses valid; both backing-bad masks zero.

The extra difference is slot 6, inactive in both states. Its record is at
`0x80E99DC4 + 6 * 0x40`. Only two words differ:

| Offset | Saved | Live | Role established below |
|---|---|---|---|
| `+0x04` | `0x00002AF4` | `0x80011B28` | callback |
| `+0x18` | `0x002A8200` | `0x8113CFA0` | input backing buffer |

In both records, byte `+2` is zero and words `+8`, `+C`, `+10`, `+14` are zero.
Every other byte matches. The live backing equals
`0x80E99FA0 + 6 * 0x70800 = 0x8113CFA0`, inside captured GAME heap
`[0x80BE4560, 0x817FB140)`. The saved word is the relative displacement alone;
it is not a valid MEM1 pointer. This is stale idle-record metadata, not evidence
of an active outside-GAME buffer.

## Retail lifecycle proof

`r13` is `0x804A0AE0`. Resource records are reached through `r13+0x228`,
7-slot count through `r13+0x230`, and active IDs start at `0x80398E90`.

1. `fn_80011BE0` marks an unloaded slot inactive by storing `-1` to its active
   ID at `0x80011C48` (or the second success path), without clearing all 64 bytes
   of its record.
2. `fn_80011C9C` does **not** universally ignore inactive records. At
   `0x80011CC8–0x80011CF0`, an inactive ID causes it to inspect record byte `+2`;
   state 2 invokes cleanup. Therefore `activeID == -1` alone is insufficient
   justification for treating arbitrary record changes as harmless.
3. `fn_8001F124` reads byte `+2` at `0x8001F138`. When it is zero, branch
   `0x8001F140` jumps to the epilogue, with no resource-pointer/callback access.
   For nonzero state, cleanup uses `+8`, `+C`, `+10`, `+14`, then clears these
   and the other lifecycle fields. It deliberately leaves `+4` and `+18`
   untouched (`0x8001F1A0–0x8001F1E0`).
4. The allocation loop in `fn_80011C9C` requires active ID `-1`
   (`0x80011D3C–0x80011D44`). For a room archive it supplies the callback
   `0x80011B28` and the per-slot backing table entry to `fn_8001F2B4` at
   `0x80011E64`, then writes the new active ID at `0x80011E90`.
5. `fn_8001F2B4` stores new loading state 1 at record byte `+2`
   (`0x8001F2D8`), overwrites backing `+18` from argument `r6`
   (`0x8001F2DC`), and overwrites callback `+4` from argument `r5`
   (`0x8001F378`). Neither old value is consumed by this setup function.

## Narrow implication

A record-only mismatch can be ignored for this known stale-metadata pattern
when both IDs are `-1`, both records are idle (`+2 == 0`), owned auxiliary
pointers `+8..+14` are zero, and all bytes except callback `+4..+7` and backing
`+18..+1B` match. Keep record table/bulk ownership validation, map/scene guards,
the empty wanted-queue condition, and all active-slot rules. This evidence does
not justify ignoring arbitrary inactive records, outstanding cleanup, map
changes, or session/heap identity mismatches.
