# 0.3.31 experimental changes

This is a targeted reliability build, not completed mansion-wide or reboot-safe
SD support. Existing 0.3.30 practice features and session-only settings remain.

## Changes

- Capture D-pad left/right edges at the physical PADRead hook, before the menu
  or game can neutralize the sample. Brief presses survive until the completed
  presenter services them. Menu-owned input, disconnects and held-button
  repeats remain suppressed.
  The closing A/B debounce no longer suppresses a fresh D-pad action while
  another button is still held.
- Correct one verified false resource refusal: when both room slots are
  inactive and fully idle, ignore only the old callback and backing-cache
  words that retail initialization replaces before reuse. All other record
  bytes, active-resource changes, map identity and ownership checks remain.
  See `../docs/lm-resource-slot-idle-proof.md` for the Japanese retail evidence.
- BUSY retains the action's failed gate/value instead of displaying the next
  frame's idle `OK`. `STABLE` means too few unchanged samples; `CHANGE` means
  identity changed during the attempt. DVD/ARAM/card/audio/camera/proof checks
  remain enforced. There is no deferred automatic save/load after a refusal.

## Runtime verification required

1. In gameplay with the menu closed, tap left, move, tap right. Repeat while
   holding a face/shoulder button. Confirm that holding a direction does not
   repeatedly save/load, and menu navigation does not trigger a state action.
2. Repeat the rejected multi-floor route using a freshly created state. After
   a successful load, enter/leave doors and continue moving before calling it
   a pass. The recorded X06 fixture alone proves only the corrected predicate.
3. For a remaining BUSY refusal, report the `G:` reason/value and the action.
   For EPOCH, hold Z and capture the detailed report; keep both attempt journals.

SD archives remain usable repeatedly in their original running session only.
King Boo/full-map loads and post-reboot imports are not claimed fixed here.
The additional shared-resource capture described in the SYS audit is not yet
part of this build.
