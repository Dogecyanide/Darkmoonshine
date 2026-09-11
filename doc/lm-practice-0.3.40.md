# Moonshine Luigi's Mansion 0.3.40 — named SD states

The user passed all twelve focused .39 Wii route/restore tests, including
same-session SD export/import. This release keeps the same snapshot layout
and restoration logic, and adds persistent names to the SD browser.

## Added

- Export asks for a name using Moonshine's controller keyboard, up to 31 characters.
- SD browser: names first, archive ID and compatibility underneath. X renames
  the selected archive; Y refreshes. Blank names fall back to the archive ID.
- Names persist across reboot. Renaming never rewrites the state file or
  replaces the state currently in memory. Old-session files can still be named.
- Names use separate checked, alternating metadata records. A failed rename
  retains the previous valid name; damaged metadata falls back to the ID.
  If export succeeds but naming fails, the result explicitly reports both facts.

## Keyboard

Stick/D-pad chooses a key. A types; B backspaces; X adds a space; Y changes
case; L/R switches letters/symbols. Z offers clear. START then A keeps the
name. Hold X and press START, then A, to discard. B cancels a confirmation.
Wait for the completion message before powering down or removing the SD.

## Installation and limits

Install the whole matching app folder: the SD naming protocol changed, so
the launcher and injected payload must both be .40. Use fresh .40 states;
archives from a different mod build are not importable. One resident state,
snapshot format 24, and the MEM1/MEM2 reservations are unchanged. The mailbox
reservation grew by 512 bytes within the existing scratch region.

**Loading an SD state after reboot is not fixed in .40.** Names survive, but
the state still belongs to its original running session. Removing the EPOCH
check would leave stale external owners and a different authentication key.
The included checklist asks for two fresh Foyer exports on separate boots to
compare retained ownership before implementing a persistent-state format.
Do not test cross-boot imports expecting them to work yet.

Back up the entire `lm_states` folder: `.lms` files hold states and `.name0` /
`.name1` files hold labels. Nothing in this release deletes existing archives.
In-game preferences are still session-local. Boss/map boundaries, Dojo and
rush modes are not newly enabled. Host tests are not a Wii visual/input pass.
