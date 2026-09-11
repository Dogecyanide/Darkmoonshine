# DarkMoonshine RC1 verification

2026-09-08. Exact displayed version: **V1.0.0 Frozen in Time. Release candidate 1**.
Displayed authors: **Dogecyanide, Nintendont Team**.

## Hardware evidence entering RC1

The user reported no observed .44 crashes. Fresh SD reports/journals/archive
and name sidecars were copied and hash-verified under
`../sd-captures/lm-0.3.44-rc1-20260908/`, without copying/disclosing SD keys.
Both crash-report banks are unchanged older .43/.42 reports. Fresh journal B,
generation 53, confirms Secret Altar room 46 warp arrival then rejected STATE
loads. The user explicitly confirmed those attempts were savestate loads, not
outgoing menu warps. Live event06/event74 resources cannot be matched to saved
Parlor ownership, so the X08 guard refuses before restore. RC1 preserves that
guard and documents the boundary in `lm-secret-altar-rc1-limit.md`.

## RC1 changes and review

Shared `lm_branding.h` supplies the exact product/authors/version to the
launcher, HBC metadata and practice home menu. Crash report headings and INI
comments use the new name, while every app/settings/archive/journal/crash path
remains stable. Existing icon artwork and upstream license/source notices are
retained. ZIP stem is `DarkMoonshine-1.0.0-Frozen-in-Time-RC1`.

The persistent memory/version panel, expanded Z census panel and checkerboard
heartbeat are removed. Heapless action events show Saving/Saved, Loading/Loaded,
Busy or Rejected for 60 presenter updates (about two seconds at normal 30Hz).
The 112x22 physical-pixel notice is inset 24px horizontally and 32px vertically.
Repeated identical actions reset the timer; passive Busy gates and raw menu
navigation do not generate misleading leftover notices. Menu Save/Load still
generate their explicit outcomes. Full gate details remain in menus/journals.

Before synchronous save/load the notice can paint only the validated current
presenter's completed XFB; there are no waits, extra game ticks or post-restore
uses of that surface. Independent review confirmed GX completion, XFB bounds,
scope/reset ordering and cache flushing. Review caught and fixed unconditional
DirectPrint binding for all overlays even when no popup is visible, plus menu
navigation suppression. Native text already flushes its framebuffer; the new
explicit popup flush is row-bounded. No guard or heap layout was weakened.

Floor/canary checks and periodic heap checks remain. First floor/canary failure
now also emits a one-shot 0x130/0x131 breadcrumb after crash initialization,
without new traps or repeated frame spam. Existing heap-failure trap and journal
milestones remain. Snapshot format **27**, SD protocol **6**, preference format,
512KiB mod region and 532480-byte arena reservation are unchanged.

## Verification

- **975 host tests pass; zero failures/errors/skips.** Full suite includes
  popup lifetime/events, current-presenter binding/order, menu suppression,
  unchanged queue/refusal rules, existing native owner proofs, storage and
  exact branding/package/checklist checks.
- Both final PPC targets and the complete Wii launcher build pass. Resident
  Wii blob is **224465 bytes**, 68.5% of the 320KiB working limit: 5368 bytes
  less than .44. Dolphin blob is **203733 bytes**, 62.2%. Manifest alignment is
  reflected in packed artifact sizes below. 27 writes/33 checks remain.
- Clean JP DOL SHA1 **722005ea9c1eab54b114f814734d8f327e5614ee** authenticated.
  BPS independently reproduces target ISO CRC32 **9DBB89E0**.
- Both ZIP CRCs and exact seven/five entry sets pass. Every packaged file
  matches its source. HBC metadata has exact name/authors/version. Current
  and versioned checklists are byte-identical and contain 25 numbered cases.
- No interactive RC1 Wii/Dolphin gameplay or screenshot acceptance is claimed.
  The included 25-case stress run remains required, especially popup visuals,
  next-door behavior, repeated reboot archive reuse and sustained practice.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wii ZIP | 1163468 | CE0809A7287E48C5BD0C0F56F612F1CA335F5A5ADA5F2C0B4B2F85939818981F |
| Dolphin ZIP | 104233 | A7F96C89C54E4158695B5D08FB17F8D4633744F71618965E6B860DD3057957E1 |
| Wii mod_lmj.bin | 225224 | 2C1FE50F74A27C4F4F3E854FB91E84422078F685B60610AB12DF3A6B3CC7A3DE |
| boot.dol | 1570464 | FCA7BA02F9182D60B59E99CFA3F818A6CA6BE06A3846F6F45B13763F8D041748 |
| Dolphin BPS | 204012 | 9DA9BE6863EA44518E96EB200E7B1A6C81EF622B1F1E12572374EAD855469163 |

Wii whole-mod CRC32 **68AD7DDA**. Fresh RC1 gameplay states are required despite
unchanged layout 27, because the authenticated build identity has changed.
No game ISO, memory-card save or private SD key is packaged.

## SD handoff

The seven previous app files were copied and hash-verified under
`../sd-backups/lm-before-darkmoonshine-rc1-20260908/moonshine_luigis_mansion/`.
The fresh `protected-before.csv` inventories 62 root/state/settings/journal files;
all were verified unchanged before and after installation. No files were deleted.

The verified ZIP was staged under
`../lm-release-staging/darkmoonshine-1.0.0-rc1/`, then installed by copying only
the seven owned files to `D:/Apps/moonshine_luigis_mansion/`. Each was flushed
and reread-hashed. Matching ZIP is installed/verified at
`D:/lm_builds/DarkMoonshine-1.0.0-Frozen-in-Time-RC1.zip`. No duplicate app
folder, state migration, unrelated cleanup or other SD changes were performed.
SD released to the user for testing. No usage reset was consumed.
