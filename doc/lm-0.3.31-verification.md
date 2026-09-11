# 0.3.31 verification

## Automated/local evidence

- Full Python/native suite: 282 tests pass.
- Console PPC payload, ARM kernel, Wii launcher and ZIP build: pass.
- Console payload code/data/BSS: 123,628 bytes (120.73 KiB), including the
  practice features and crash/diagnostic code. No whole-SYS snapshot was added.
- Whitespace check: pass; Git reports existing LF/CRLF conversion warnings.
- The compiled idle-resource helper accepts the actual seven-slot records
  extracted from the user's 0.3.30 native Dolphin checkpoint. Previously the
  masks were active `0x3F`, records `0x7F`; the extra mismatch consists only of
  the two proven stale words in idle slot 6. This is predicate evidence, not
  evidence that a full restore or following door transition succeeded.
- Idle-record tests reject active/loading slots, nonzero auxiliary pointers,
  every other changed byte, invalid mask bits and unmatched active changes.
- Native hotkey tests cover short pulses, held directions, menu suppression,
  unrelated held buttons after menu closure, conflicting directions and
  disconnect/unavailable behavior. Source contracts verify capture occurs
  before the pad is neutralized and BUSY retains its failure reason.

## Console package

`build-lm-diag/Moonshine-Luigis-Mansion-Full-State-Experimental-0.3.31.zip`

- Size: 1,101,198 bytes.
- SHA256: `769B16D14A4DB6FDBF593F240AD803B8EAE84029FD81AF71CACE48F39CEE4249`.
- Packed mod SHA256: `A32DC560D423A79DAAC709A2B8052AA9A3C88ECAFA41259F0E214C6B720DBFB2`.
- Exactly five entries: app `boot.dol`, `icon.png`, `meta.xml`, `mod_lmj.bin`,
  and `licenses/miniz.txt`. No retail disc/save/RAM contents included.
- The 0.3.30 ZIP was preserved unchanged, SHA256
  `3FD3AFE9B23CDF801211DF0F72DD89900CFA55FA36E7C42C3687F3ED60C77AD9`.

## Dolphin package

`build-lm-emu/Moonshine-Luigis-Mansion-Dolphin-Experimental-0.3.31.zip`

- Size: 72,469 bytes.
- SHA256: `E94ABDC98A93E407E8EE22BF16ECAC8B56C7A41053137FF466DFF716EEAFC7D4`.
- Dolphin payload: 120,356 bytes.
- BPS: 120,604 bytes, SHA256
  `7384CCD37AE3D42CBFFAE2424AF068775485F080931841D5099C6BD3DD094389`.
- Exactly four entries: README, project LICENSE, miniz MIT license and BPS.
  ZIP CRC test and patch/license byte comparisons pass.
- A separately named local `moonshine_lmj_dolphin_0.3.31.iso` was generated
  from the clean source, target CRC32 `36A29B76`. The currently running image
  was not overwritten or restarted. Native checkpoints s01/s02/s03 and the
  0.3.30 ZIP were preserved.

## Not yet verified

No 0.3.31 Wii test or fresh Dolphin gameplay test has been performed here.
Do not restore an older native Dolphin checkpoint over a newly built mod:
that replaces the injected code too. Start the new build and create a fresh
in-mod state for runtime testing.

No SD deployment was performed. Existing SD app/theme and retail saves were
left intact. King Boo/full-map transfers and cross-reboot SD imports remain
unimplemented/unverified. In-game preference persistence remains queued.
