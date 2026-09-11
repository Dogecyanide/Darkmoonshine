# 0.3.39 verification record

Date: 2026-09-07. Japanese revision-0 experimental candidate, snapshot format 24.

## Fresh hardware evidence

Subsequent user acceptance: all six release tests and all six follow-up tests
passed on Wii. The follow-up set covered repeated reuse, cross-floor routes
in both directions, a five/six-room natural route, saving after menu warp,
gameplay rewind and same-session SD export/import. The user reported no issues
in those tests. This is route-specific acceptance, not a universal boss/map
compatibility claim. A later full reboot invalidated an SD archive as expected
under the current per-session contract; this remains an unresolved user need.

Eight files, including one valid .39 archive, both journals, both binary/text
crash generations and `ndebug.log`, were subsequently preserved and SHA256
verified at `../sd-captures/lm-0.3.39-pass-20260907`. The crash generations may
be stale; their presence does not contradict the reported clean pass.

Seven files from D: were copied to
`../sd-captures/lm-0.3.38-user-20260907-anteroom` and SHA256-verified: both
binary/text crash generations, both attempt journals and `ndebug.log`.

Fresh report A is generation8, mod CRC `2E6F16CD` (.38). It fails at
`PC800286A4`, `LR80028680`, `DARAFEFF007`, with `r30=803C1C60`,
`r3=81248830`, and invalid vtable `r12=AFEFEFFF`. The native ten-picture
cleanup reads a stale fixed pointer into the rewound GAME image. The stack
returns through `80011668 -> 8000BE7C -> 8000B728`: old Mission cleanup,
after the dialogue cleanup fixed in .38, before destination construction.
Report B (generation7, CRC7B68C87A) is still the old .37 report and is not a
second .38 failure.

Fresh journal A generation36 records completed same-room and post-Storage
loads, successful bounded ROOT/SYS/GAME and grain checks, then the next
Anteroom menu-warp request/accept/dispatch. The invalid external picture
owner is not detected by allocator-list health alone. This is not evidence
of out-of-memory or of an Anteroom-only/floor-distance limit. Ordinary mansion
menu warps reload Mission map2 and still require coherent teardown owners.

## Implemented correction

Capture `803C1C60..803C1C98`: ten native J2DPicture owners plus a retained
three-float view origin and padding. The corresponding actor-array root,
count and GAME objects were already captured. The existing static-copy,
restore and cache-writeback manifest handles the entire span. Native cleanup
is not skipped or repaired by deleting/zeroing pointers.

The clean DOL and two private older MEM1 fixtures independently authenticate
the full lifecycle and reproduce invalid vtables when the two eras are mixed.
Those fixtures demonstrate the mechanism; they are not the precise new
Wii crash's saved/live endpoints. See [ownership proof](lm-iwamoto-picture-state.md).

The wider audit adds two bounded plain caches: model-render context
`803C4A10..803C4A50` and player/effect-query cache `803CC718..803CC818`.
These hold borrowed GAME pointers and scalars, not live OS machinery. They
are coherent-state improvements, not separately observed crash causes.
The known live display allocation and DVD request remain excluded, along
with global-destructor registrations. The neighboring `803C1C98` animation
selector table is not folded into the picture-owner proof.

Added static bytes: `178` hex / 376. Aligned core growth: `180` hex / 384.
Statics/camera/GAME offsets: `1858C / 186D4 / 189E0`. Format24 requires fresh
states. No reservation, resident-slot, warp, input or launcher transport change.

## Verification

- Full host suite: **632 passed, zero failures/errors/skips**.
- Twenty-four new tests authenticate the room-prop lifecycle and mixed-epoch
  failure, renderer-context/native-cleanup coverage and query-cache boundary.
  The wider layout/owner targeted run passed all168 tests as well.
- Both payloads and the full Wii launcher compile. Wii resident blob is
  154,613 bytes (47.2% of the320KiB working limit); Dolphin is147,629
  bytes (45.1%). Both include BSS and grew by32 bytes, not a new snapshot buffer.
- New authenticated Wii mod CRC: `F8390A81`; file155348 bytes, code154616,
  58 authenticated records, arena reservation532480. The code is padded to
  a four-byte boundary, distinct from the linker-resident byte total above.
- Independent BPS application verified target ISO CRC32 `5166CDB3` against
  clean GLMJ01 DOL SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee`.
- Both ZIPs passed CRC checks. Wii payload, launcher, documents, icon and
  license match their build inputs; metadata says0.3.39. Dolphin distribution
  contains only BPS, README, TESTING, CHANGELOG and the miniz license.
- Current and versioned six-test checklists are byte-identical. Whitespace
  check passed. No interactive .39 Dolphin or Wii gameplay pass is claimed.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Wii ZIP | 1,123,439 | `8e061d6614271a90ad16f11ccffa90a0aa17191ef9ef91d004147d590c77b897` |
| Dolphin ZIP | 77,170 | `3d7e6ffba5a587f67e5676ace89f9f1ff430052d993ec287ecb0bc03abe474b0` |
| Wii mod_lmj.bin | 155,348 | `15b442e7d761677d0fa2383e53f57d6c622933dce93c6444f080a4c9b1f63e4d` |
| Wii boot.dol | 1,563,488 | `2d4d26b10508b4d1bc61fd759ecfbaa5b4e5ae96b726f6795ef0077415e0b0d3` |
| Dolphin BPS | 147,893 | `7c5b8a0f838c50415f1609fc5e4e902d38886a06110c0d14f1976d97b01234b1` |

## SD handoff

Seven previous app files were backed up and hash-verified at
`../sd-backups/lm-before-0.3.39-20260907/moonshine_luigis_mansion`.
The seven .39 files were installed to `D:/Apps/moonshine_luigis_mansion`,
flushed with `Flush(true)` and individually SHA256-verified by readback.
The Wii ZIP in `D:/lm_builds` was also flushed and hash-verified, and the
workspace compatibility ZIP matches the versioned Wii ZIP. No log, retail
save, archive, setting, theme or other app was removed or modified.
SD work is complete; the user was told it can be safely ejected.

The six-test checklist keeps the exact restore-then-Anteroom-warp reproduction
and adds no-state, same-room, reverse-direction and normal-door controls.
Format24 requires fresh states. SD archives remain same-session only;
cross-reboot archives, persistent in-game preferences and dojo/rush controllers
are not delivered by this crash-focused release.
