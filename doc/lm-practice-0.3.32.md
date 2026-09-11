# 0.3.32 experimental changes

This build targets the Room Warps menu crash, not a demonstrated failure of
same-room state loads. The tester confirmed two same-room loads worked in
0.3.31; Parlor to Anteroom through the menu produced corrupted video and a
crash. Foyer to Storage was also a menu-warp test.

## Changes

- Request a scene-2 reload through LM's native nonblocking scene queue. Its
  consumer waits for the normal transition checks and starts the fade/exit.
  The mod no longer forces the scene field to zero.
- Recheck current identity and I/O at both request and dispatch, without
  incrementing the frame-stability counter. Restore the exact touched flags,
  requested map, appearance point and pause request if native enqueue fails.
- Keep ownership of an accepted but stalled request; never abandon the
  temporary appearance point or edit native queue counters to cancel it.
- Give save-complete and warp events a 32-entry critical logging queue in
  the existing 4 KiB crash reservation. The ARM service drains it separately
  from sampled frame traces. Overflow is counted, never allowed to block the
  game. Older launcher/payload combinations retain sampled compatibility.
- Capture bounded owner, model-slot, configuration and private-heap windows
  for the exact reported fountain-renderer fault. The existing report remains
  2 KiB. This is evidence collection, not a null-pointer suppression patch.

Install the entire app folder: **both boot.dol and mod_lmj.bin changed**.
No patched ISO is required on Wii, and game saves/settings are not replaced.

## What the crash establishes

The current 0.3.31 report faults at `801717E0`, reading offset 12 from a null
model pointer in the fountain effect. That effect uses a separate 512 KiB
solid heap. Total GAME free memory cannot establish its remaining capacity.
The report does not prove an allocation failure or prove the queued warp
change alone resolves the crash. Do not enlarge or skip this owner blindly.

The captured EPOCH X05 is a separate archive-ownership refusal. Its safety
check remains; this version does not claim mansion-wide state compatibility,
King Boo state loads, or reusable archives after reboot. SD archives remain
reusable within their original running session only.

## Wii checks

1. Start fresh, without a mod-state load. Use Room Warps for Foyer to Storage,
   then Parlor to Anteroom. Move around and enter/leave a door afterward.
2. Restart, perform one same-room save/load, then repeat Parlor to Anteroom.
   This distinguishes a native warp failure from damage after a state load.
3. Report menu warp versus D-pad state load explicitly. Keep both attempt
   journals, `ndebug.log`, and the Luigi's Mansion crash reports. A pre-save
   warp can appear only in the main log/crash breadcrumbs; the attempt journal
   still starts when a successful save is observed.

Host tests and a successful build do not establish Wii runtime correctness.
