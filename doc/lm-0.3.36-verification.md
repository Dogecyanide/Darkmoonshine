# 0.3.36 verification and deployment

## Scope and remaining acceptance

See [release changes](lm-practice-0.3.36.md) and the
[54-test runner checklist](lm-testing-current.md). This is an experimental
candidate, not a mansion-wide or 1.0 compatibility claim.

Snapshot format 21 uses one resident state and a compressed shared-resource
companion. The MEM2 reservation is unchanged. Saved/live root and resource
ownership proofs, omitted HUD/effect owners, particle checks and the protocol-3
SD browser/transaction rollback are included. SD archives remain same-session
only; cross-boot portability and dojo/rush modes are not implemented.

The Dolphin D-pad shortcut problem remains under investigation. The new sticky
States-page Dpad/Link/Edge/Use diagnostic records a tap after release; no held
button is needed. This is instrumentation, not a claimed shortcut fix.

## Local verification

- Full `test_*.py` discovery: **552 tests passed**, zero failures/errors.
  The narrower `test_lm_*.py` discovery passed all 390 tests as well.
- Console launcher/ARM service and emulator payload builds passed. Console
  section total is 151,093 bytes (151,096 bytes with final image alignment);
  emulator section total is 144,077 bytes. Console footprint is about 28.8%
  of the 512 KiB window, or 46.1% of the 320 KiB working cap.
- Clean Japanese DOL authentication:
  `722005ea9c1eab54b114f814734d8f327e5614ee`.
- Final versioned Dolphin image/BPS generation passed, target ISO CRC32
  `64F9F8BB`. The source ISO was not modified. The earlier generic image
  was locked by the old running session; the final versioned image was built
  separately and is the one configured for the restarted test session.
- The existing isolated Dolphin 5.0 was closed and restarted normally,
  without movie playback. Its log confirms boot of the final versioned image;
  the Nintendo boot screen and mod overlay were observed. The profile retains
  GCI-folder Slot A and the existing Japanese completed-save file. Earlier
  movie playback used a different RAW card. No card was formatted or deleted.
- No final integrated save/load, post-warp, cross-floor or Wii hardware pass
  is claimed. Earlier intermediate .36 gameplay cannot certify this final
  format-21 image. Never resume an older native Dolphin checkpoint to test it.
- Source whitespace check passed; Git's existing LF/CRLF warnings were not
  test failures. No commit, push, usage reset or unrelated cleanup was done.

## Packages

Both ZIPs contain the exact current 54-test checklist and release changes.
Checklist numbering 1 through 54 and exact packaged text were checked.
Neither ZIP contains a game image or save. The Wii ZIP has seven app files;
the Dolphin ZIP has BPS, README, TESTING, CHANGELOG and the miniz license.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Wii ZIP | 1,129,672 | `4DE7079E9736BA9ECF9D85CBE10D6F4362781B724A1FEB6268B0BF543DE4941A` |
| Dolphin ZIP | 85,031 | `B4C8C282AB751591A1DD6DCDFB67F6DD8244F986FDC1C6CADBD367C80A9ED0B0` |
| Dolphin BPS | 144,341 | `93F1C010F014B9150B3C3B53830FF84B77598F192650C35D092054429F8C1E4E` |
| Packed Wii mod | 151,828 | `E0FE59B8FCAE259E47583C558589848DB6D5D8EE8832D8C88233EF0B2197E654` |

The versioned Wii ZIP and compatibility-name launcher ZIP are identical.
Release notes were explicitly supplied when packaging; the nested launcher
build's empty CHANGELOG setting alone does not include them automatically.

Minor checklist terminology: test 23 calls capacity refusal "CACHE FULL";
this includes the current `TOOBIG` and `FULL NEED ... FREE ... STAGE ...`
messages. Capture any capacity refusal, not only that literal phrase.

## SD deployment

Before writing, all six existing owned app files were copied to
`../sd-backups/lm-before-0.3.36-20260907/moonshine_luigis_mansion` and verified
against the SD. Only boot.dol, icon.png, meta.xml, mod_lmj.bin, TESTING.md,
CHANGELOG.md and licenses/miniz.txt were installed under
`D:/Apps/moonshine_luigis_mansion`. Each write was flushed and SHA256-verified
by readback. The versioned Wii ZIP was also flushed and verified under
`D:/lm_builds`.

Saves, settings, themes, crash/attempt logs and other apps were untouched.
SD access finished and the user was told the card could be ejected normally.
