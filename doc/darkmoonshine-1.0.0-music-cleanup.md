# V1.0.0 packaging correction: no bundled music

The initial release mistakenly included personal launcher music at
`Darkmoonshine_Theme/bgm.mp3`. The corrected package omits that file.
Runtime support for optional user-supplied SD music remains unchanged.
Existing personal SD themes must not be deleted by this correction.

The packager now allowlists only `background.png`. A regression test places
personal music beside that image and verifies it is excluded; the release
workflow's exact-content verifier also rejects any bundled music.
The current Wii ZIP contains nine files: eight app files and the background.

The published ZIP is repacked without recompiling or replacing its launcher
or mod payload. V1.0.0 savestate build identity therefore remains unchanged.
This is a packaging-only correction; no new gameplay testing is required.

The affected release commit and its merge are replaced with music-free
history, with only `main`, `release/v1.0.0-frozen-in-time` and `V1.0.0`
updated. Consumers of the old history should use a fresh clone or carefully
realign their local branches; do not merge the old release commit back.

Removing reachable history and replacing release assets cannot retract
downloads already made, and GitHub may retain old commit/PR references or
cached source archives. GitHub Support may be needed for server-side cleanup.
