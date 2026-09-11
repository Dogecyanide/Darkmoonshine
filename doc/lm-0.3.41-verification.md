# 0.3.41 verification record

Date: 2026-09-07. Japanese GLMJ01 revision-0 experimental candidate.

## Evidence and implemented scope

The user passed .40 naming/export, rename/discard, bounded/blank names and
same-session SD/route tests. The soft-reset journal identifies the fixed camera
gate at 80399BE0 before restore. Its individual failing condition was not logged.

The requested controlled .40 pair was received and preserved at
`../sd-captures/lm-0.3.40-reboot-pair-20260907/lm_states/`:

- `archive_00000003.lms`: Ilovemywife, build E0700BF2, session 005D6980.
- `archive_00000004.lms`: Sheislovely, same build, session A5899354.

Both complete envelopes/core/companion CRCs, bounded inflation, censuses and
GAME used-list traversals validate. Their retained ROOT/SYS geometry and
endpoints agree. All three standalone cameras move but are independently
proved exact EC-byte group-1 GAME allocations. The full GAME image and fixed
manager already capture their complete restoration, including active embedded
camera roots. No arbitrary pointer relocation is introduced.

.41 adds a 1,440-byte retained-owner profile to format 25. Native audio is
drained and its postcondition checked before capture and before load. A bounded
ROOT/SYS allocation manifest and typed FIFO/XFB/render-mode/pad/ARAM/audio
anchors must match before a profile-bearing state is restored. These checks
repeat inside the existing frozen boundary before live writes. Any nonzero
resident profile is checked, including after soft reset; this is not limited
to files imported from SD. OS and hardware-service state remain live.

Archive envelope v2 binds its header and payload using the SD-persistent key;
its identity is separate from the fresh process ID used for request/receipt
ownership and cancellation. Both key copies are checked, immutable and never
silently replaced on corruption. Config mismatches and unsupported cheat/debug
boots refuse. A zero-profile save remains a clearly labelled current-boot
backup, not a reboot archive. The browser distinguishes these formats.

Eight journals retain attempts a–h. Expansion fills free/invalid slots before
replacing valid legacy a/b history; full rotation replaces the oldest valid
generation. Journal v2 can start from the first LOAD/01 after a boot, capturing
refusals even without a fresh save. Subsequent loads append; successful saves
rotate. Legacy v1 save-anchored logs remain readable. Camera/profile evidence
and the load-start anchor use the bounded critical queue.

See `lm-camera-state.md`, `lm-persistent-owner-proof.md` and
`lm-state-storage.md` for native instruction evidence and the limits of this
restricted ordinary-mansion, same-build/setup profile.

## Verification

- Final complete host suite: **809 tests passed, zero failures/errors/skips**.
- 44 real client/worker transactions test repeated simulated cold boots,
  complete raw-slot reconstruction, all header-byte mutations, payload damage,
  malformed authenticated profiles, wrong keys/config and rollback preservation.
  These simulate storage, not native gameplay rewind.
- Native camera helper tests execute both actual .40 GAME images. Profile and
  audio helpers authenticate JP instructions and execute bounded readers. Two
  older complete MEM1 images produce matching 39-allocation profiles across
  room changes. They are not the new cold-boot Wii SYS contents.
- Journal tests execute extracted production recovery/rotation/write functions
  with synthetic FatFS, including boot → first load → refusal, wraparound,
  24 consecutive rotations and interrupted writes. This is not a physical SD
  power-cut test.
- Archive comparison tooling accepts old format 24 and new format 25/v1/v2,
  validates profile structure/checksums and names typed anchor differences.
  It does not read key files or claim SipHash authentication/live compatibility.
- Both PPC payloads and full ARM/Wii launcher compile. Wii resident blob:
  **170,861 bytes**, 52.1% of the existing 320 KiB working limit; Dolphin:
  **156,189 bytes**, 47.7%. No MEM1/MEM2 reservation increase. Protocol 5 occupies
  864 bytes within the existing 1,024-byte mailbox reservation.
- Clean retail JP DOL SHA-1 authenticated as
  `722005ea9c1eab54b114f814734d8f327e5614ee`. BPS application verifies target ISO
  CRC32 **B75A6944**; patch size **156,453 bytes**. No ISO/save is in either ZIP.
- Wii ZIP has seven owned app files; Dolphin ZIP has five entries. ZIP CRCs,
  source-file identity, versions and current/versioned checklist equality pass.
- **No interactive .41 Wii or Dolphin gameplay pass has been performed.**
  Actual cold-boot import/load, movement, door cleanup and repeated reuse are
  the runner acceptance tests, not already verified outcomes.

## Artifacts and SD handoff

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Wii ZIP | 1,133,671 | 1A6777588806264E37D88D3CF45B80BA4C260C442760CFB0F829D8199E585471 |
| Dolphin ZIP | 83,539 | 5D3A67E7A9875B4F7890551C21BCC0A2E03F8259B8FDF9D4855A9D8701D4D138 |
| Wii mod_lmj.bin | 171,596 | 2A4FEB1C7BD01BCE75FBAE9009A36EDC3106D4ECEAA2CAD0F695AA905584288B |
| boot.dol | 1,565,792 | AC07A2DB6B9B3F286DF8073EDADA5FD07101D25D3AB6DC2E562670D1F90802A0 |
| Dolphin BPS | 156,453 | 07B9264A5252478E551ACA1662026286F56D3E5979C1BC072E091B0B91C0EE93 |

Wii whole-mod CRC32: **17466DA9**.

The verified .40 app backup is
`../sd-backups/lm-before-0.3.41-20260907/moonshine_luigis_mansion/`.
The final ZIP was extracted to `../lm-release-staging/0.3.41/`. All seven
matching files were installed to `D:/Apps/moonshine_luigis_mansion/`, explicitly
flushed and reread-hashed. The matching versioned Wii ZIP was installed to
`D:/lm_builds/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.41.zip` and
verified. All 17 pre-existing archive/name/settings/journal/crash/debug files
were checked byte-identical afterward. No archive or key was created on the PC;
the running launcher/mod performs first-key initialization. SD work is finished.

The six-test checklist prioritizes first export/reuse, soft reset, full reboot,
post-load Parlor/Anteroom cleanup, cross-floor reboot reuse and one runner-chosen
ordinary mansion stress spot. It does not request unrelated cosmetic or broad
feature regressions. Old-build archives are preserved but cannot be imported
as .41 states; testers must make fresh files and confirm REBOOT READY.
