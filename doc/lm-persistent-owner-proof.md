# Same-build persistent states: retained-owner proof

This is the implementation proof for a **bounded Japanese retail / identical
launcher configuration / ordinary mansion map 2** profile. It is not permission
to import an arbitrary old snapshot, change heap geometry, relocate words that
look like pointers, or restore SYS/OS/ARAM/audio memory wholesale.

## Evidence and scope

All native instruction claims below use clean JP main.dol SHA-1
`722005ea9c1eab54b114f814734d8f327e5614ee`. Region-mismatched JSystem header
comments are not used as address proof. `test_lm_audio_idle.py` and
`test_lm_persistent_profile.py` authenticate the relevant JP instructions and
exercise the actual C helpers through read-only host harnesses.

The controlled `.40` Wii pair is
`sd-captures/lm-0.3.40-reboot-pair-20260907/lm_states/archive_00000003.lms`
and `archive_00000004.lms` in the workspace parent. Both came from fresh full
boots. Their root/SYS geometry, allocation endpoints/modes, map/scene 2 and audio
scene agree. The three GAME camera addresses do **not** agree. Therefore camera
pointer equality would refuse a valid candidate; restoring an old camera
address into the still-live SYS heap would be the wrong response. The saved
camera objects belong to the validated replacement GAME image.

Those archives do not contain retained SYS contents, so they cannot
retroactively provide this new profile's typed anchors. Two older private
Dolphin MEM1 fixtures do contain them. Their actual profiles contain 39 ROOT+
SYS used-allocation records and compare equal across different rooms, despite
changing input, render cursors and active audio. This validates the selected
field widths and exclusions, **not** a completed cross-boot hardware test.

## What must remain live, and why it can still reference a restored scene

| Owner / endpoint | Native evidence | Persistent-state treatment |
| --- | --- | --- |
| ROOT and SYS used-allocation topology | Retail ExpHeap `+7C/+80`, 16-byte CMEM headers; native boot `80005F10` makes fixed SYS then remainder GAME | Compare a bounded complete used-allocation manifest, not just endpoints. Preserve the existing full SYS free/used fingerprints. Never copy allocator contents from disk into SYS. |
| Native pad `*[804A0BF8]` | `80005854` allocates `98`; `8000586C` calls ctor `801D1E9C`; `80005870` publishes it. Player setup stores its supplied controller at Luigi `+794` (`800ACBB0`). | Saved GAME objects contain this retained pointer. Require identical pad identity, vtable `8038925C`, owning SYS and port. Keep fresh input state. |
| Pad optional callbacks and rumble | Ctor zeros pad `+78/+7C`; rumble subobject begins `+64`. `801D2A3C` loads **word** length at pad `+68`; no pattern is read when zero. | Initial portable profile requires zero callback, callback data and active rumble length. An inactive stale pattern at `+6C` is not followed or compared. No controller pointer relocation. |
| GX FIFO | `80005EB8` allocates `80000`; globals `804A0BA0`/`0BA4` retain buffer/object. `GXInitFifoBase` `801ED5A8` sets object `+0=base,+4=base+size-4,+8=size`. | Anchor buffer/object/geometry, not CPU/GPU cursors or contents. Existing completed-render barrier remains mandatory. |
| XFB pair / mode | `800073AC/73EC` publish `96000` allocations at `804A0BBC/0BC0`; `80007428/7430` publish pair `0BCC/0BD0`; `8000743C` publishes mode `0BD4`. | Require the same two buffers and render-mode object/contents. Exclude `0BC4/0BC8` current/previous frame and VI timing/queue state. |
| Audio camera endpoints | Native setter `8018B3B8`, wrapper `80186830`, sole retail caller `800093B4`. Caller uses fixed renderer `80398780` and `+18C=8039890C`, wrapper supplies middle pointer zero. | Audio basic `803E3CF8 +0C/+10/+14` must remain `{80398780,0,8039890C}`. These targets are already within captured fixed renderer `80398770..803989E0`; they are not the moving GAME camera objects. Anchor them, do not copy audio state. |
| Audio gameplay handles | Detailed drain/release proof below | Native-normalize both source and destination; verify the postcondition. Retain fresh sound pools, bootstrap handle, bank caches and workers. |
| Five boot ARAM resource descriptors | Native boot fills address/size at `803C8428,843C,8450,8464,8478`; load/reset proof below | Compare ten address/size words and require inactive transfer scratch `+8=0`. Do not restore ARAM or completed command pointers. Existing empty ARAM/DVD queues remain mandatory. |
| Mounted shared archive owner | Existing `lm-sys-resource-audit.md`, guarded volume/link reconstruction | Restore only validated shared resource payload. Keep the actual live SYS JKRMemArchive owner and reconstruct the already-authenticated mount/list endpoints. |
| OS/worker/disposer state | SYS manifest covers allocated object/thread/stack/message backing identity; existing `ioIdle` and scheduler/interrupt/render barrier cover operation boundary | Keep live. Never infer object safety solely from a stable pointer, nor restore an OSContext, thread stack or queue from an archive. |

The fixed dialogue, mission picture, depth renderer and other previously
audited GAME-backed managers remain part of the existing core snapshot. The
new profile does not replace their ownership checks or their capture ranges.

## Audio: normalization rather than rewind

Native `JAIBasic::changeSoundScene` at `8018D4E4` performs a real owner teardown:

- It walks every SE category's active head (`data+1E8`, stride `C`, head `+4`)
  and stops each sound through `8018C9EC` with zero fade.
- It walks the sequence table (`data+180`, stride `4C`, handle `+44`) and stops
  all but the bootstrap index; then all stream entries (`data+184`, stride
  `14`, handle `+10`).
- It clears basic `+64` and prepares `80000800` again, bound to the fresh
  basic `+64` slot. Passing the **live unchanged audio scene** avoids the bank
  change branch at `8018D630..D65C`.
- SE release `8018F1D0..F1E0`, sequence release `8018CAB4..CABC`, and stream
  release `8018CB88..CB94` explicitly zero the caller's registered handle slot.
  Thus GAME-held sound handles are removed before the source GAME copy is
  committed, and destination handles are detached before replacement.
- `8018D0F4` removes a controller from its active list and clears its `+30`
  caller-slot backlink and `+38` parameter. Sequence and stream active-list
  roots are `data+214` and `data+220`. Freed nodes can retain other old-looking
  words, but are not traversed as active GAME owners.

`LmAudioIdleValidate` is a **read-only postcondition**: bounded SYS pointers,
at most 16 entries per kind, no linked-list traversal, no writes. It requires
empty SE heads, empty stream table/active head, and exactly the bootstrap in
both sequence table and active list, with a reciprocal basic slot and no
active neighbors. It rejects the two actual old non-quiescent MEM1 captures,
which contain an SE or a non-bootstrap sequence.

The profile additionally compares data/table pointers and the three configured
counts (`804A042C/0444/045C`). It deliberately does not compare a bootstrap
handle's numerical address; the native allocator may recycle another handle.
This helper belongs **after** the native drain and existing settling barrier.
It is neither a substitute for that drain nor a claim to serialize the DSP.

## ARAM: a nonzero command pointer is not always a pending command

`80066134` begins a generic title/pause/list transfer by allocating destination
scratch and overwriting descriptor `+8` and `+10`. Completion `800661A0` waits
on `+10`, decompresses, frees and clears `+8`, then destroys the command at
`80066228`. It **does not clear `+10`**. The synchronous guidemap variant
`8006624C` likewise clears scratch and destroys its command without clearing
that slot. The dedicated gameboy path `8011FEE0` does clear both.

The old title descriptor contains `+10=80BE4A00` after completion. Treating that
as a live GAME object, comparing it across boots, or dereferencing it would be
incorrect. Every audited consumer overwrites the stale slot before its next
wait/destruction. Admission instead checks scratch inactive and native queues
idle, while anchoring only the boot-copied ARAM address/size. These addresses
are physical ARAM offsets, not MEM1 pointers.

## Required admission sequence

1. Fully validate the versioned archive, integrity/provenance, supported disc,
   build/config, stored core, companion and replacement GAME geometry.
2. Rebuild current live identity and demand supported ordinary map/scene,
   idle resource I/O and the existing object/heap/render ownership proofs.
3. Run native audio drain; settle; verify its bounded postcondition.
4. Capture the current retained profile and compare the saved one, including
   complete bounded allocation records and typed service anchors.
5. Enter the existing restore barrier and repeat identity, I/O, retained
   profile and ownership gates before the first restore write.
6. Restore the admitted GAME and fixed owners/shared resource payload, perform
   the existing typed volume repairs, then resume fresh hardware services.

A full reboot archive has fresh transport ownership; its provenance identity
is separate. An in-process game reset can leave an ordinary MEM2 slot alive
while changing its epoch. That slot must not bypass the same retained-profile
admission just because it was never read back from disk.

## Remaining limits and proof boundaries

This is a defensible restricted prototype, not evidence for arbitrary save
files, other builds/consoles/configurations, boss maps or a general pointer
relocator. Invalid profile or unsupported topology must refuse before writes.
The old fixture match does not validate a cold boot after every progression
path, alternative controller setup or renderer setting. Existing native I/O
and audio settling must still prove that workers no longer reference replaced
GAME memory; this work does not raw-capture their task queues. Future findings
of a retained owner require a typed reconstruction/anchor addition, not
expanding a numerical pointer whitelist.

The next meaningful hardware evidence is one freshly exported profile-bearing
state, a full reboot into the supported mansion, an import/load, then movement
and an ordinary door. A second load and a subsequent menu warp exercise the
cleanup lifetime that previously exposed missed owners. Neither successful
archive import alone nor a single restored frame proves those lifetimes.
