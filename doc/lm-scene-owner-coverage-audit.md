# Mission cleanup and omitted static-reference audit

This follows the 0.3.38 room-prop cleanup crash. Instruction evidence uses the
authenticated clean Japanese DOL, not a patched executable. The two optional
0.3.30 MEM1 captures are read only for allocation/reference comparisons; they
are not the crashing snapshots and are never imported into a running game.

## Cleanup after the crashing family

The crash returns through `8000BE7C`: the actual call to `80011650` is at
`8000BE78`. The remaining native cleanup work is:

| Call | Routine and relevant roots | Current coverage |
| --- | --- | --- |
| `8000BE7C` | `80160E30`, effect controllers at `803CE0F0`, additional records through `803CE7D0`, and SBSS roots | Fixed `803CE0F0..803CEB00` and SBSS already captured |
| `8000BE88` | `8015EAAC`, emitter manager at `803CD4FC`; deletes three arrays and resets its intrusive anchors | Entire `803CD4FC..803CE0F0` already captured |
| `8000BE9C` | Mission-6-only `801851A0` | Native `blr`, no work |
| `8000BEA0` | Mission-6-only `80056E08`; object read from `r13+720` | Root `804A1200` is inside captured SBSS |

Earlier direct cleanup calls also use captured fixed families: event-active
and visibility state (`8002BC34`), dialogue (`80043B80`), HUD (`8003E4C8`),
actor array/model records/render targets (`800605F4`), grain managers
(`8012EBC0`, `8012B280`), scene effects (`80156C50`) and timer (`80043DD4`).
Other direct roots are inside captured SBSS; `80037B68`, `800F2784`, and
`80066324` are native no-ops. Audio calls remain live; this audit does not
recommend restoring audio or OS object graphs. This is a bounded direct-call
and known-family audit, not proof of every conditional/transitive game path.

## Additional plain model-render context

Capture exactly `803C4A10..803C4A50` (64 bytes):

- `+00..03`: render flag bytes and a mesh halfword;
- `+04..33`: twelve-float model/view matrix;
- `+34`: borrowed current model matrix pointer;
- `+38`: scalar state;
- `+3C/+3D`: render flag bytes, followed by padding.

`80058AB4/80058ABC/80058AC0` selects `+04` for native matrix copy.
The three mesh paths publish the borrowed pointer at `80058B80`, `80058CAC`,
and `80058DA4`. Material rendering reads it at `8005B35C`. Initialization
`800576B8` and setters `80057684`, `80057698`, `800576A8` touch the scalar and
flag fields. The next separately addressed object starts at `803C4A50`.
No OS queue, audio owner or destructor registration record was found in this
bounded context.

The old captures have different matrices and `+34` values (`813BDC60` and
`81420944`). Normal mesh rendering refreshes this pointer before consuming it;
therefore this is a coherent-state coverage correction, **not another proven
cleanup crash cause**. The separate room-prop table supplies that crash proof.

## Candidate scan and exclusions

A byte-only scan of the first optional capture checked uncaptured words in
`803985D4..803E3CF8` against the native GAME used-allocation list, including
interior pointers. With the new room-prop range already included, it found
twenty numerical pointer candidates. The four room-prop references from the
earlier 0.3.38 range scan are listed separately for context. Matching an
allocation proves neither ownership nor permission to rewind it.

| Address/group | Classification or remaining uncertainty |
| --- | --- |
| `803989E0/E4` | Known double-buffer object, not a newly proved scene owner. `8000AC20` allocates `400` bytes, `8000AC78` frees it, `8000ACA4` switches halves. Keep the existing exclusion. |
| `803993D4/E8` | Interior GAME references; lifecycle not established by this bounded audit. Do not capture the surrounding gap merely from numerical matches. |
| `803C1C60/64/68/6C` | Four live pictures in the ten-entry room-prop family. Independently proved by the room-prop audit and this crash. |
| `803C2138/236C/2460` | Interior GAME references; precise lifecycle not established here. No broad gap capture recommended. |
| `803C33F8` | `81400000` numerically falls in a GAME allocation. It is inside the deliberate font/destructor exclusion and is not ownership evidence. |
| `803C4934/4958/4964/4970/497C` | Pointers into one large GAME resource allocation; lifecycle not established here. No broad gap capture recommended. |
| `803C4A44` | Plain model-render borrowed matrix pointer, as above. |
| `803C8460` | **Do not capture:** `+10` of the archive loader at `803C8450` is a live DVD request. `80066180` calls `801CBE40`, and `80066184` stores its result; `800661C4` waits for that request. |
| `803CC730/74C/750/768/76C` | Separately audited effect-query cache. See the dedicated gap audit; preserve adjacent destructor registrations. |

The adjacent `803C1C98..803C20C8` family remains excluded in this build. Its
first `400` bytes are rebuilt actor selectors; the last `30` bytes are four
persistent XYZ vectors, not heap owners. The inactive fixture comparison is not
proof of active-mode restore behavior, so no broad adjacent capture is inferred.

`scripts/test_lm_scene_owner_audit.py` authenticates native instructions and
asserts the exact new context, existing tail-cleanup coverage, and exclusions.
