# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 2

Authors: **Dogecyanide, Nintendont Team**.

## RC1 runner evidence

The September 10 report passed the ordinary mansion state routes, repeated
menu warps and doors, SD imports after soft/full reboots, rumble, preferences
and archive management without a reported crash. Eight valid RC1 journals
and two structurally valid format-27 archives accompany the report. These
results do not establish unrestricted boss/mansion support.

The critical finding was untimed gameplay: RC1 skipped the native timer
update while the practice menu was open even though simulation continued.
The runner also reported display flicker and the user reported timer lag.

## Changes

- Remove the menu-only timer-update hook. LM's original updater now runs
  unchanged: opening the mod menu, editing displays or hiding the timer
  cannot suppress game time. Native event stops, resets and state rewind stay.
- Optimize the Sunshine-style software timer renderer, preserving the native
  angled artwork and editor. It still composites onto each fresh game frame;
  it does not skip alternate frames or slow the clock to reduce rendering work.
- Hide optional gameplay displays while native single-buffer presentation is
  active (pause/Game Boy Horror and related screenshot transitions). They
  return automatically in normal gameplay; the clock is unaffected. LM uses
  the other framebuffer as a background texture in that mode, so forcing
  double buffering is not safe. This reduces the known flicker source; it is
  not a claim that every native-menu or gameplay flicker has been eliminated.
- Distinguish element-fill wait reasons instead of telling players to release
  suction/spray for unrelated transitions, events or unavailable game state.
  Existing safety checks remain in place.
- Include ten focused retests, prioritizing the critical timing exploit,
  rendering performance/flicker and the important state/SD paths.

## Install and compatibility

Install the complete matching app into `Apps/moonshine_luigis_mansion`.
The launcher still uses an unpatched Japanese GLMJ01 ISO. Settings, normal
saves, keys, `lm_states`, `lm_dumps` and crash-report paths are unchanged.
Snapshot format remains 27 and storage protocol remains 6. Create fresh RC2
states: prior-build archives have a different authenticated build identity
and must not be relabelled to bypass compatibility checks.

Secret Altar's unmatched event-resource state-load refusal remains guarded.
A failed SD import leaves the previous resident state intact; that is not a
successful import of the missing/deleted archive. The timer is native
game-update time, not lag-inclusive wall time, and rolls over after 36 counted
minutes. Dojo/rush and verified trick-success detectors remain deferred.

This is a release candidate. No RC2 Wii/Dolphin gameplay acceptance is claimed
from compilation or host testing. See TESTING.md and the verification record.
No retail ISO, normal save, or private key is distributed.
