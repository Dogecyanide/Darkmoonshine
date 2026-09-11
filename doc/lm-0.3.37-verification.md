# 0.3.37 verification record

Date: 2026-09-07. Experimental Japanese revision-0 build, snapshot format 22.

## Evidence and scope

The fresh .36 Wii files were copied from D: into
`../sd-captures/lm-0.3.36-user-20260907-first` and hash-verified. Crash B,
generation 5, is the current payload (`692195EB`); crash A is older.
Wii D-pad and same-room saves/loads work according to the user. Parlor ->
menu Anteroom -> load was refused; Parlor -> menu Storage -> load returned
and then crashed. The native heap checker dereferenced a corrupted free-list
link after the immediate post-restore heap checks had passed.

The clean executable and old complete MEM1 fixtures establish an omitted
scene-owned graphics target controller and its allocation lifecycle. The
new crash's actual snapshot was not exported, so its exact old/new target
addresses are not known. This is a source-backed crash-fix candidate, not
proof of hardware success or complete mansion-wide state compatibility.

Changes are the complete depth-target owner capture, saved/live target
allocation validation, bounded first-fault heap diagnostics, and the three
proven uninitialized padding-byte exceptions for inactive resource records.
Early checks run only during the first eight traced frames in active native
scene mode, outside scene exit. Ordinary periodic checks retain their
60-sample cadence. Heap and render faults use distinct critical phases F4/F3;
the existing audio F0/F1 checkpoints retain their original meaning.

The snapshot prefix grows by 288 aligned bytes. No MEM2 reservation or slot
count change. Persistent preferences, cross-boot SD states and dojo/rush
controllers remain outstanding. New mod states are required for format 22.

## Build verification

- Full host suite: **600 passed, zero failures/errors/skips**. This includes
  native render-target, heap, resource-record, snapshot, transport, and
  source-order regressions. The initial runs exposed stale version/layout
  assertions and Windows-default text decoding in the packaging test;
  those checks now use format 22 and explicitly read UTF-8.
- Console configuration and complete launcher/payload build passed.
- Wii resident blob: 154,581 bytes including BSS; 173,099 bytes remain under
  its 320 KiB limit (47.2% used).
- Emulator payload build passed: 147,597 resident bytes (45.0%).
- Versioned Dolphin ISO/BPS generation authenticated the clean GLMJ01 DOL
  SHA-1 `722005ea9c1eab54b114f814734d8f327e5614ee` and every patch location.
- Independent BPS application verified target ISO CRC32 `6EBC2A7F`.
  The BPS is 147,861 bytes; no retail image or save belongs in distribution.
- No interactive .37 Dolphin or Wii gameplay pass is claimed. The user's
  Wii test routes take priority over the separate Dolphin/Phob input issue.

## Installation preparation

Seven owned .36 app files were backed up and hash-verified at
`../sd-backups/lm-before-0.3.37-20260907/moonshine_luigis_mansion`.
The old 54-item checklist is preserved as `doc/lm-testing-0.3.36.md`.
The .37 handoff is six focused state tests, not the entire feature matrix.

## Final artifacts and SD installation

Both ZIPs passed CRC validation. The Wii ZIP's launcher, mod and documentation
match their build sources byte-for-byte; its metadata says 0.3.37. The Dolphin
ZIP contains only BPS, README, TESTING, CHANGELOG and miniz license, with no
ISO, memory-card save or state archive.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Wii ZIP | 1,123,194 | `E75FF90D7F577490926F6FD37E86A694773C2BB31988B63895774AFBDDDAFBCE` |
| Dolphin ZIP | 78,853 | `BEC7CA635B783984EFD8D8C9B3D24CAA8309377B0FFA7352004F7710FFD35D78` |
| Dolphin BPS | 147,861 | `5845A51A2C06A20F817DCA0BBC6765F2BA9F55CB0C8A87E19B58CC1BF36770A3` |
| Wii mod_lmj.bin | 155,316 | `F74586A2700BC7DDBBE5AC93028F4686D559E6BD09B69BB83B7E1D244EE4EE85` |
| Wii boot.dol | 1,563,488 | `DA85CE7090A4FA15BC3E8B3B32F27BFD63B6F5F8F1C5F524DB2BADCD6C44A13F` |

The versioned Wii ZIP also replaces the workspace compatibility ZIP
`build-lm-diag/moonshine_luigis_mansion_launcher.zip`.

Seven owned app files were installed from that ZIP to
`D:/Apps/moonshine_luigis_mansion`. Each was flushed with `Flush(true)` and
SHA256-verified by rereading the installed file. The versioned Wii ZIP was
also copied to `D:/lm_builds`, flushed, and hash-verified. Logs, saves,
settings, themes and other applications were not modified. SD work is done.
