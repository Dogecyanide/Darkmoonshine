# Moonshine Luigi's Mansion 0.3.42 — timer, preferences and reboot-map candidate

## Changes

- Displays → Sunshine timer has Visible, Edit, TIME icon, and Edit streak.
  Retail Sunshine textures and pane geometry retain the individually angled
  digits, punctuation, TIME label and streak. The default position is upper
  right for LM. Testers do not need a Sunshine ISO.
- The heapless Moonshine Creation editor supports position, size, per-character
  RGB including TIME, opacity, brightness, background and padding. The streak
  has an independent editor. START selects the next target; X+START goes back.
  D-pad moves, L/R resize, C-stick selects/adjusts options. A keeps, B discards,
  Z resets the selected option; each asks for confirmation.
- In-game preferences are now saved by the Wii launcher when the main mod menu
  closes; recorded timing references also autosave when recording completes.
  Displays, timer styling, Luigi colour, timing references and Boo-safe warp
  preference persist. Gameplay health, money, flags, progress, elapsed
  time and savestate contents are NOT preferences. Loading a state does not
  rewind preferences. Wait for `SETTINGS: SAVED TO SD` before turning off;
  reopen the category menu to check status. Dolphin has no preference storage.
- Preferences live in `/moonshine_lm.ini`, section `[lm_preferences]`.
  Updates preserve launcher settings and use `.lm.tmp` and `.lm.bak` companion
  files for a recoverable replacement: write the temporary file, retain the
  previous INI as backup, then promote the completed file. Do not remove these
  files during a write or power off before the success message.
- The clock reads LM's captured native game-update counter at 30 nominal updates
  per second. Injected menus pause counting without changing the saved active
  flag. Loads rewind it; native map reloads reset it, ordinary doors do not.
  Native event stops/starts remain, plus GaddWarp's Boo-capture stop and next
  Boo-introduction resume. This is game time, not a wall/lag timer.
- Reboot EPOCH/X08 fix candidate: MissionMode's map archive now has an explicit
  native-owner proof, instead of being incorrectly required to be a model row.
  Saved/live mission, wrapper and entire map backing each prove their GAME
  allocations and native relationships. Other archive, audio, SYS, rendering
  and session compatibility checks remain mandatory.
- Unknown-volume refusals now identify which saved/live archive remained
  unmatched in the attempt logs. The archive browser also explains that Import
  fills memory and Load restores gameplay; no second manual-ID import is needed.

## Evidence and limitations

The .41 report passed long natural routes, cross-floor routes, loads after
menu warps, door re-entry, gameplay rewind and alternating named archives.
Soft/cold reboot loads still refused. Both supplied ZIPs were preserved and
decoded; no exception reports were present. The new map proof passes the saved
graphs in all three archives, but this is not a verified .42 Wii reboot pass.

The timer is not a complete replacement of GaddWarp's modified event assets.
Custom Dojo/rush flows are not added; the cemetery cutscene's earlier
branch-specific stop is not mirrored. There is no universal room-clear stop
in the recovered scripts. The native clock rolls over after 36 minutes of
uninterrupted counted gameplay; use this as a room/practice timer, not a
full-run timer. Confirm runner timing endpoints before relying on readings.

Use fresh .42 archives: cross-build states remain visible but incompatible.
Do not delete the .41 data or durable SD key files. One resident state, eight
attempt journals and two exception reports remain. Memory reservations and
snapshot format are unchanged. Install the complete matching app folder.

Preferences and new reboot restores require Wii testing; host tests are not a
substitute. No .42 Dolphin runtime test has been performed. See TESTING.md for
the focused 16-case checklist.
