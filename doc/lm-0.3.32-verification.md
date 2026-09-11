# 0.3.32 verification and handoff

Built and installed on 2026-09-06. No runtime pass is claimed.

## Evidence and checks

- User clarified that Foyer to Storage and Parlor to Anteroom were **Room
  Warps menu** actions. Two same-room state loads worked. Parlor to Anteroom
  produced garbled video and a crash.
- Preserved SD evidence under workspace `sd-captures/20260906-215352`.
  Current crash A is the 0.3.31 payload (CRC `A9BB4CB0`), fault `801717E0`,
  DAR `0000000C`. Crash B is an older payload and is not mixed into this diagnosis.
- The attempt journal also records X05 archive-ownership refusal, separately
  from the reported menu crash. No archive guard was widened in 0.3.32.
- All 302 automated tests pass, including native warp publication/rollback,
  independently simulated PPC/ARM caches, bounded crash capture, journal
  compatibility, existing storage/codec tests and authenticated retail anchors.
- Wii PPC/ARM/loader cross-build and Dolphin payload build succeed. The first
  Wii compile caught a callback typedef mismatch; typed wrappers corrected it
  before the successful builds. Independent queued-warp review found no
  normal-path blocker.
- Dolphin UI tooling did not expose a targetable test window. No gameplay
  reproduction or successful candidate warp was observed. No old native
  checkpoint was loaded over new code and no personal Dolphin profile changed.
- Source whitespace checks pass (Git emits only existing LF/CRLF notices).

## Artifacts

Console ZIP: `build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.32.zip`

- 1,103,580 bytes; SHA256 `1DDC75FC7924B7D586049128CFF0979A140B4FA7B05A53A71D158184AC741816`.
- Payload including data/BSS: 125,805 bytes; reserve unchanged at 532,480
  bytes including the 8 KiB debug-stack gap. Growth from 0.3.31: 2,177 bytes.
- `mod_lmj.bin`: 126,480 bytes; SHA256 `6C6D1454ACF7514468F02EAF0A8EFBE3D82A51A5F46DC8846C79F0B6DFA2771F`.
- `boot.dol`: 1,562,656 bytes; SHA256 `51B0C4707C2C82EEB480B52C09CE985EFCD2797F8A3BA10015861D92C60C8C6C`.

Dolphin ZIP: `build-lm-emu/Moonshine-Luigis-Mansion-Dolphin-Experimental-0.3.32.zip`

- 73,639 bytes; SHA256 `A62A1A928C53DE9D74BE1830B4BC993175674D8BC399A63D53DBADFC835C5513`.
- BPS: 122,880 bytes; SHA256 `2DD450732A8D99990BCD24F03C77C6B588D8A9D3E56B148B5D18A912EA138B06`.
- Separate local 0.3.32 ISO verified through BPS source/patch/target checksums;
  target CRC32 `69EF51DF`. Clean source and earlier versioned images preserved.
- Both ZIPs include miniz's MIT license. No retail ISO or save distributed.

## SD handoff

Verified the installed 0.3.31 app against its preserved backup at workspace
`sd-backups/lm-before-0.3.32-20260906-215352/moonshine_luigis_mansion` before
replacement. Installed only boot, mod, metadata, icon and miniz license into
`D:/Apps/moonshine_luigis_mansion`. Flushed and read-back hash-verified all five
files. Theme, launcher preferences, saves and diagnostic logs were untouched.
The card was released to the user after verification.

Next hardware test: fresh-boot Foyer to Storage and Parlor to Anteroom menu
warps, movement/door traversal afterward, then repeat after one same-room
state load. Mansion-wide state compatibility and reboot-portable SD states
remain unfinished; the native-transition fix is not proof of those features.
