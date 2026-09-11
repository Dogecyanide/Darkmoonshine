# 0.3.39 experimental changes

This is a focused scene-cleanup candidate, **not a 1.0 release or a mansion-wide
compatibility claim**. It has **not yet been verified on Wii**. Install the
matching launcher and payload together and make fresh states: snapshot format
is now **24**, incompatible with previous formats.

## What the latest Wii report established

The fresh 0.3.38 crash report A (generation 8, build CRC `2E6F16CD`) reached
`PC 800286A4`, `LR 80028680`, `DAR AFEFF007`. The next warp passed the earlier
dialogue cleanup failure, then crashed in the native Mission picture cleanup
while destroying the **old scene**. This is another owner that was left out
of the snapshot, not evidence that Anteroom initialization failed or that a
floor/distance limit was reached. Report B was still from 0.3.37, not a second
fresh 0.3.38 failure.

## Targeted change

Format 24 captures the full fixed Mission picture-owner table and its view
origin alongside GAME. The ten picture owners must match the restored heap
when the next scene cleanup destroys them. The earlier dialogue-manager and
depth-owner fixes remain; this does not yet establish that all fixed owners
are covered or that every cross-room route works.

The wider audit also adds a small model-render context and player/effect-query
cache, including their borrowed GAME pointers and scalar state. These are
coherence improvements, not additional proven causes of the reported crash.
Live display and DVD-I/O owners and destructor-registration records remain
excluded. There are no menu, input or warp-mechanism changes.

The three ranges add **376 bytes** of static state. Alignment makes the total
core snapshot grow by **384 bytes**. The MEM2 reservation and resident slot
count are unchanged; no extra MEM1 snapshot buffers are allocated.

The six tests in `TESTING.md` keep the representative route unchanged:
**save in Parlor → menu-warp to Storage Room → load back to Parlor → menu-warp
to Anteroom**. The final warp is essential: seeing Luigi return to the saved
position is not enough. There are also fresh-boot, same-room, reverse-direction
and ordinary-door controls. One attempt per test is enough; stop at the first
crash.

## Unchanged limits

These checks cover ordinary mansion rooms, not separate boss arenas. There is
one resident state, and SD archives remain **same-session only**. They are not
portable across a soft reset, new boot or different build. Cross-boot SD
states, persistent in-game settings, Dojo, Boss Rush and Portrait Rush remain
deferred; no SD-feature tests are required for this candidate. Door-transition
refusals remain intentional. Older native Dolphin checkpoints restore their
old injected code and cannot verify this build.
