# 0.3.40 verification record

Date: 2026-09-07. GLMJ01 revision-0 experimental candidate.

## Scope and evidence

Subsequent Wii acceptance: the user passed naming/export, rename/discard,
31-character/blank-name handling, and the complete same-session SD/route
regression (checklist rows 1–4). Soft game reset followed by a state load was
refused with EPOCH. A full Wii restart refused the old archive at import with
DIFFERENT SESSION. These are distinct from the accepted same-session routes.

Eleven SD files were copied and hash-verified, without SD writes, at
`../sd-captures/lm-0.3.40-names-pass-20260907`. Archive 1 is still .39 build
`CCECC76B`, session `272781FB`; archive 2 is .40 build `E0700BF2`, session
`17344B45`. Both payload CRCs validate. Archive 2's name generations contain
`Test` then `Test2`. This is not the controlled two-boot .40 export pair.

The .40 journal A (generation 44, 349 valid records) includes successful
restoration and later repeated EPOCH refusals at phase `A8000000`, argument
`80399BE0`. This identifies `cameraObjectsValid(preflight, true)` at the fixed
camera pointer table, before the next audio-quiescence/restore copy. Saved/live
map and scene are both 2. The log does not record which of three camera
targets failed, or distinguish address, range, duplicate or captured-size
failure. The `XA0` value reflects preceding guarded admission, not a camera
index. Do not infer a relocation fix from that value alone. Journal B is
generation 43 with 13 valid records. Existing crash files may be stale.

Subsequently the controlled full-boot pair arrived as archives 3 and 4, named
Ilovemywife and Sheislovely. Both are .40 build E0700BF2 with different process
sessions; see the .41 verification record and camera ownership proof. The
requested pair is complete; no repeat of accepted rows 1–4 is required.

The user reported that all twelve .39 Wii route/restore checks passed. An SD
state then refused after reboot. The fresh .39 archive and journals were
preserved; this is not evidence that the archive was deleted or corrupted.
The immediate reset gate is the per-process session/authentication contract.
Retained native ownership across boot is a separate unresolved requirement.
See the cross-boot plan for the source audit and controlled two-export pair.

.40 adds persistent display names only; snapshot format 24 and the capture /
restore manifest remain unchanged. SD transport protocol is now 4. Name
metadata is separate from immutable `.lms` files, with canonical bounded names,
archive-header binding and alternating checked records. Failure feedback
distinguishes an exported state whose name could not be committed.

## Validation

- Full host suite: **674 passed, zero failures, errors or skips**.
- Native tests execute production keyboard logic, ARM metadata worker, and
  client/worker/codec/authentication transactions. Cases include exact receipt
  matching, malformed labels, failed writes, metadata recovery, rename after
  reset without importing, partial export feedback and resident-state safety.
- Independent review found and fixed the shared-codec scratch offset left at
  512 bytes after the mailbox grew. Both paths now use the common 1024-byte
  reservation. A production-codec canary test verifies all mailbox bytes survive
  shared staging, dry validation and restore. Cache-line ownership checks remain.
- The full Wii launcher and both payloads compile. Wii resident blob:
  **159,365 bytes**, 48.6% of the 320 KiB working limit; Dolphin: **151,453
  bytes**, 46.2%. Wii growth is 4,752 bytes. No MEM1/MEM2 reservation increase.
- Clean GLMJ01 DOL SHA-1 authenticated as
  `722005ea9c1eab54b114f814734d8f327e5614ee`. Independent BPS application
  validates target ISO CRC32 `87CFCEFE`; BPS size 151,717 bytes.
- Wii ZIP has exactly seven owned app files; Dolphin ZIP has exactly five
  entries (BPS, README, TESTING, CHANGELOG, miniz license). Both pass ZIP CRC
  checks and match built inputs and current documents. No retail ISO/save is
  distributed. Versioned/current checklists are byte-identical; whitespace
  check passes.
- No interactive .40 Wii/Dolphin keyboard or gameplay pass is claimed.
  Host fault injection is not a physical SD power-cut test.

## Artifacts and SD handoff

Wii ZIP: `build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.40.zip`
(1,127,326 bytes), SHA256
`a133590e49892571a73801cf93daf12eff43a0c3f5834af34cf3cdcd16135da1`.

Wii payload: 160,100 bytes, SHA256
`724d14ed83eae25b1964486900eab7981178d99739c929fc8e72b9638f40dd91`.
Launcher: 1,564,480 bytes, SHA256
`da2f827ddb6f3a16c1b080ac358ebe9173b241fd1e94ce24afae2addd34a009b`.
Dolphin ZIP: `build-lm-emu/Moonshine-Luigis-Mansion-Dolphin-Experimental-0.3.40.zip`
(82,007 bytes).

Seven previous app files were copied and SHA256-verified at
`../sd-backups/lm-before-0.3.40-20260907/moonshine_luigis_mansion`.
Seven new files were installed to `D:/Apps/moonshine_luigis_mansion`, each
flushed with `Flush(true)` and verified by SHA256 readback. The versioned ZIP
in `D:/lm_builds` was also flushed and hash-verified. Eight log/archive files
were hashed before/after installation and are unchanged. No archive, retail
save, preference or other app was modified. SD writes are complete and the
user was told the card can be safely ejected.

The included six-test checklist covers naming, one accepted route regression,
and controlled BOOT A/B exports. Post-reboot loading remains unsupported, not
silently admitted. Names surviving reboot do not make snapshots portable.
