# FurnitureInfo scene lookup capture (.44 / format 27)

## .43 Wii failure and scope

Cold-boot SD loading and the earlier Storage → Parlor load crash passed in .43.
A longer sequence (Parlor import → Storage warp → Parlor load → Boneyard warp →
Parlor load → Anteroom door) then crashed in the native furniture lookup. This
was after a successful restore, not an epoch refusal.

Report A: build CRC `B1F105D5`, DSI at `8002DD1C`, LR `8007AB44`,
DAR `5241524F`, DSISR `04000000`. Registers `r19/r27=803C236C`,
`r3=52415243` (`RARC`), and room argument `r30=27`.

The native door path at `8007AB2C..8007AB40` passes the fixed holder
`803C236C` to `8002DCE4`. That routine loads holder+0 as a ToolData pointer,
ToolData+4 as a JMP pointer, then JMP+0x0C at the faulting instruction. The exact
bad address is `RARC + 0x0C`; free-space measurements do not explain this fault.

## Authenticated native ownership

Tests authenticate the clean JP DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee` before checking instructions.

The scene initializer wrapper `8002E62C` invokes `8002DA8C` with
`this=803C236C`. The initializer calls `800B8658` with the native string
`FurnitureInfo` at `802F5B78`. This is the same Mission `getJmp` helper used by
RoomInfo, returning a borrowed entry in Mission's GAME-owned ToolData array.
It stores:

- +0: ToolData pointer;
- +4: row count copied from the JMP header, or zero if absent;
- +8..+80: 31 cached JMP field indices.

The straight-line decoder `8002DE8C..8002E628` writes decoded per-row values
through +F8, including scalar positions and borrowed resource-string values.
There are no allocation, OS, service or callback calls in this decoder. Native
cleanup clears the holder's count at `8002E8BC`; it does not retire a service.
The exact symbol `803C236C..803C2468` is 0xFC bytes. Capturing only its first
pointer would leave cached indices/current-row fields from a different epoch.

## Matching saved-image proof

The private .43 archive 1 is pinned by SHA-256
`146779974a83acbf4ddb62ac39d387a505e135fd09dacc170ea2a3be0b6274e1`.
Its Mission ToolData array `80E6BFB8` has 25 entries in an exact GAME allocation
with group B and matching array cookie/count. Record 9 is FurnitureInfo:
`80E6C000 → {vtable=8034F1D4, JMP=80D54FE0}`. The 143712-byte JMP is fully
inside the captured map RARC and matches `jmp/furnitureinfo` byte-for-byte.
Its header is `(731 rows, 35 fields, 0x1B4 data offset, 0xC4 row size)`.

The observed scene reload relocates the live array by +0x220. Without fixed
holder capture it leaves `803C236C` pointing to future address `80E6C220`, where
restored GAME contains `{80BB5805, 52415243}`. Thus the native pointer chain
reproduces the crash register and DAR exactly. This is a cross-epoch alias;
it does not require assumptions about controller input or heap exhaustion.

## Fix and format boundary

Capture FurnitureInfo alongside its GAME backing, plus the adjacent native
per-room lookup bytes and dependent map UI scalar state documented in
[Room map lookups](lm-room-map-lookups.md). All three are explicit ranges in the
common save/restore/cache-store manifest. Other live-service exclusions and
epoch/resource/allocator proofs are unchanged.

Snapshot format 27 has static size `0x18D6C`, camera sidecar offset `0x18EB4`,
and aligned GAME offset `0x191C0`. The additional aligned payload is 1440 bytes
relative to format 26. Existing format-24/25/26 diagnostic archives remain
readable with their original decoder layouts; the game requires new .44 states
because old states do not contain the missing owners. No old archive is
relabelled, silently upgraded or certified as loadable.

Tests cover native initialization, exact field/tail extents, faulting pointer
chain, the authentic saved-image mismatch, manifest uniqueness/non-overlap,
live-service exclusions, all manifest capture paths, format-27 readback, and
rejection of recomputed-CRC archives merely relabelled between formats 26/27.
These are source/fixture tests, not a claim of .44 Wii runtime verification.
