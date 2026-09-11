# 0.3.30 verification record

Development results on 2026-09-06. This is not a Wii hardware certification.

## Inputs and provenance

The supplied `lm_attempt_a.bin` and `lm_attempt_b.bin` were copied intact to
the local evidence directory `crash-reports/lm-0.3.29-runner-2026-09-06` outside
the repository. They have 3,960 and 471 valid records respectively, with no
invalid or trailing records. They contain continued post-load gameplay but
no recorded King Boo refusal. Absence of a sampled fast transaction phase
does not mean the phase never ran.

The clean Japanese revision-0 main.dol is authenticated by SHA-1 and by each
patch/check word. The original ISO is unchanged. The existing supplied Hidden
Mansion GCI is copied into an isolated Dolphin profile; personal Dolphin
settings and saves are not the test profile.

## Observed gameplay

- Patched game boots in the installed Dolphin 5.0 and recognizes the GCI.
- Real XFB displays the CPU-drawn HUD and nine-page practice menu correctly.
- User-operated King Boo warp renders the boss arena successfully.
- The user reports in-room menu save/load works.
- A native screenshot records `S:LOADED`, `SZ12468K`, and healthy F/C/H checks
  while rendering Luigi in the foyer. It does **not** prove a King Boo-to-foyer
  restore: the user explicitly confirms that cross-map attempt did not work.
- The original strict D-pad equality check did not start the user's save/load
  requests, while menu actions did. The new edge handler ignores unrelated
  face/shoulder bits, rejects conflicting D-pad directions and checks controller
  connection status. Its hardware behavior still needs retesting.
- The first compressed-cache layout refused slot 2 with `CACHE FULL`; the
  existing selected state was retained. Do not count that as successful
  multi-slot support. Revised layout results must be recorded separately.
- A native Dolphin checkpoint was decoded offline: the raw snapshot is
  12,767,712 bytes; the original cache codec produces about 7.4 MiB. This is
  an actual capacity failure, not a misleading generic status.
- The colour guard rejected the live model because it required the GAME heap
  and on-disc offsets. The live regular model is in SYS, with a relocated
  table and different geometry offsets. Both clothing image CRCs match the
  clean ISO exactly. The revised guard follows that table, validates its
  ranges, and retains the original-image authentication before modifying it.
- The menu now has a nine-category home, highlighted rows, stick navigation
  and per-category cursor memory. Moonshine's graphical controller replaces
  the previous text-only input panel. The user reports the updated menu/WHITE
  check works perfectly. A later native screenshot shows purple clothing with
  intact skin, gloves and overalls and healthy F/C/H checks.
- The revised heapless deflate codec compresses the captured snapshot plus
  its 6,400-byte census trailer to 5,218,488 bytes. This fits the approximately
  5.5 MiB inactive cache. The resident menu therefore offers two slots, not
  three. Native tests cover segmented staging, full-stream prevalidation and
  rollback when a larger target fails validation; live slot-switching results
  must still be recorded separately.

Short synthetic key pulses and early DTM movies were not a reliable way to
control this installed Dolphin/adapter session. Their failure is not a game
test result. The verified gameplay above was driven by the user; computer-use
inspection and a native Dolphin screenshot supplied the visible evidence.

## Automated checks

At the final integrated-build checkpoint, all 271 tests passed. Coverage
includes authenticated hooks, exact recovered warp fields, native CMPR colour
transformation, native timing/lag/edge counters, BPS round trips, malformed
compressed streams, authentication vectors and fault-injected execution of the
actual ARM filesystem worker. Host tests are not proof of Wii cache timing,
filesystem runtime behavior, boss rehydration or arbitrary heap compression.

The console payload is 122,596 bytes including code, immutable data and BSS
(approximately 119.7 KiB, 23.4% of the 512 KiB reserved MEM1 code window).
The separate 320 KiB deflate workspace is in MEM2. Both console and Dolphin
builds link successfully; their manifests identify their distinct runtime
memory maps. The packaged console archive includes the miniz MIT notice.

## Unproven or deliberately restricted

- Truly map-wide restoration, especially mansion/boss boundaries.
- Automatic pearl-dupe/Chauncey success detection. Current tools compare a
  configurable timing reference, not the game's success condition.
- SD archives across reboots/builds/consoles: deliberately refused.
- Runtime SD transfers on Wii and reliable multiple-slot capacity across rooms.
- Colour variants used only in cinematics: not modified.
