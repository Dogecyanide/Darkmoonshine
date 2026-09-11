# 0.3.33 experimental changes

The 0.3.32 Wii tester reported menu warps and D-pad saving worked. Saving a
state, performing a menu warp to Anteroom, then loading crashed. The new
report completed LOAD/7F and failed on the first draw at `8012E3A4`, with an
invalid grain-controller self-sentinel (`FFFFFFFF` instead of node+`40`).

## Targeted changes

- Capture all three native model-effect managers as separate `0x2A8` objects.
  Their private heaps/model arrays were already in GAME, but their fixed
  owners were omitted. Do not copy the 12-byte destructor registrations
  between objects. See [retail proof](lm-effect-manager-reload-proof.md).
- These managers update models before the failing grain renderer. A stale
  model pointer can write the exact grain field that failed. The missing
  owners and execution order are verified; the precise alias in this Wii
  attempt remains a hypothesis, not a reproduced write trace.
- Validate all 80 controller self-sentinels before overwriting the old state
  during save and before modifying the game during load. Preserve state on
  source-validation failure. No null skip or pointer repair is applied.
- Record a read-only post-copy check before resuming. It does not undo a
  completed restore. Save-source (`5F`), saved-source (`07`) and restored-copy
  (`6B`) evidence uses the bounded critical queue, preserving both fault
  address and value against later sampled draw events.
- Decode the room from the packed player's low byte, with only full-word
  `FFFFFFFF` treated as unset. This fixes false ROOM MISMATCH results and
  misleading room metadata. The destination list itself is unchanged.

Snapshot format is **17**. Make fresh states: older snapshots have a different
layout and are rejected. The extra captured objects total 2,040 bytes; packing
alignment makes the total raw snapshot grow by 2,016 bytes. MEM1/MEM2
reservations are unchanged. Two compressed slots and session-local SD archives
remain available under their existing capacity/identity checks.

## Test

Repeat the failing Wii sequence: save in Parlor, menu-warp to Anteroom, load.
After a successful load, move around and use a door, then repeat in reverse.
Report the build number and whether you used a menu warp or an ordinary door.
Keep both attempt journals and LM crash reports. The three grain evidence
records distinguish an invalid source from corruption after restoration.

This is not a claim of mansion-wide, boss-map or cross-reboot correctness.
Additional shared-resource capture and persistent in-game preferences remain
unfinished. Emulator and hardware results must be reported separately.
