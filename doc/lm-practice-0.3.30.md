# 0.3.30 development changes

This is an experimental Japanese revision-0 build. Console releases still
boot the clean ISO through Nintendont. The separate Dolphin BPS is a developer
testing path, not a replacement for the console launcher.

## Controls

- D-pad left: save to the selected memory slot.
- D-pad right: load the selected memory slot.
- D-pad down: open the practice menu.
- The category home screen supports the control stick or D-pad. A enters a
  category. B returns to categories, then closes the menu.
- Inside a category: up/down selects, left/right adjusts, A applies. L/R
  switches categories directly; both analog trigger presses and digital clicks
  work. Each category remembers its cursor. Destructive actions require A twice.
- The game continues updating while the menu owns controller input.
- Hold Z after an EPOCH refusal for the detailed memory/resource report.

## New pages

Savestates offers two selectable slots. The active slot is uncompressed;
the other uses a bounded deflate-compressed cache. The captured 12.18 MiB
state fits that cache after compression. Slot switching can still refuse
if a different state exceeds capacity, without discarding the current slot.
The status reports the needed and available space. Three slots are not offered
because the measured data does not leave enough room for two inactive states.

Export writes a new numbered file under `lm_states/archive_00000001.lms`.
Choose its decimal archive ID to import it into the selected slot, then use
Load to apply it. A on the ID row chooses the latest completed file ID.
These archives are accepted only in the same running game session and build.
They are not portable across reboots, not Dolphin states, and not the
`lm_attempt_a.bin` / `lm_attempt_b.bin` diagnostic journals.

Room Warps is a scrolling list of recovered GaddWarp destinations with
provisional readable names. Left/right jumps ten entries; A twice requests
a native scene reload. It is not a coordinate teleport. See
[runtime warp notes](lm-runtime-warps.md) for exact compatibility checks.

Displays offers position, facing angle, room/map/HP, per-update displacement,
Moonshine's graphical controller overlay, and lag counters. The controller
uses Moonshine's stick gates, moving stick markers, button palette and
analog/digital trigger bars, adapted to LM's completed-frame renderer.
It shows the raw physical pad before menu neutralization. Position and
displacement are displayed in hundredths of a game unit (labelled `x100`).
Angles use the game's unsigned 16-bit turn representation, not degrees.
Displacement is an observation, not the internal velocity field; a warp
or other discontinuity must not be treated as physical speed.

The lag display counts extra VI retraces beyond the retail display queue's
requested interval, and separately counts updates that exceeded that interval.
It is not a CPU profiler or a count of intentionally skipped logic steps.
Menu and savestate I/O intervals are excluded. Counters are session-local
and do not rewind with a state.

Input Timing provides independent pearl and Chauncey reference profiles.
It measures the first monitored press and its hold length in completed game
updates. R/L include the analog trigger at 30 or above, not just the digital
click. Recording stores a reference; comparisons report early/late and
short/long relative to that reference, with adjustable tolerance. A successful
state load re-arms the measurement. Manual Arm starts after menu closure.
If the monitored button is already held, measurement waits for release.

**This is not yet a verified pearl-dupe or Chauncey one-cycle success detector.**
Exact inputs, event anchors and success conditions still need runner evidence.
No unverified frame-perfect L rule is hardcoded.

Luigi Colour offers presets and full red/green/blue channel controls. Hold
left/right to adjust a channel continuously. Only green endpoints in the
verified normal gameplay model's shirt/cap textures are changed, using an
immutable original backup. Original restores the exact texture bytes. The
setting is session-local, reapplied after a state load, and does not yet
cover separate cutscene models. See [colour notes](lm-luigi-colour.md).

## Test priorities

Use a fresh state for each route. Test same-room load first, then one-room,
multi-room and floor changes. Test a state saved *inside* King Boo separately
from a state saved in the mansion and loaded from King Boo: the latter crosses
a full map/scene boundary and needs different reconstruction.

For slots, save two clearly different positions, switch between them and load
each. Report a capacity refusal rather than assuming it is a crash. Export a state,
save a different position into the slot, import the old file in the same
session, then load. Power-cycle imports are deliberately refused.

Keep the last two diagnostic attempt journals and any Luigi's Mansion crash
reports. Include the build number and the exact route/door/arena in the report.
The journals are evidence about transactions; they do not contain playable
full savestates.
