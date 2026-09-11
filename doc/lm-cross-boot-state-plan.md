# Cross-boot SD states: bounded implementation plan

Status: read-only audit after the reported 0.3.39 Wii tests passed, including
same-session SD export/import. **Cross-boot restoration is not implemented or
proved by that result. No session, identity or ownership checks were changed.**

## Recommendation

The smallest credible next target is **same build, same console, same boot
configuration, ordinary Japanese Hidden Mansion map 2**, following normal
retail initialization. Keep raw addresses fixed and refuse a mismatched
bootstrap. This is substantially smaller than general pointer relocation or
an emulator-style whole-machine state, but needs a retained-owner proof in
addition to a different archive admission policy.

Do not copy all SYS, preserve the old process key as the entire solution, or
remove session checks and treat a surviving first frame as success. The
snapshot contains absolute pointers; the new process must provide compatible
objects at their endpoints, not merely identical address numbers.

## What currently rejects a rebooted archive

| Layer | Current behavior | Persistent-profile requirement |
|---|---|---|
| PPC catalog, `catalogEntryCompatible` | Build CRC, snapshot version and session must match. | Separate “same-session” and explicitly supported persistent profiles; catalog hints never authorize loading. |
| ARM `LmStateStorage.c`, import phase 2 | Refuses a file whose session differs before transferring its body. | Add a versioned persistent file kind, not a blanket exception for old archives. Preserve bounded transport and complete file-length checks. |
| PPC `importedArchiveError` | Session/build/format checks, payload CRC, per-process SipHash, complete raw-slot/companion/census validation. | New integrity/provenance policy plus mandatory bootstrap/owner proof. CRC only detects damage; authentication does not establish object lifetime. |
| `initializeSlot` | Clears the MEM2 commit word on every injected-process startup. | Keep this. A rebooted memory buffer is not an imported, validated persistent state. |
| ARM/PPC mailbox | Fresh process session cancels old transfers; immutable receipts bind operation, ID and sequence. | Keep **transport session** fresh even when an archive is portable. File provenance and transfer ownership are separate identities. |
| `loadState` | Rechecks live scene, heap, archive, camera, resource, model and renderer compatibility before copying. | Retain these checks and add persistent-profile admission before any live restore write. |

`initializeStorage` derives its private keys from the time base and uncaptured
low-memory bytes. A deliberate reboot changes them even if allocator addresses
repeat. This explains the immediate refusal, but does not mean changing key
storage alone would make the underlying restore safe.

## Present capture and retained state

The current core captures the GAME payload, GAME heap metadata `+3C..83`,
audited static ranges, RNG and three camera records. It also carries the
compressed `0x419B00` shared resource payload and saved proof censuses. It does
**not** copy SYS/ROOT allocators, the shared archive's separate mounted owner,
the GAME heap mutex at `+18..2F`, OS state, DVD threads/queues, GX FIFO/XFBs,
ARAM allocator/content or the audio arena.

The GAME heap metadata includes child/disposer list anchors; equality of heap
bases alone is insufficient for those links. `headerMatchesLive` presently
requires exact ROOT/SYS geometry, modes/groups and free/used endpoints,
current heap/group, map/archive/current-scene/audio identities and loop state.
The shared descriptor additionally binds the entire bounded SYS used/free
list signatures and the retained parent's owner, base, tags and metadata.
Camera sidecars require matching live targets. The same-map guard only admits
audited GAME root/resource replacements; it is not a cross-boot owner proof.

Both save and load already call native audio scene reset before capture/copy;
the replacement bootstrap handle remains live and excluded. This is useful
normalization, not a reason to restore the audio arena. `ioIdle` proves DVD,
ARAM and card quiescence. Existing DVD normalization clears completed-request
payload pointers only after workers are asleep; it does not rewind their
queues or stacks. Retained archive-list links are repaired from the validated
census when the guarded room path applies.

## What the available captures establish

Read-only decoding used the existing Dolphin 5.0 decoder, then validated the
unique MEM1/VMEM layout markers. The actual compiled `LmSharedArchiveValidate`
helper passed on all four native captures below.

| Captures | ROOT / SYS / GAME | Shared payload / owner | SYS used/free signatures |
|---|---|---|---|
| `build-lm-emu/dolphin-user/StateSaves/GLMJ01.s01`, `.s02`, `.s03` | `80538420 / 805384C0 / 80BE44D0` | `807BAFE0 / 80BD4AF0` | `49168428 / 5C4D7D16` |
| `build-lm-emu/dolphin-portable-0.3.32/User/StateSaves/GLMJ01.s01` | Same | Same | Same |

All have map/scene 2, current scene `80212E48`, input owner `80538560`, and
GAME extent `80BE4560..817FB140`. The latter isolated-profile capture contains
an intermediate 0.3.36 payload, not 0.3.39. The old `.s02/.s03` pair is explicitly
same-process; these files are **not a controlled same-build before/after-reboot
pair**. Their agreement supports investigating fixed-address bootstrap, not
enabling it immediately.

The old `.s02` and isolated capture have exactly the same 37 SYS used-allocation
descriptors. Nevertheless input state, an ARAM thread stack, FIFO, both XFBs,
direct-print state and 88,585 audio-arena bytes differ. Matching allocation
headers cannot establish equivalent live contents. All observed cameras are
inside GAME, but their addresses differ between captures; that variation is
not a license to bypass the existing camera gate.

A raw-word census found values in GAME falling inside input, audio, render and
thread allocations. These are **pointer candidates, not typed references**:
GAME includes free bytes and integers can resemble pointers. No reciprocal
GAME-slot/audio-handle pair was found with the tested `handle +30 == slot`
condition in the old fixture. Do not relocate or clear these candidates by
number range, and do not infer an audio bug from their count.

### Fresh Wii archive

The locally preserved
`../sd-captures/lm-0.3.39-pass-20260907/lm_states/archive_00000001.lms`
is 14,876,000 bytes: archive version 1, snapshot format 24, build `CCECC76B`,
session `272781FB`, generation 5. Its full length, envelope payload CRC
`3A3D8468`, core CRC `A9BBA93A` and companion CRC `A19BA9DE` validate; the
companion decodes to exactly 4,299,520 bytes. This offline check does not verify
the session SipHash tag, whose private key is not in the file.

Its ROOT/SYS/GAME geometry, modes/groups, SYS free/used endpoints and all ten
shared-descriptor words match the older Dolphin captures, including the two
list signatures above. Its current scene is `80212E48`, audio object
`803E3CF8`, audio scene 0 and map/scene 2. This adds a real Wii observation,
but different builds/platforms are still not the requested controlled reboot
pair. The archive's captured GAME allocation addresses are not a requirement
that all room-local objects initialize in the same order; existing same-map
admission validates the allowed differences.

Native provenance remains independently checked: all seven tests in
`scripts/test_lm_shared_archive_native.py` pass against clean JP DOL SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`. JP boot callback `8000E338` allocates
the shared resource and separate owner; setter `80066328` publishes it. Mission
cleanup's `80066324` is inert. Native resource cache writes are archive-relative.
Local decomp `Unsorted/80005EB8.cpp`, `bootScene.cpp` and `LMDisplayUtil.cpp`
explain the boot sequence and independent SYS/audio/GX/ARAM allocations, but
their region-specific labels must not be copied as Japanese addresses.

## Additional proof/data needed

Add a new, bounded persistent-profile descriptor. Start with evidence records,
not another raw SYS image:

1. **Bootstrap identity:** clean retail revision, exact payload and launcher
   identity, memory-layout/profile version, relevant video/patch configuration,
   and native initialization recipe. Initially require the same completed
   Hidden Mansion baseline and map 2; relaxing these is later work. No foreign
   disc/mod build, boss arena or arbitrary title-screen restore in this profile.
2. **Retained allocation manifest:** each ROOT/SYS retained block's exact
   address, size, group, typed owner and disposition, with reciprocal heap,
   disposer and child-tree links. The existing shared-parent validator supplies
   only part of this. Thirty-seven 32-byte records would cost 1,184 bytes before
   anchors/digests: an illustrative metadata budget, not a final format size.
3. **External-reference contract:** enumerate actual typed outgoing references
   from restored objects/statics to retained input/font/math/render/audio/ARAM
   owners. Each endpoint must be immutable, native-recreated at the same
   address with proved semantics, or rebound/normalized through a typed rule.
   Include excluded fixed tables and incoming retained callbacks/back-references
   into GAME. Unknown ownership fails closed. Do not apply address-like-word
   scanning as a relocation mechanism.
4. **Resource/ARAM identity:** preserve the current shared payload capture and
   exact live parent ownership. Record/prove the normal boot's five UI ARAM
   resources and allocator descriptors; ordinary Mission restoration must
   refer to the newly initialized banks. Do not restore ARAM OS ownership or
   assume raw MEM1 coverage includes ARAM data.
5. **Canonical live services:** authenticate the complete native audio-drain
   semantics for every saved handle-bearing class, retaining the fresh audio
   bootstrap and matching scene/banks. Prove completed DVD requests neutral,
   no stale callbacks, unlocked live GAME mutex, valid current OS context and
   valid current renderer/FIFO/display owners at the freeze boundary. Timers
   based on OS time need typed rebasing if any captured consumer uses absolute
   boot-time values; do not rewind the hardware time base or scheduler.

If an endpoint is mutable gameplay data missing from the current domains, add
only that authenticated payload with explicit ownership and capacity checks.
If it is an OS/hardware object, reconstruct or keep it live. The complete
additional mutable-byte requirement is **not yet known**; the evidence does
not justify either “zero more bytes” or a blanket SYS snapshot.

## Smallest implementation sequence

1. **Read-only reboot proof first.** Export a fresh state plus the proposed
   retained-owner manifest. After a real cold boot and the same normal retail
   initialization, compare it before any import can become loadable. Record
   exact mismatching fields, not one EPOCH bit. Include a soft reset separately;
   it has a different transport/boot lifecycle. Preserve existing same-session
   files and their semantics.
2. **Close the identified reference gaps.** Trace constructors/consumers for
   each retained class, authenticate JP instructions and add helper tests.
   Audit saved state after existing save-time audio normalization, not only
   arbitrary running frames. Add typed rebind/reset routines only where proven.
   Keep fixed-address mismatch a refusal; generic relocation is out of scope.
3. **Versioned persistent envelope and profile admission.** A new file kind
   carries the immutable manifest, normalization version and complete payload
   binding. Legacy format 24 files cannot be promoted by changing a session
   field. An optional persistent local provenance key is a separate file-trust
   mechanism, not the admission proof. Keep fresh mailbox session/receipts,
   old-state backup, exact sizes, dry decode and no-write rejection guarantees.
4. **Construct native baseline, then load.** First prototype may require the
   user to enter the specified Hidden Mansion file and wait for control.
   Automated baseline loading can follow using authenticated native flow.
   Import stages bytes only; compare the new profile, run all existing load
   gates, quiesce, freeze and revalidate live owners before the first restore
   write. Derive destinations from live validated objects, never file addresses.
   Any preflight mismatch leaves the native scene playable and old slot intact.
5. **Post-restore reset of mod caches.** Use the established timeline/load
   revision notifications to invalidate colour/display/room caches and sample
   fresh input. Restore GAME/shared payloads and repair only validated list
   links; retain current OS/audio/GX transport. No native reinitialization may
   run midway through a partial heap rewind.

## Focused proof before enabling the profile

- Host rejection cases: wrong build/config/profile, moved retained allocation,
  altered type/owner/disposer link, stale ARAM identity, bad external-reference
  record, truncated/corrupt companion, capacity failure and reset during every
  import phase. Verify no live game writes and the previous state survives.
- Controlled same-build pairs: two cold boots into the same native baseline,
  then a soft reset, recording full retained manifests at both ends. An equal
  header or CRC alone is not a passing comparison.
- Wii prototype: same-room persistent load first; then the proven
  Parlor → Storage → restore → Anteroom cleanup route, reverse direction,
  ordinary door after restore, repeated loads, and save/export again after a
  persistent load. Stop at the first fault and retain both attempt journals.

General cross-console, changed video/launcher configuration, boss arenas and
typed relocation remain separate expansion work. This plan recommends the
bounded fixed-address profile, not immediate removal of the current guard.

## Exact next data request: two ordinary 0.3.40 exports

**No new runtime diagnostic or cross-session load permission is needed for
the first comparison.** Use two ordinary exports from the same matching
0.3.40 installation:

1. Cold-boot the Wii, open the usual completed Japanese Hidden Mansion file,
   and leave Luigi at the initial bottom-Foyer position. Do not menu-warp or
   load a mod state first. Once controllable, wait about five seconds, create
   one mod state, export it, and wait for the export-completed message.
2. Preserve that newly created `.lms` file as the **before-reboot** file. Note
   its actual archive ID; do not assume it will be `00000001` or overwrite an
   existing archive. Preserve the attempt journals for this session as well.
3. Fully power off and restart the Wii, using the same launcher/settings,
   progressive-mode choice, controller and memory-card file. Do not overwrite
   the normal game save between these two boots. Repeat the same Foyer setup,
   save and export, again waiting for completion. **Do not try to import or
   load the first boot's state.**
4. Send the two newly created `.lms` files, explicitly labeled before/after,
   plus each session's `lm_dumps/lm_attempt_a.bin` and `lm_attempt_b.bin` when
   available. Include any difference in startup choices or controller setup.

The files already contain enough for the first comparison: core layout,
ROOT/SYS/GAME identity and allocator endpoints, all shared descriptor fields
(including full-list fingerprints), saved mounted-volume descriptors, captured
external anchors, camera records and resource/model censuses. Sessions/auth
tags, RNG, time-dependent gameplay and mutable resource bytes are expected to
differ; whole-file byte equality is not the criterion.

This pair can expose a concrete layout/owner mismatch and prioritize the next
fix without a diagnostic build. It cannot reveal all retained SYS object
contents or prove every external pointer's type: those objects are deliberately
excluded, and a list hash cannot reconstruct their full records. If the pair
matches, finish the native retained-reference audit; request a narrow new
owner manifest only for an identified missing proof. A soft-reset pair follows
later, separately from this first cold-boot test.
