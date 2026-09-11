# 0.3.43 verification record

2026-09-08. Japanese GLMJ01 revision-0 experimental build.

## Hardware evidence and fix

The user passed .42 checklist 1–3: named same-session, soft-reset and complete
Wii power-cycle archive loads. Timer visuals/toggles had no reported lag and
preferences persisted. Test 4 crashed while loading Parlor after a Storage
menu warp, not on the later door. Tests 5–6 were not performed.

Fresh report generation 9 (build 9D445DA8) and journals were preserved with
verified hashes under `../sd-captures/lm-0.3.42-user-test4-20260908/`.
The load passed the copied-state grain/heap checks and completed before the
next native RoomInfo lookup faulted at 8002D470 (LR 80013C44, DAR A15ABD59).
The uncaptured fixed object at 803C2138 retained live ToolData pointer
80E6C280. In the saved GAME image that address belongs to an archive link;
its +4 value, 80E6C2F0, is exactly the erroneous JMP base used by the crash.
The correct saved ToolData is 80E6C060 with valid RoomInfo backing 80DA20C0.

.43 captures the exact 0x234-byte scene-owned RoomInfo object through
803C236C, before the adjacent descriptor table. This adds 564 static bytes,
576 aligned snapshot bytes, and changes the format to 26. It does not enlarge
memory reservations or weaken ownership/renderer/audio guards. Native
initializer/consumer instructions and the captured image back the diagnosis;
see `lm-room-info-state.md`. The hardware route still requires retesting.

## Other changes

- Creation movement retains two-pixel taps and accelerates held directions.
  The executable helper crosses 640 pixels in 83 updates (2.77s at 30Hz).
  Release, reversal, disconnect and confirmation reset acceleration.
- Browser Z opens a frozen-name/ID confirmation; A permanently deletes, B
  cancels. The worker validates completed-catalog identity, process/version,
  exact current file size and header-prefix CRC before unlinking that archive.
  Only its name sidecars are cleaned up. Partial cleanup reports
  `DELETED; NAME CLEANUP FAILED`; orphan metadata reserves the ID. No resident
  snapshot is changed. Old-build/corrupt/empty archive deletion is supported.
- Protocol 6 appends two cache-separated lines, preserving all old offsets;
  the mailbox is 928 bytes within its existing 1,024-byte reservation.
  Preferences keep their previous format. Install the complete matching app.

## Verification

- **924 host tests passed; zero failures, errors or skips.** Includes actual
  kernel/PPC deletion, fault/receipt/capability handling, orphan-name protection,
  modal input ownership and the new RoomInfo/format regression evidence.
- Both PPC payloads and the complete ARM/Wii launcher build passed. Resident
  Wii blob: 228,937 bytes (69.9% of 320KiB working limit); Dolphin: 208,133
  bytes (63.5%). Final manifest packing includes alignment.
- Clean JP DOL SHA-1: 722005ea9c1eab54b114f814734d8f327e5614ee. Generated
  Dolphin BPS independently reproduces target ISO CRC32 **76876BCB**.
- Both ZIP CRCs, exact seven/five entry sets, packaged source identity,
  versions and byte-identical current/versioned eight-test checklists pass.
  No retail ISO, save or SD key is included.
- No interactive .43 Wii or Dolphin runtime test was performed. Existing
  timer 36-minute rollover and incomplete GaddWarp endpoint limits remain.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Wii ZIP | 1,161,487 | 9E5AEB5BF55D82B1D0C3AE6585FCEE83C5FB370BE8645A4B814138934545982D |
| Dolphin ZIP | 102,157 | 3C8F7BFDB095C7E4C4297348052B02423D0E1CCD50675309BC4974C11701E0C5 |
| Wii mod_lmj.bin | 229,696 | 31C14524BDFC736B1F4B705F8A849DC1E0450A35EB0552ADF5FD124118072133 |
| boot.dol | 1,570,080 | D1312308ABCF76C107897415814F29DD4785C618FD93F66C0D907F585AE4F099 |
| Dolphin BPS | 208,412 | 9EF62283CAEFE4F83D8E41996A34CC849D5DFB61DE34DD4AAC4628DD2D9B2FE9 |

Wii whole-mod CRC32: **B1F105D5**. Fresh .43 savestates are required.

## SD handoff

The .42 app was copied and hash-verified at
`../sd-backups/lm-before-0.3.43-20260908/moonshine_luigis_mansion/`.
The release was staged at `../lm-release-staging/0.3.43/`, installed to
`D:/Apps/moonshine_luigis_mansion/`, explicitly flushed and reread-hashed.
The matching ZIP is in `D:/lm_builds/`. All 69 tracked root/state/journal/
settings files are byte-identical before and after installation. No existing
SD archive was deleted; deletion tests operate only on synthetic host files.
