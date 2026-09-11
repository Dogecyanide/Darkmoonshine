# 0.3.37 experimental changes

This is a focused post-warp savestate candidate, **not a 1.0 release or a
mansion-wide compatibility claim**. It has compiled and passed targeted host
checks, but has **not yet been verified on Wii**. Install the matching launcher
and payload together, and make fresh states: snapshot format is now **22**.

In 0.3.36, Wii D-pad save/load and same-room loads worked. Parlor → menu-warp
Anteroom → load was refused with EPOCH; the Storage route restored but crashed
afterward. The Storage failure was a post-load fault, not a demonstrated
memory-capacity failure.

## Targeted changes

- Capture the fixed depth-copy render owner, including its embedded GX texture
  object, alongside the GAME snapshot. Previously, its destination pointer
  could still belong to the newer scene after GAME was rewound. The audited
  owner occupies `803C4B6C` through `803C4C80` (exclusive). Alignment increases
  the core snapshot by **288 bytes**; the MEM2 reservation is unchanged.
- Idle room-resource records now ignore three native uninitialized padding
  bytes: `+3`, `+3E`, and `+3F`. This targets an EPOCH false-refusal after their
  allocation moves. Inactive IDs, idle state, auxiliary pointers, and every
  semantic field remain checked. The current logs do not contain complete
  endpoint records, so another mismatch may still require diagnosis.
- Heap diagnostics use bounded checks and retain the first fault instead of
  calling the retail heap walker on potentially damaged links. Heap evidence
  uses phase **F4**; render-owner evidence uses **F3**. Extra early checks cover
  the first eight post-load frames, alongside the existing 60-sample cadence.
  These improve fault evidence; they do not establish that every corruption
  source is fixed.

There are no new input changes. This release's six focused Wii tests are in
`TESTING.md`: first the same-room baseline, then same-room menu reload,
Parlor/Anteroom, Parlor/Storage in both directions, and a state made after a warp.
A restored first frame alone is not a pass: movement and a normal door must
also work afterward.

## Unchanged limits

There is one resident state. SD archives remain **same-session only**; older
builds, soft resets and new boots are not portable-state workflows. Cross-boot
SD states, persistent in-game settings, Dojo, Boss Rush and Portrait Rush remain
deferred. No SD-feature tests are required for this candidate. Door-transition
refusals remain intentional. Do not assume older native Dolphin checkpoints
contain this build: they restore the old injected code too.
