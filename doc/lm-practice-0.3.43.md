# Moonshine Luigi's Mansion 0.3.43 — restore fix, faster positioning and SD deletion

## Changes

- Captures the missing native RoomInfo lookup at `803C2138..803C236C`
  (end exclusive). The .42 Storage-warp crash evidence showed that a loaded
  snapshot could restore room resources while this lookup still contained
  pointers from the later live room. The new range restores that lookup with
  the snapshot. This is a targeted fix candidate, not a universal crash claim.
- Snapshot format advances to **26**. Make fresh .43 memory states and named
  archives. Earlier builds' archives remain visible but cannot be loaded.
  Install the complete matching app: SD transport advances to protocol **6**.
- Creation keeps precise two-pixel D-pad taps, but a held direction now
  accelerates. A full 640-pixel crossing takes 83 editor updates, about
  **2.77 seconds at 30 Hz**. Release, reversal, disconnect and confirmations
  reset acceleration. Timer and streak use the same controls; colour and
  size adjustments retain their previous repeat rate.
- The SD archive browser now offers **Z: Delete**. Confirmation freezes the
  selected archive's name, ID and catalog token. A fresh A press permanently
  deletes it; B or controller disconnect cancels before submission. A pending
  deletion blocks further browser actions until its result arrives.
- Successful deletion frees the SD archive file and removes its name when
  possible, then refreshes the list. The already-resident memory state and
  other archives are unchanged. Old-build or corrupt archives can also be
  deleted; a stale catalog or busy operation refuses safely.

## Deletion safety

Back up useful archives first and test with disposable copies. There is no
automatic undelete. Never remove the SD during a pending operation.

`DELETED; NAME CLEANUP FAILED` means the state file was deleted but
name cleanup did not finish. It is not a complete deletion failure: preserve
the remaining files and logs. Orphaned name-file IDs are not reused for new
exports, avoiding accidental association with an old name.

## Evidence and limits

The user passed .42 named archive loads after both soft and full Wii reboots,
timer visuals/toggles without noticeable lag, and persistent preferences.
The later Storage-warp/load path still crashed; its captured pointer mismatch
motivated the RoomInfo change above.

The .43 Creation harness passes precise-tap, held-movement, bounds, reversal,
disconnect, confirmation and unchanged colour/size tests. Host checks and the
proved missing lookup do not replace Wii acceptance: **.43 has not yet passed
the new runtime checklist**. See TESTING.md for eight focused tests.

Preferences, durable SD keys and the one resident state remain as before.
Keep existing keys, settings and diagnostic evidence. No timer endpoint,
clock rollover, Dojo/rush or boss-map support expansion is claimed; the native
practice clock still rolls over after 36 counted minutes. Dolphin has no SD
archive/deletion or persistent-preferences backend.
