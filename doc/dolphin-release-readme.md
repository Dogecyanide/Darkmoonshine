# DarkMoonshine — V1.0.0 Frozen in Time

Dolphin development patch. Authors: Dogecyanide, Nintendont Team.

This is a development patch for your own Japanese Luigi's Mansion revision-0
GameCube ISO (GLMJ01, 1,459,978,240 bytes). Apply the included `.bps` with a
BPS-compatible patcher, keeping your original ISO. The source checksum must
match; do not force the patch onto another revision or a modified ISO.
No game image or game save is included.

The Wii/Nintendont release is separate and does **not** need a patched ISO.
Do not put this Dolphin-only payload into the Wii launcher.

## Runtime status and setup

No V1.0.0 Dolphin gameplay acceptance is claimed. Compilation, host tests and
patch validation are not a Dolphin runtime pass. Earlier development used
Dolphin 5.0 Win64 with MMU disabled and Real XFB enabled for the CPU-rendered
menu/overlays, but controller input remained unreliable in some test profiles.
Wii D-pad shortcuts work according to hardware testing; this release
prioritizes the Wii reports.

Use a separate test profile/card or back up your usual saves. Imported
memory-card saves must be Japanese-region. Never load a native Dolphin
checkpoint from another mod build: it also restores the old injected code.

Dolphin 5.0 movie playback overrides GCI-folder cards with a RAW card.
Boot normally without a movie to use a completed save in `GC/JAP/Card A`.
A movie needs a separately verified populated RAW card; do not format a
completed card to work around this profile mismatch.

## Controls

- D-pad Left: save the selected mod state.
- D-pad Right: load the selected mod state.
- D-pad Down: open the practice menu.
- Stick/D-pad: navigate. A: select. B: back/close.
- L/R: switch menu pages. Left/right: adjust an option.

There is **one expanded in-memory state**, including a compressed copy of the
persistent shared game-resource archive. Saving may pause longer; a capacity
refusal preserves the previous state. The States page's Dpad/Link/Edge/Use line
records closed-menu shortcut input: tap, release and inspect it instead of
holding the button.

The permanent memory/version panel and heartbeat are removed. A small top-left
popup briefly reports Saving/Saved, Loading/Loaded, Busy or Rejected. Detailed
state/refusal information remains in the menu; background guards remain active.

Room Warps, Displays, Input Timing, Luigi Colour and practice options are
available. Warps perform native scene reloads; they are not savestate loads.
Luigi recolouring covers his normal gameplay cap/shirt, not every separate
cutscene model.

The R-pump display is an optional right-side popup, hidden while idle. It
counts digital R or raw analogue R at least 30; this is an input measurement,
not a verified trick-success predicate. The press update counts and the release
update does not. Menus, state operations, player replacement and disconnects
cancel incomplete measurements.

## Sunshine timer and Creation

Displays → Sunshine timer provides Visible, Edit, TIME icon and Edit streak.
It uses Sunshine's retail artwork and individually angled panes, positioned
upper right by default. Editing covers position, size, per-character colour,
opacity, brightness, background and padding; the streak has its own editor.

D-pad taps move two pixels precisely. Hold to accelerate: a full 640-pixel
crossing takes 83 editor updates, about 2.77 seconds at LM's normal 30 Hz.
Release, reversal, disconnect and confirmations reset acceleration.
C-stick selects/adjusts options; L/R resize at the unchanged rate.
START selects the next target, X+START goes back. A keeps, B discards and
Z resets the selected option, each with confirmation.

The timer reads LM's native game-update clock, not wall time or lag-inclusive
time. Mod menus leave it running, state loads rewind it, native map reloads reset it,
and ordinary doors retain it. GaddWarp's Boo-capture stop and next
Boo-introduction resume are mirrored; custom Dojo/rush timing and the
cemetery's earlier branch-specific stop are not included. The native clock
still rolls over after **36 counted minutes**: it is a practice timer,
not a full-run timer.

## V1.0.0 changes and remaining limits

RC4 adds independent 128 KiB LZ4 blocks for the shared-resource companion,
dense fallback when needed and equivalent table CRCs. All integrity and
ownership checks remain. Host round trips pass, but Wii/Dolphin pause times
are unmeasured. A second complete RAM state did not fit safely in the current
layout, so one slot remains.

RC3 adds Displays → Sunshine timer → Run in native menus (default OFF), and
Room Tools → Reset bind (default OFF). Native START/Y/Z menu time is optional;
the D-pad Down menu always counts an active clock. Record a 2–4-button combo
using A on the binding row, release the opening A, hold the combo and release.
B alone cancels; Z on the row disables it. Use full L/R clicks. Start and
D-left/right/down are reserved. The shortcut uses the same guarded reset as
the menu, triggers once, and needs a full release before another activation.

.43's RoomInfo fix passed the user's Storage-to-Parlor test. A later sequence
(Parlor SD state → Storage warp/load → Boneyard warp/load → Anteroom door)
exposed another stale native furniture lookup. .44 captures that lookup and
its proven adjacent scene-owned caches. Successful loads also stop stale motor
rumble and reset playback bookkeeping, without permanently disabling new rumble.
The user subsequently reported no observed .44 crashes. RC2 kept these fixes,
removed RC1's menu-only timer stop, optimized timer drawing and clarified
element waits. The maintainer reports all ten RC4 Wii tests passed; V1.0.0
promotes that implementation with final branding and packaging only.
Create **fresh V1.0.0 format-28 states**. Earlier builds have different build
identities even when their layout version matches.

The user passed .42 named soft/full-reboot archive loads, timer visuals and
persistent preferences, then .43 cold-boot, Storage-load, SD deletion and faster
positioning checks on Wii. Those results do not establish Dolphin runtime
acceptance or universal mansion/boss support. Ordinary mansion resource
ownership and compatibility guards remain; mansion-to-boss/boss-to-mansion
loads are still guarded. Loading during a door action should return BUSY/DOOR;
retry after the transition. Mid-door snapshots are not supported.

Secret Altar can load event resources that safely reject an earlier ordinary
mansion state. Entering the room and loading a state are separate operations;
V1.0.0 does not bypass this ownership guard or claim a verified escape route.

Input Timing compares against recorded references; it is not yet a verified
pearl-dupe or Chauncey one-cycle success detector. Blackout, supported room
reset/clear, Boo-safe entries and Poltergust tank presets remain available;
tank presets do not grant medals. Dojo and rush modes remain deferred.

## Wii-only SD features

Dolphin has no Wii ARM SD service. SD archives, the new Z → A permanent-delete
flow, durable-key operations, automatic SD diagnostic/crash files and
persistent preferences are **Wii only**. Preferences stay session-local in
this Dolphin patch; do not infer SD behavior from an emulator test.

On Wii, preferences save when the main menu closes and after timing-reference
recording completes. Wait for `SETTINGS: SAVED TO SD` before turning off.
They live in `/moonshine_lm.ini`, section `[lm_preferences]`; `.lm.tmp`
and `.lm.bak` companions support recoverable replacement while preserving
launcher settings. Gameplay progress and savestate contents are not preferences.

The Wii archive browser's deletion is permanent and leaves its resident memory
state unchanged. Test only disposable copies. Old-build/corrupt archives may
be deleted; stale or busy requests refuse. A name-cleanup warning means the
state file was already removed, not that an undo is available. Install the
whole matching V1.0.0 Wii app for transport protocol 6 and preferences version 2; this Dolphin patch is not
a replacement for any of its files.

The renamed app keeps its existing SD folder, settings, state and log paths.
See TESTING.md for ten focused Wii retests. Send the build, exact route/action,
refusal text and screenshots with failures. Dolphin results do not prove Wii
cache, timing or SD correctness.

Includes miniz under the MIT license in `licenses/miniz.txt` and LZ4 under
the BSD 2-Clause license in `licenses/lz4.txt`.
