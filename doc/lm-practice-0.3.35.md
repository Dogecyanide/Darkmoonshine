# 0.3.35 experimental changes

This is a hardware-test candidate, not a 1.0 release. The main acceptance
tests are repeated post-menu-warp and cross-floor state loads, followed by
ordinary movement and door use. See [the included runner checklist](lm-testing-current.md).

## Post-warp savestates

The new 0.3.34 SD journals show X06/X08 compatibility refusals after native
scene reloads, not new exceptions. A same-room menu warp can rebuild an
archive under the same model ID, which the previous rule rejected. There
is also a separate scene VR archive outside the 262-row model registry.

The new proof checks the saved and live model endpoints individually and
accounts for that separate owner. Completed room-streamer records may be
rebuilt with the same room ID only when both records pass explicit native
layout, callback, state and GAME-memory bounds checks. Unaccounted archives,
pending model requests and unknown field changes still refuse.

Snapshot format **19** captures the missing fixed VR-model output and
animation arrays, adding **480 bytes** to the raw snapshot. It does not
increase the MEM2 reservation or rewind the SYS heap. Make fresh states.
The [scene-reload proof](lm-state-scene-reload.md) separates measured journal
facts, authenticated retail code and the remaining runtime uncertainty.

## Controller and R-pump

The native JP game requests the older mode-0 controller report. Older Phob 2
firmware can reply in mode 3, causing L pressure to appear in the game's R
field. This matches the reported physical L vacuuming and triggering R-pump,
but the user's exact firmware is unknown. The official
[Phob v0.31 notes](https://github.com/PhobGCC/PhobGCC-SW/releases/tag/v0.31)
list a Luigi's Mansion trigger fix for Phob 2.

Two authenticated initialization writes make both JUT's stored mode and its
PAD request mode 3. Native decoding, calibration and button assignments remain
in use; L/R are not swapped in the overlay. Verify physical triggers on Wii.

Displays -> R-pump display remains a separate default-OFF toggle. It now uses
a small right-side popup while R is held and for 90 game updates after
release. It is hidden when idle, and menu/state/player/disconnect changes
cancel a partial measurement. Counts are game updates, not a verified
pearl-dupe success test. See [R-pump notes](lm-r-pump-display.md).

## Practice additions

- Fix the packed room/table-index comparison behind Blackout's persistent
  BUSY refusal. The active room table has 74 records, not 72.
- Reset supported current rooms through native scene reconstruction after
  applying their recovered dark/uncleared-state recipe. Entry coverage is
  60 distinct mansion room IDs plus the four existing boss presets; twelve
  hall/unused segments have no validated reset entry and report unavailable.
- Add the thirteen missing clear recipes, completing the source recipe
  partition for GaddWarp's 72 ordinary logical mansion IDs. This is not a
  claim of 72 room-by-room hardware passes.
- X on Room Warps toggles the existing alternate Boo-safe entry points.
- One Poltergust selector sets/refills NONE, FIRE, WATER or ICE. It requires
  the corresponding medal; it does not spawn physical pickups or grant medals.

Room reset/clear requires confirmation and changes live progression, but
does not automatically save to the memory card. Back up the card before
runner testing. See [room-tool coverage and limits](lm-room-tools-runtime.md)
and [authenticated tank behavior](lm-elements.md).

## Still limited

Door-animation states remain refused with BUSY/DOOR. Boss-map state changes,
cross-reboot SD portability and persistent in-game preferences are not fixed
by this build. SD archives remain separate from the compact attempt journals.
Dojo and rush modes are deferred while runners advise on their importance.
Other unported GaddWarp features are listed explicitly in the room-tool notes;
this is not advertised as complete GaddWarp parity.

Every LM launcher ZIP now includes a version-checked `TESTING.md` so testers
have the precise sequence and the diagnostic files to return.
