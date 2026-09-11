# Runtime room tools and GaddWarp coverage

This describes source implementation, not a claim of Wii verification for
every room. Evidence is the authenticated clean GLMJ01 DOL and recovered
GaddWarp Event77; no modified game asset is shipped.

## Blackout refusal

The old menu compared the complete word at Player+`0xB4` to the high byte of
the room manager's `+0x0C`. This rejects ordinary rooms even when both objects
agree. The packed value has a physical table index in its high byte and a
logical room ID in its low byte. Parlor is `24010323`: table index36, room35.

`roomActionReady` now validates both packed words against the active room
table record, instead of mixing the two ID domains. The live room count is
the byte at `804A0CF4`; clean map2 has74 table entries, not72. The74 records
contain logical IDs0..73, while GaddWarp's ordinary clear recipes cover0..71.
Every dependent pointer/extent is checked first, and actions recheck current
state/I/O rather than trusting the previous presenter's ready flag.

Blackout still calls only the retail live setter `80037498(0/1)`. Its
flag61 update and MissionMode lighting-list switch are unchanged; it is not
a cosmetic LIGHT-word write and does not call PLIGHT to advance room progress.

## Reset/reload lifetime

1. User selects Reset in Room tools and confirms with a second A press.
2. The warp module chooses that room's recovered spawn row (or the current
   boss's existing native preset), and arms the normal queued transition.
3. At execution it rechecks the engine, door/event and I/O gates. Mansion
   requests also validate the current room and build the reset recipe.
4. The native queue sender must accept the reload before any room recipe is
   applied. A failed queue therefore leaves room progression unchanged.
5. Accepted mansion reset clears the persistent room-lit bit and applies
   only its recovered flag repairs. The queue consumer runs later on this
   same game thread, so reconstruction sees the completed recipe.
6. The normal full scene teardown/reload rebuilds actors and live room state;
   the existing shadow CharacterInfo row preserves native spawn ownership.

The reset does **not** blank arbitrary actor memory, rewind live OS/audio/GX
objects, invoke an event-handler interior, or automatically save a memory card.
Old savestates remain separate and can restore earlier progression if their
usual compatibility checks pass.

Retail `800186A0` reads halfword `803C2EB0 + 2*logicalRoom`, bit1, to initialize
live lit bit `room+6.80` and sticky word `room+14.8`. Clearing this persistent
bit before a full reload is therefore materially different from only turning
the old live room dark. Other persistence bits are preserved. Foyer has a
linked second room, selected by the native index at `80398C8C`; both persistent
lit bits are cleared, following the special case in `8001989C`.

`lm_room_tools.h` contains the reset flag recipes. They include Parlor,
Storage, Graveyard, Nursery, Twins, Guest Room, Astral Hall, Observatory,
Ceramics, Artist, Balcony3F, Cold Storage, Pipe Room and Secret Altar.
Fortune-teller reset moves only already-shown Mario-item flags back to their
owned/not-yet-shown equivalents, exactly as Event77; it does not grant absent
items or create pickups. Secret Altar's recipe changes its boss flag without
inventing a PLIGHT operation absent from the script.

Current reset entry coverage is60 distinct mansion room IDs plus the four
boss maps. Twelve hall/unused segments have no recovered menu entry and are
reported as unavailable:15,18,26,29,31,32,54,58,64,65,68,71. This is not
advertised as all-room reset coverage.

## Clear-room additions

The thirteen formerly unsupported clear recipes are now present: Ballroom,
Graveyard, Rec Room, Nursery, Twins, Guest Room, Study, Observatory, Ceramics,
Artist, Balcony3F, Cold Storage and Secret Altar. They reproduce the actor
kills, flags and PLIGHT usage recovered from Event77 and finish through the
queued reload where the original recipe requires it. In particular:

- Guest Room sets18,38,36,25 and removes `demo_girl`.
- Artist removes `dm_gaka` and three `tony_montana` actors, then sets31,
  177..183 and55.
- Twins sets51 and removes `iyapoo`; the unreachable later gemini-kill block
  is not confused with the active script branch.
- Graveyard, Balcony3F and Secret Altar set67,81 and66 respectively, without
  adding a PLIGHT call missing from those branches.

The source recipe partition now covers all72 logical mansion IDs used by the
GaddWarp clear table. This is recipe coverage, not proof of72 hardware tests.

## Additional user-facing controls

- X in Room warps toggles the existing Boo-safe alternative entry points.
  It is a mod-local preference; it does not pretend clean flag22 implements
  GaddWarp's patched-event consumer. Destinations without an alternate retain
  their standard point.
- Game options contains one Poltergust element set/refill row backed by
  `LMElements`, avoiding a duplicate physical elemental-ghost selector. It
  requires the corresponding medal and does not spawn items or grant flags.
- Existing HP, music, door, mansion, Boo and plant options are retained.

Still not implemented as GaddWarp-equivalent behavior: patched Perfect RNG,
global event skipping, Boo-event instant reset, GBH scan hotspots, physical
medal/Mario-item pickups, custom Weirdboo Dojo, complete Boss/Portrait Rush
chains, and all-room bulk traversal. Their flags alone would be inert or alter
unrelated retail progress; they are not exposed as working toggles.

## Tests

`scripts/test_lm_room_tools.py` executes the pure room-ID and reset helpers,
tests all74 IDs, all32 shown-item combinations, all65536 persistence values,
the real74-row MEM1 fixture, authenticated retail instructions and queue/commit
source ordering. `test_lm_diag.py` retains native-menu primitive contracts and
checks the complete72-ID clear partition. Runtime validation should prioritize
blackout both directions, foyer reset on each floor, one normal and one
portrait room reset, each newly added complex clear, then door exit/re-entry.
