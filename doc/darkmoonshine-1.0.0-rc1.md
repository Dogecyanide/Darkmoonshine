# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 1

Displayed authors: **Dogecyanide, Nintendont Team**.

## Changes from .44

- Launcher/Homebrew Channel and the practice-menu home screen now use the
  DarkMoonshine name and RC1 version. Upstream license/source notices remain.
- The permanent memory/version panel and heartbeat are replaced with a small,
  brief top-left action popup: Loading, Busy, Rejected, Loaded, plus Saving/Saved
  feedback. Repeating an action re-arms the notice; idle gameplay stays clear.
  Refusal details remain available in the States menu and diagnostic files.
- Heap/canary checks, restore guards, crash reports and the eight rotating
  attempt journals remain active in the background. Removing text does not
  remove diagnostics or weaken the safety checks.
- Includes a 25-case runner stress checklist covering long routes, repeated
  warps/restores, SD reuse after reboot, gameplay rewind and sustained practice.

## Compatibility and installation

Install the complete matching app. Its on-SD folder remains
`Apps/moonshine_luigis_mansion` so upgrading replaces the existing app instead
of creating a duplicate. The settings file `moonshine_lm.ini`, `lm_states`,
`lm_dumps`, and `luigis_mansion_crash_a/b` filenames remain unchanged. Existing
preferences, keys, normal saves and diagnostic evidence should be kept.

Snapshot layout remains **27**, SD transport remains **6**, and memory
reservations are unchanged. Create new RC1 states and named archives: .44 and
other builds have a different authenticated build identity and cannot be
imported as RC1 gameplay states. Do not rename/relabel their headers to bypass
this check. SD Import fills the memory slot; Load restores gameplay.

## Hardware evidence and known limitations

The user reported no observed crashing in the .44 tests. The supplied newer
journals contain successful Secret Altar entry followed by safely rejected
savestate loads; the root crash reports are unchanged older .43/.42 reports.
The live `event06` and later `event74` resources cannot yet be matched to the
earlier Parlor state's validated resource ownership. This is a known Secret
Altar state-load boundary, not a proven outgoing room-warp failure. Its guard
is intentionally preserved in RC1; see `lm-secret-altar-rc1-limit.md`.

This is a release candidate, not final 1.0 acceptance or proof of arbitrary
boss/mansion restores. Mid-door loads remain guarded. Dojo/rush modes and exact
pearl-dupe/Chauncey success judging are not added. The native practice timer
still rolls at 36 counted minutes. SD archives/deletion and persistent
preferences require the Wii launcher; Dolphin has no matching SD service.

See TESTING.md for the stress test and evidence collection instructions.
No game ISO, memory-card save or private SD key is distributed.
