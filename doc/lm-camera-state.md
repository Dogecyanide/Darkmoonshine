# Japanese camera ownership and reset admission

## Scope and finding

The fixed table at `80399BE0` owns three separately allocated `0xEC` camera
objects. Their addresses are not boot-lifetime identities. The game constructs
them during scene setup and directly frees them during Mission cleanup.
The complete GAME heap and fixed camera manager are already captured together.
Therefore changed targets can be admitted when **each memory image separately
proves all three exact GAME allocations and its own fixed table references**.
This is not permission to relocate retained SYS cameras, bypass session
authentication, or skip the other scene/audio/archive/renderer gates.

The .40 soft-reset journal refuses at the camera gate, but does not record the
individual failed target or condition. The controlled reboot pair below proves
that all three addresses can legitimately change, not that address inequality
was the only condition in that particular soft-reset refusal.

Native instruction assertions use the clean GLMJ01 revision-0 DOL, SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`, SHA-256
`1c5e4f2ac8d67782b82060ff8496622f1d7029975dc523ae2175b19f22576c7d`.
Addresses below are authenticated JP addresses, not another region's symbols.

## Construction and lifetime

`80010B5C` calls `80020040` at `80010B68`. The latter calls the current-heap
operator new `801C9308` three times, requesting `0xEC` at `80020058`, `80020088`,
and `800200B4`. It stores the results at table offsets `0`, `4`, and `8`
(`80020084`, `800200B0`, `800200DC`). The allocation operator reads current heap
`804A1FF4` at `801C931C` and requests alignment 4 through its virtual allocator.
Scene setup selects allocation group 1 at `8000BEE4..8000BEE8`, immediately
before calling the camera allocator wrapper at `8000BEEC`. The group setter
updates GAME through `804A0B98` at `800060F0`. The captured examples are exact
GAME used-list payloads with group 1.

Initialization calls `8001FA24`, which stores flags, vectors, matrix, projection
parameters and viewport bounds. These are **plain camera objects, not vtable
objects**. The flags occupy the first halfword, not the entire first word.

Mission cleanup calls `80011650` from `8000BE78`. The cleanup chain calls
`80020538` at `80011674`. That function loads all three table values and calls
ordinary delete `801C9508` for each at `80020554`, `8002055C`, and `80020564`.
There is no camera-specific nested object destructor, thread/queue destruction,
or callback-unregistration traversal in this cleanup function.

## Retained references and callback semantics

| Reference | Evidence and restoration coverage |
|---|---|
| `80399BE0..80399BEC` | Three owners; included in captured manager `80399B60..80399C60`. |
| `804A0DB8` | Active-camera alias, written by `800202A8` and callback selection; included in captured SBSS. |
| Camera `+4` | Default executable callback. Scene setup writes `80021464`, `800219FC`, `80022A48` for the observed mansion mode. |
| Camera `+8` | Override executable callback, used only while flags bit 0 is set. Initialization does **not** clear this slot. |
| Camera `+C` | Argument loaded into r3 for the default callback; setter `8001FAD8` clears it. Preserve as camera state, not a generic pointer to relocate. |
| Camera `+E8` | Borrowed descriptor for the relevant view mode; setup assigns `80398C00` or `80398C28` to cameras 0/1. Those descriptors are captured in `80398BF8..80398C50`. Camera 2's unused field is not initialized. |
| `804A0CF8` | Current descriptor alias, published from camera `+E8` by `80021288..80021290` and `80021A0C..80021A10`; captured SBSS. |
| `80398780` renderer | `800203EC` rebuilds renderer scalars/matrices from active camera; included in captured `80398770..803989E0`, ending before the boot display owner. |

`800202B0` reads the active-camera flags at `8002030C`. When bit 0 is set it
dispatches through `+8`; otherwise it dispatches through `+4`, loading `+C`
as the callback argument. Null callbacks are explicitly allowed. An unconditional
"all callback slots must be executable" rule would reject legitimate inactive
bytes. The constructor likewise leaves `+E4` untouched; do not give that word
an ownership meaning by its numerical range.

The active alias need not equal any of the three standalone cameras.
`80025660` iterates a `0x24C` record array rooted at `804A0E30`, takes an embedded
camera at record `+11C` (`8002573C`), then publishes it through `800202A8`
at `800257A8`. The current record is published at `804A0E34`. Both roots are in
captured SBSS and the array is GAME-owned. The observed arrays have allocation
size `0x2960` and an 8-byte array cookie: `8 + 18 * 0x24C`.
Forcing the active alias to equal a standalone table entry would therefore be
incorrect. Replacing the full GAME image and captured roots preserves these
embedded references without separate pointer rewriting.

A raw-word census of two older complete .30 MEM1 captures found references
into the standalone camera spans only in the captured fixed table, captured
active alias, and GAME bytes. None appeared in uncaptured SYS allocations.
This supports the native owner classification but is not an exhaustive typed
reference proof: free bytes and scalar words can resemble pointers. The
established quiescent load boundary must still ensure a camera callback is not
actively executing while the heap rewinds.

## Controlled .40 evidence

The two user-supplied full-boot exports are preserved read-only under
`../sd-captures/lm-0.3.40-reboot-pair-20260907/lm_states/`.
Both archive/core/companion CRCs and bounded GAME allocator traversals pass.
Both have build `E0700BF2`, snapshot format 24, with different process sessions.
CRC validation is **not** SipHash authentication or authorization to load them.

| Archive | Standalone cameras | Active embedded camera |
|---|---|---|
| `archive_00000003.lms` | `80C3A2D8`, `80C3A3D4`, `80C3A4D0` | `80CF9204`, in record `80CF90E8 +11C` |
| `archive_00000004.lms` | `80E6C900`, `80E6C9FC`, `80E6CAF8` | `8123F044`, in record `8123EF28 +11C` |

All six standalone allocations have tag `484D0001`, exact size `EC`, offset 0,
and sidecar captured-size 0 because their bytes are already in GAME. Their
ordinary callback addresses match the three JP functions above. In archive 3,
camera 0's inactive `+8` is `AA9C3962` and camera 2's unused `+E8` is `02545454`;
these values must not invalidate an otherwise authenticated state.

The older archive 1 copied with the names-test files is .39 build `CCECC76B`;
archive 2 is .40. Neither is silently treated as part of this controlled pair.

## Validation and integration

`LmCameraGameValidate` is a heapless read-only helper. Its callback bounds every
read to the supplied image, verifies the image's fixed roots, then traverses
the reciprocal used list with a limit of 8,192 blocks. Each target must be an
exact group-1 `EC` payload, not an interior pointer, duplicate, free block or
overlapping allocation. Allocator alignment padding is included in overlap
checks, including other allocations' backward padding. It returns the first
fault address/value and clears diagnostics on success.

The .41 runtime change keeps unchanged retained targets strict. When any target
changes it requires all saved camera sidecars to have captured-size zero, then
validates all three saved endpoints against the saved GAME/manager and all three
live endpoints against live GAME/manager. Only the existing whole-state restore
does writes. No pointer fixup, SYS-camera relocation, or new snapshot range is
introduced. LOAD phases `F4..F6` record saved/live targets; `F7/F8` identify a
saved/live helper failure. These action-qualified phase codes are distinct from
the existing POST_LOAD heap failure phase.

`scripts/test_lm_camera_state.py` authenticates allocation, cleanup, dispatch,
and embedded-view instructions; executes the compiled helper on synthetic
mutations and both controlled exports; and verifies capture/runtime contracts.
Mixing one archive's manager with the other GAME image is rejected in either
root-disagreement or missing-allocation form.

Required hardware regressions remain: existing same-room/cross-room/floor
routes; soft reset then load an earlier same-process state; camera tracking,
ordinary door entry and menu warp after that load; repeated loads after another
soft reset. Cross-boot SD import remains separately gated until retained SYS,
audio and authentication handling is complete.
