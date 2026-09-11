# Clean-JP encounter replay prerequisites

The runner's candle/reset and idle-Bowser reports exposed a missing layer:
room lighting and story flags are not sufficient to reset an encounter.

## Native event play counts

Clean GLMJ01 DOL SHA1: `722005ea9c1eab54b114f814734d8f327e5614ee`.

`EventInfo.EventLoad` is the allowed lifetime play count, not an archive-load
boolean. `8002D61C` reads it into descriptor+24. `8002B148` copies it into
record+4C and, when nonzero, disables the event if its persistent count is at
least that limit (`8002B1F0..21C`). `8002BC34` increments the counter after
completion (`8002BD14..24`). Counters are bytes at `803C2E30 + eventID`.
The separately tracked active-event bytes at `803C20C8` are not these counters.

| Action | Scoped counters to clear | Additional reset predicate |
| --- | --- | --- |
| Parlor reset | 61 | Existing flag14/8 reset retained |
| Astral Hall reset | 76 | Clear map-transient198; retain83/6 reset |
| Nursery reset | 22, 64 | Clear89 (event22 skip predicate), retain46/47 |
| Chauncey warp/restart | 64 | Existing defeat/key/222 flags retained |
| Bogmire warp/restart | 66 | Existing defeat/key/222 flags retained |
| Boolossus warp/restart | 72 | Existing defeat/key/222 flags retained |
| King Boo warp/restart | 75 | Existing defeat/222 flags retained |

Nursery event50 is repeatable (EventLoad=0) and performs the native WARP10;
rearming its destination intro64 is necessary as well as Nursery intro22.
The four boss intro records all have EventLoad=1, EventFlag=0 and no disappear
flag. Adding arbitrary progress-flag clears would not fix their exhausted
counters. This change replays the retail intro, not a cutscene-skip mode.

Room recipes remain read-only during prepare; counters are committed only once
the native scene request was accepted, before its same-thread consumer runs.
Boss play-count changes are part of the publication transaction: a refused
queue restores the old counter, every touched flag and the scene globals.
No global event-counter reset or direct interpreter manipulation is used.

## Linked foyer

The native global at `80398C8C` identifies the companion **table row**. Resolve
that row's packed record+10 low byte for its logical persistence ID. In the
captured retail mansion it is row31/logical30 (upper Foyer Hall), paired with
logical2 (lower Foyer). Do not substitute row31 for logical30.

Retail `80018748(entry*)` marks the supplied row lit and persists it. If its
logical ID is2, it also follows that companion mapping. It does not link in
reverse. Practice commands deliberately treat the pair symmetrically:
validate the companion, darken both persistence slots before reset/reload;
for clear, invoke that native entry setter with authenticated bottom-room2.
Other rooms retain the ordinary current-room setter.

## What remains unverified

These changes have native-DOL/data authentication and host regression tests,
not a claim of Wii encounter completion. Test every intro, candle chain,
boss activation, death/restart and full victory twice. Confirm unchanged
unrelated rooms, Boo placement, keys and pickups. Dojo/rush chaining must use
these encounter resets; a sequence of warps alone is not a faithful mode.

`scripts/audit_lm_encounters.py` reads local extracted JMap data and obtains
field names from the local clean DOL. Its hash explicitly wraps to 32 bits
before modulo; the older local parse helper lacked that step for long names.
No retail asset is embedded in this patch or required in the distributed mod.
