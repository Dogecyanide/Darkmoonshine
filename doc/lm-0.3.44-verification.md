# 0.3.44 verification record

2026-09-08. Japanese GLMJ01 revision-0 experimental build.

## User evidence

The user passed .43's cold-boot baseline and previous Storage-to-Parlor crash
route, plus SD deletion and faster timer/streak positioning. Loading during
rumble left the physical controller running until another rumble event.

A longer route still crashed: import the original Parlor SD state → warp
Storage → load Parlor → warp Boneyard → load Parlor → walk into Anteroom.
Fresh crash A is generation 10, mod CRC B1F105D5; crash B is older .42 evidence.
The report, journals and archive were copied and hash-verified under
`../sd-captures/lm-0.3.43-boneyard-door-20260908/`. No SD keys were copied or
disclosed. The preserved source and installed app backups remain separate.

## Corrections

The load completed before native FurnitureInfo lookup faulted at 8002DD1C,
LR 8007AB44, DAR 5241524F. Its uncaptured holder at 803C236C followed the future
ToolData address 80E6C220. Restored GAME at +4 contains RARC magic 52415243,
which the consumer interpreted as a pointer. The correct saved ToolData is
80E6C000, backed by byte-identical FurnitureInfo JMP at 80D54FE0. This explains
the actual register/DAR values; it is not an out-of-memory diagnosis.

Three exact ranges join the shared capture/restore/writeback manifest:

| Range, end exclusive | Bytes | Proven content |
| --- | ---: | --- |
| 803C236C–803C2468 | 252 | FurnitureInfo owner, field indices, decoded properties |
| 803C2468–803C24E8 | 128 | Per-room scalar lookup bytes |
| 803C1C98–803C20C8 | 1,072 | Room classifications and four position vectors |

Format 27 has statics 18D6C, camera sidecars 18EB4 and GAME offset 191C0:
1,440 additional aligned snapshot bytes over 26. The native audit covers the
related fixed JMP owners, without claiming an exhaustive whole-game audit.
OS/audio/GX/transport exclusions and admission guards remain. Historical
formats 24–26 remain diagnostic-readable; they are not upgraded/loadable in 27.
See `lm-furniture-info-state.md` and `lm-room-map-lookups.md`.

Successful loads now validate rumble objects, cancel native playback waves,
clear only transient JUT rumble bookkeeping and call native PADControlMotor
with hard-stop command 2 on all four ports. This entry also routes through
Nintendont's controller transport. Input, connection and enable settings remain
live; refusals and SD import alone do not stop rumble. Independent review found
no actionable bounds/ABI/ownership issue. See `lm-rumble.md`.

## Verification

- **967 host tests passed, zero failures/errors/skips.** Includes private
  captured-pointer reproduction, clean-JP instruction authentication, new
  manifest/schema proofs, real rumble-helper harness and package/storage tests.
- Both PPC payloads and the complete Wii launcher built successfully. Wii
  resident blob 229,833 bytes (70.1% of the 320KiB working limit); Dolphin
  209,061 bytes (63.8%). Manifest alignment is included in packed artifacts.
- MEM1 reservation remains 532,480 bytes; MEM2 reservation is unchanged.
  Manifest has 27 hook writes and 33 expected-instruction checks.
- Clean JP DOL SHA-1 722005ea9c1eab54b114f814734d8f327e5614ee verified.
  Independent BPS application reproduces target ISO CRC32 **3A0C0F4F**.
- ZIP integrity, exact seven/five entry sets, every packaged source, version
  labels and byte-identical current/versioned seven-case checklists pass.
  No ISO, memory-card save or private SD key is packaged.
- No interactive .44 Wii/Dolphin gameplay pass is claimed. The new exact route
  and physical rumble behavior still need the seven hardware tests.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii ZIP | 1,161,025 | 5B4F5C2E9A233E5EE2F3F235276B013E1874C94D0EE7A21A9F9ED2E8C10C5196 |
| Dolphin ZIP | 101,914 | D4B23ECA9ECDC3016AFBFFBFCF06F35DD659A34A6936FB70A01BC6EAAD9B31E0 |
| Wii mod_lmj.bin | 230,592 | 4640850CBDFEF0FA29917764EEBAD403758049E2ED28A89F32B7510B47625D57 |
| boot.dol | 1,570,080 | 3210FF823C43BA156D9F71AF56EBD29874C603155B642F77EA10D04D5BD999A5 |
| Dolphin BPS | 209,340 | 193AE7970CF282830B1CC34A75C79EA36AE0118F62F8CEAAF64388256BB1A58C |

Wii whole-mod CRC32: **B080F446**. SD protocol 6 and preferences format remain.
Fresh .44 memory states and archives are required.

## SD handoff

The previous app was copied and all seven owned files hash-verified in
`../sd-backups/lm-before-0.3.44-20260908/moonshine_luigis_mansion/`.
`protected-before.csv` records the 61 root/state/journal/settings files present
after the user's latest testing/deletions. They were rechecked before and after
installation and are byte-identical; no state or key was deleted by this work.

The verified ZIP was staged at `../lm-release-staging/0.3.44/`. Only the seven
owned app files were installed to `D:/Apps/moonshine_luigis_mansion/`, flushed
and reread-hashed. The matching ZIP was copied, flushed and hash-verified at
`D:/lm_builds/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.44.zip`.
No cleanup or unrelated SD changes were performed. The SD is released for
hardware testing. Existing timer rollover, Dojo/rush and boss-map limits remain.
