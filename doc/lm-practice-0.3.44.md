# Moonshine Luigi's Mansion 0.3.44 — post-warp door and rumble fixes

## Changes

- Extends captured native room/furniture lookup state. The fresh .43 crash
  completed its Parlor load, then the normal Anteroom door followed a lookup
  pointer left over from the later live map. It interpreted an archive header
  as an address. The missing lookup and its proven adjacent scene-owned caches
  now rewind with their GAME resources. OS, audio and renderer safeguards remain.
- Successful loads stop the controller motor and clear stale rumble playback
  bookkeeping. This addresses rumble continuing indefinitely after restoring a
  quiet state. New gameplay rumble is not permanently disabled. Refused loads
  must not cancel an otherwise valid ongoing rumble.
- Snapshot format advances to **27**: create new .44 memory states and named
  SD archives. Earlier-build archives are left intact but cannot be loaded by
  .44. Preferences and SD keys keep their existing format; install the complete
  matching app. SD transport remains protocol **6**.

## Hardware evidence and next tests

The user confirmed .43's cold-boot baseline, the earlier Storage-to-Parlor
crash fix, SD deletion and faster timer/streak positioning. A longer sequence
still failed: import Parlor archive → warp Storage → load Parlor → warp
Boneyard → load Parlor → touch Anteroom door. This is the main .44 regression
route, alongside cancelling old rumble without breaking new rumble.

Native instruction and saved-image evidence support this targeted correction;
they are not a new Wii gameplay pass. See TESTING.md for **seven focused tests**.
No need to repeat the already-passed editor/deletion suite for this build.
Report delayed failures after movement or door use, not only whether a load
displayed the correct first frame. Keep diagnostic evidence on any failure.

One resident state, reusable named SD archives, deletion, timer Creation and
persistent settings remain available. Existing limits remain: not every boss
or map boundary is supported; mid-door loads are guarded; the native practice
timer rolls over after 36 counted minutes. Dojo/rush and exact trick-success
predicates are not added here. Dolphin has no Wii SD or persistent-preferences
service. No game ISO, memory-card save or private SD key is packaged.
