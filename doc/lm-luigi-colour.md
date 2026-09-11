# Luigi's clothing colour

The regular Luigi descriptor names `/model/luige.szp`, but the observed live
asset is `Game/game.szp:model/luige.arc:model/luige.mdl`, retained in the SYS
heap's shared resource archive. The standalone SZP contains a different retail
variant. Both use LM's custom MDL renderer, not J3D materials. Green is baked into texture 3
(the shirt/overalls atlas) and texture 6 (the cap). The model has no vertex
colours and mostly white material multipliers, so a material tint cannot
produce arbitrary colours while preserving Luigi's skin, gloves, and overalls.

`LMColour` changes only green-dominant RGB565 endpoints in those two CMPR
textures. It scales the selected RGB by the original green intensity to keep
the texture's shading. Non-green endpoints remain unchanged. This gives the
menu arbitrary 24-bit RGB plus nine colour presets and an exact Original
option. The default is Original.

## Asset identity and lifetime

Model descriptor 0 at `0x803435AC` identifies `/model/luige.szp`; its loaded
MDL root is at `+8`, and its state at `+0x30` must be 3. The code also requires
a live player in GAME, the existing stable-state gate, and no active warp.
The live regular model is held in **SYS**, not GAME. The validator requires
the model header, relocated texture table, and both complete image spans to
lie inside the validated SYS heap.

The standalone on-disc MDL is `0x36C85` bytes; the nested shared-archive MDL
is `0x37CA9` bytes. A byte-level comparison with the clean Japanese ISO
confirmed that the nested file already has the captured live geometry counts
and table offsets on disc. The difference is retail asset selection, not
evidence of runtime geometry compaction. Fixed offsets from the standalone
variant are therefore not a valid runtime identity check. Loader `0x80060EBC` proves the table at
header `+0x60` and its entries become absolute pointers when the model loads.
The code follows that table to texture indices 3 and 6 and authenticates
their original image bytes, which match the clean ISO exactly.

| Texture | Standalone SZP offset | Shared-archive / captured live offset | Format / size | Image CRC-32 |
| --- | ---: | ---: | --- | --- |
| Shirt/overalls atlas 3 | `0x143C0` | `0x14320` | CMPR, 128 x 128 | `65714408` |
| Cap 6 | `0x16840` | `0x167A0` | CMPR, 128 x 128 | `4C866065` |

The observed live model is `0x80AA2800`, within SYS's
`0x80538550..0x80BE44C0` bounds; its table is `0x80AC74F4`, with texture
headers at `0x80AB6B20` and `0x80AB8FA0`. These addresses document a private
Dolphin fixture, not addresses baked into the runtime.

The shared archive also contains runtime-mutated vertex morph output and door
matrix caches; its lifetime must not be confused with byte immutability. See
[SYS allocation audit](lm-sys-resource-audit.md) for the owner and restore boundaries.

`lm_colour_model.h` is the exact shared validator executed by both the mod
and native regression tests. It checks heap vtable/size/alignment, stable
model signature fields, bounded texture count and complete table extent,
texture alignment/format/extent, and disjoint image/header/table ranges.
No pointer is followed before its read span is validated. Resolved texture
addresses are passed directly to the colour operation instead of chasing
the table again after validation.

Each image begins 32 bytes after its header and occupies 8 KiB. A one-time
CRC-checked 16 KiB copy in mod BSS retains both original compressed images,
including selectors. This remains outside the snapshot and is never modified
by recolouring. No texture data is embedded in the distributed payload.
Recolours always use that original copy, so repeated colour choices do not
accumulate quantization errors. The Original option restores every byte.

The module forgets its live model address whenever the game is not stable or
a warp is active. This catches a model allocation reused at the same address.
It also reapplies after `LMState::loadRevision()` changes, keeping the user's
current colour setting consistent when the gameplay timeline changes.
All writes happen after the completed-frame `GXDrawDone` barrier, followed by
data-cache writeback and texture-cache invalidation.

Alternate cinematic `dluige01/02/03` assets are intentionally not modified by
this implementation. A failed regular-model or original-texture check leaves
the asset untouched and exposes the reason in `statusText()`.

## Compressed-palette correctness

CMPR endpoint ordering chooses between an opaque four-colour palette and a
three-colour palette with transparency. Arbitrary recolouring can reverse
that order. `lm_colour_codec.h` preserves the original palette mode and
remaps all sixteen selectors if the endpoints exchange positions. When an
opaque palette collapses to one colour, it chooses selectors that represent
the exact colour while keeping the block opaque; pure black therefore does
not punch transparent holes in the clothing.

`scripts/test_lm_colour.py` compiles the exact shared C implementation to a
small native test library and checks decoded pixels. Coverage includes both
palette modes, selector remapping, opaque collapse to black, unchanged
non-green blocks, immutable originals, and alpha preservation for 5,000
random blocks and RGB choices. Visual appearance still needs Dolphin/Wii
verification, especially the shirt-to-overall boundary and cap logo.

`scripts/test_lm_colour_model.py` runs eight additional native validator tests,
including 3,000 hostile pointer mutations and a read callback that records
invalid accesses. The optional private MEM1 fixture test verifies the actual
observed live layout resolves successfully and both original image CRCs
match. These tests do not substitute for an in-game visual check.
