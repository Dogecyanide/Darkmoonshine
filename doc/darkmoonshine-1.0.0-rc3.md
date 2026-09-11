# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 3

Authors: **Dogecyanide, Nintendont Team**.

## Changes

- Displays → Sunshine timer → **Run in native menus** controls native START
  pause, Y map and Z Game Boy Horror timing. Default OFF preserves RC2's native
  menu behavior. The D-pad Down practice menu and Creation editor never pause
  an active timer, regardless of this setting. Genuine scripted stops remain.
- Room Tools → **Reset bind** records a custom 2–4-button reset combo. Default
  OFF. Press A, release the opening button, hold the combination, then release.
  B alone cancels; Z on the row disables it. Use full clicks for L/R. Start and
  D-pad Left/Right/Down are reserved. Outside the menu, an exact chord resets
  once and requires a full release before another reset. Existing room/event/
  transition checks still apply; choose a chord whose individual buttons do
  not start an unwanted interaction while assembling it.
- Both settings persist in `moonshine_lm.ini`. Valid previous settings are
  migrated with their original checksum verified; existing timer artwork,
  displays, colours and launcher choices are retained. Close the mod menu
  and allow the settings save to finish before powering off.
- Adapt Moonshine V2.3.0's startup approach: avoid the unused USB probe on an
  SD-only start, mount a selected game device on demand, initialize music only
  when actually showing the launcher menu, and skip the startup reveal delay.
  LM's custom injection, storage and native-runtime safety code is retained.
  Actual boot-time improvement still needs Wii measurement.
- Read launcher art/music from **`/Darkmoonshine_Theme`**. The launcher creates
  this folder on its own storage device when missing, including autoboot.
  Creation never replaces existing files. The ZIP includes copies of the
  supplied Moonshine `background.png` and `bgm.mp3`. Without assets the normal
  fallback still boots; folder auto-creation does not manufacture artwork.

## Install and compatibility

Put the ZIP's `moonshine_luigis_mansion` folder inside SD **`Apps`**. Put its
separate **`Darkmoonshine_Theme` folder on the SD root**, alongside `Apps`.
Keep an existing custom Darkmoonshine theme if you prefer it. The original
`Moonshine_Theme` is not renamed or removed. Install the matching `boot.dol`
and `mod_lmj.bin` together; this update includes a new preferences protocol.

The Wii launcher still uses an unpatched Japanese GLMJ01 ISO. Normal saves,
keys, archive and log paths are unchanged. Snapshot format remains 27 and
storage protocol 6; preferences become version 2 without a larger mailbox.
Create fresh RC3 savestates: older builds have a different authenticated
build identity. Do not relabel old archives to bypass that check.

RC2's user tests reported no issues in the exercised timer/display, same-room
and varied warp routes. The RC2 SD reboot test was not repeated. RC3 needs
the included ten focused Wii tests; compilation and host checks are not a
Wii or Dolphin runtime pass. No state-engine safety guards were loosened.
Secret Altar's unmatched event-resource refusal remains. Timer time is native
30 Hz game-update time, not lag-inclusive wall time; native rollover remains
36 counted minutes. Dojo/rush and verified trick-success detectors remain deferred.

No retail ISO, normal game save or private key is distributed.
