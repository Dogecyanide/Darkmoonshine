# 0.3.38 experimental changes

This is a focused post-restore scene-cleanup candidate, **not a 1.0 release or
a mansion-wide compatibility claim**. It has **not yet been verified on Wii**.
Install the matching launcher and payload together and make fresh states:
snapshot format is now **23**.

## What the latest Wii reports established

The 0.3.37 Storage-to-Parlor restore kept running for hundreds of updates. The
next menu warp from Parlor to Anteroom crashed while **destroying the old
scene**, before Anteroom initialization. Both fresh exception reports reached
`PC 8003C248`, `LR 8004339C`. This is not the earlier immediate post-load heap
failure, and the evidence does not point to running out of memory or exceeding
a floor/distance limit. These routes use the ordinary mansion map (map 2);
separate boss arenas are not established by this test.

## Targeted change

Capture the fixed dialogue/message-window manager alongside GAME. Its text,
choices, 25 picture wrappers, palette and scalars can retain pointers into the
newer scene if only GAME is rewound. The next scene cleanup then acts on
mismatched owners. Format 23 adds the audited range `803C3730` through
`803C4448` (exclusive): **3,352 bytes** of manager state. Existing padding
shrinks, so the total core snapshot grows by **3,328 bytes**. The MEM2
reservation is unchanged.

This is a candidate fix for the observed cleanup path, not proof that every
fixed graphics owner has been covered. The previous depth-owner capture,
bounded heap diagnostics and resource-padding checks remain. There are no new
menu, input or warp-mechanism changes in this release.

The six focused tests in `TESTING.md` include a fresh-boot warp without any
savestates, the exact restore-then-warp sequence, and a normal-door control.
One attempt per test is enough; stop at the first crash. A restored frame alone
does not count as a pass: the next scene transition matters too.

## Unchanged limits

There is one resident state. SD archives remain **same-session only**;
cross-boot SD states, persistent in-game settings, Dojo, Boss Rush and Portrait
Rush remain deferred. No SD-feature tests are required for this candidate.
Door-transition refusals remain intentional. Older native Dolphin checkpoints
also restore their old injected code; they do not test this build.
