# 0.3.36 experimental changes

This candidate targets post-menu-warp state failures and the ownership faults
in the runner reports. It is **not a 1.0 release or a mansion-wide compatibility
claim**. Install the matching launcher and payload together, and make fresh
states: snapshot format is now **21**. See the included `TESTING.md` for the
version-specific test sequence and files to return.

## One resident state, broader capture

- One resident state replaces the previous two-slot arrangement. SD can keep
  multiple archives, but only one state is resident at a time.
- The state now includes the complete shared Japanese `Game/game.szp` resource
  payload, compressed alongside the GAME/static snapshot. This captures mutable
  model/animation data and resource caches that a menu scene reload could leave
  inconsistent with restored GAME objects. It does **not** rewind the SYS heap,
  OS threads, audio machinery or graphics buffers.
- Changed GAME archive wrappers can borrow captured SYS resource bytes only
  through the validated native memory-archive path. Unknown owners, unrelated
  SYS buffers and invalid model/resource relationships still refuse.
- Saved and live GAME roots and room-resource allocations are checked separately
  when a native scene reload moves them. Room distance alone is not an admission
  rule; map/archive and other uncaptured identities remain guarded.
- Additional scene HUD and effect-manager owners are captured together. Particle
  graph checks run before restore and again after the copy. A failed post-copy
  check enters the crash dumper instead of resuming a known-invalid renderer.

Shared-resource compression is staged before overwriting the previous state.
A capacity refusal preserves it. These are source-backed changes awaiting
integrated runner validation, not a claim that every reported crash is fixed.

## SD browser and safer feedback

States -> Browse SD archives lists actual files, eight per page, with their size
and compatibility. Import asks for confirmation, fills the resident state, then
waits for an explicit Load. Manual archive IDs remain available.

Failed imports now identify the requested file and say whether the previous
state was kept. A complete rollback is held outside the entire import window,
including when the incoming file is larger. Export never overwrites an existing
archive. The new browser/response transport requires the matching protocol-3
launcher.

**SD states are still same-session only.** Soft reset, a new boot or another mod
build invalidates compatibility. The browser displays incompatible files but
does not bypass authentication to import them. This is not cross-boot storage.

## Displays and input diagnostics

- The Moonshine-style controller diagram now shows D-pad directions.
- Metadata includes horizontal speed, explicitly in game units per update.
  State/menu/player discontinuities do not count as movement speed.
- Lag counting reads the corrected native display interval and distinguishes
  extra video retraces from slow game updates. It is not a benchmark certification.
- States has a sticky `Dpad / Link / Edge / Use` line to help identify whether
  a direction tap reached input sampling, queued an action or was blocked.
  This instrumentation is **not a confirmed D-pad save/load fix**.

## Room and encounter practice

- Foyer reset/clear follows the native linked-room mapping, including the
  distinction between table rows and logical room IDs.
- Scoped native event play-count resets rearm Parlor/Astral Hall/Nursery
  encounters and the four boss intros. Their existing room/flag recipes and
  transition checks remain active. These replay retail events; they are not
  global cutscene skipping or complete rush modes.
- Tank preset no longer mistakes first-element event flags for medal ownership.
  It sets/refills NONE, FIRE, WATER or ICE without granting medals or changing
  acquisition records. Non-idle weapon actions still refuse.

Room reset/clear and boss replay change live progression but do not automatically
save a memory card. Keep a backup before testing complex encounters.

## Still open

Dojo, Boss Rush and Portrait Rush are not implemented. Cross-map/boss-arena
states, cross-boot SD portability, persistent in-game preferences, archive-name
editing, infinite health and verified trick-success feedback remain outstanding.
Door-transition safety refusals remain intentional.

Native-code, ownership, codec and transaction tests support the implementation.
Dolphin input delivery prevented completing interactive acceptance testing for
this candidate; **no new Wii hardware pass is claimed**. Repeated loads must
also survive subsequent movement, ordinary door entry and another load before
this can be called reliable.
