# 0.3.38 verification record

Date: 2026-09-07. Japanese revision-0 experimental build, snapshot format 23.

## Fresh hardware evidence

The two .37 crash reports and both attempt journals were copied from D: into
`../sd-captures/lm-0.3.37-user-20260907-anteroom` and hash-verified, along with
`ndebug.log`. Reports A/B are generations 6/7, payload CRC `7B68C87A`.
Both fail at `8003C248`, LR `8004339C`, while destroying the first picture
owned by the fixed dialogue wrapper at `803C4130`.

Both show completed state restoration before a later Anteroom menu-warp
request/accept/dispatch. The call stack is old Mission cleanup, before the
destination appearance callback, not Anteroom construction. Journal B
explicitly records Storage arrival, a successful state load, hundreds of
post-load updates, then this next warp. This differs from the .36 immediate
post-return heap corruption. Neither new exception establishes out-of-memory
or a room-distance limit. Ordinary mansion destinations use map 2; separate
boss maps are outside this acceptance claim.

The exact missing owner is proven by clean executable lifecycle and two older
private MEM1 fixtures. All 25 primary picture targets are valid GAME-owned
objects in each original fixture; mixing the later wrappers with the earlier
GAME image makes every target vtable invalid. Those fixtures are not claimed
to be the exact new crash's save/live endpoints. No .37 raw state archive was
present on D: to establish that pairing.

## Implemented correction

Capture the whole `803C3730..803C4448` dialogue-manager state: text, choices,
25 picture wrappers, colour channels and scalar arrays. Its SBSS controller
root/cursors and GAME allocations were already captured. The font/destructor
gap remains excluded. Existing save/restore/writeback loops handle the new
range; native cleanup is not skipped, patched or repaired.

Added static bytes: 3,352 (`D18`). Net aligned core growth: 3,328 (`D00`),
because padding shrinks by 24 bytes. Statics/camera/GAME offsets are now
`18414 / 1855C / 18860`. Format 23 requires fresh states. No MEM2 reservation,
resident slot count, warp mechanism, input, or launcher transport change.
The .37 depth-target and bounded-heap corrections remain.

## Verification

- Full host suite: **608 passed, zero failures/errors/skips**.
- Eight new dialogue tests cover authenticated native instructions, complete
  boundaries, both private allocation graphs and the mixed-epoch defect.
- Console and emulator payloads, plus complete Wii launcher, compile.
- Wii resident blob remains 154,581 bytes (47.2% of its 320 KiB limit);
  emulator remains 147,597 bytes (45.0%). These sizes include BSS.
- Independent Dolphin BPS application verified target ISO CRC32 `2388A1F5`.
  Source DOL SHA-1: `722005ea9c1eab54b114f814734d8f327e5614ee`.
- Both ZIPs passed CRC checks. The Wii ZIP's launcher, payload and documents
  match the build files; metadata says 0.3.38. Dolphin distribution contains
  only BPS, README, TESTING, CHANGELOG and the miniz license, not an ISO/save.
- Whitespace check passed. No interactive .38 Dolphin or Wii pass is claimed.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Wii ZIP | 1,123,232 | `1b5d9ec28dbe81a6fb4ec282d92dde98499c543abe6507a04880bb8fbbaaae3b` |
| Dolphin ZIP | 78,932 | `c3523d170f25d95965229221ab1f813888a8c78ee3ac977134978bdad7fc39d7` |
| Wii mod_lmj.bin | 155,316 | `b2716837b6ea15ed172de11558c01d4f2466a9a18aaed8cdb44ed204b55275ae` |
| Wii boot.dol | 1,563,488 | `f6ecc246f56cf47080b5dd3c00a9b21ca295ec8747491655d3936a5d4f37395f` |
| Dolphin BPS | 147,861 | `46b4d34a5b1029409cb22ff95c71bd63a46d8bbec869d8650c54973194265443` |

## SD handoff

Seven old app files were backed up and hash-verified at
`../sd-backups/lm-before-0.3.38-20260907/moonshine_luigis_mansion`.
The seven .38 app files were installed to `D:/Apps/moonshine_luigis_mansion`,
flushed with `Flush(true)` and individually hash-verified by readback. The
versioned Wii ZIP was copied to `D:/lm_builds`, flushed and hash-verified.
The workspace compatibility ZIP also matches the versioned Wii ZIP.
Logs, saves, settings, themes and other apps were not modified. SD work is done.

The six-test checklist includes a no-state fresh-boot control and the exact
restore-then-warp sequence. Cross-boot SD archives, persistent in-game settings
and dojo/rush controllers remain outstanding; no such feature tests are required.
