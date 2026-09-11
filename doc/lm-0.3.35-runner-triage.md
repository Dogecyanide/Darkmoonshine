# 0.3.35 runner report and file triage

2026-09-07. Diagnosis and prioritization only: no runtime fix, new build,
archive import, emulator gameplay test, or SD deployment in this intake.

## Evidence and provenance

The user supplied [LM Tests](https://docs.google.com/document/d/1wDFuOtGmIgpMt33Zpxw6D7dn6rGFLh7odiQ6yfXRsY4/edit?tab=t.0),
two attempt journals, both binary/text crash reports, and two playable SD
archives. The document was read through its existing browser tab without
editing it. An unchanged private copy of the files and exported report is
under the workspace's `sd-captures/lm-0.3.35-runner-20260907` directory.
Copies were SHA256-checked against the supplied originals.

Tester: normal Wii, stock Smash Ultimate GameCube controller, No OoB file
before King Boo. These results do not independently validate the user's Phob.

Both crash reports identify 0.3.35 (`8B437156`, 136276-byte mod, 58 writes).
Both completed restore phase `7F` before a native drawing fault. Heap checks
passed; neither report demonstrates an out-of-memory failure.

The journals contain 310 and 4410 structurally valid records, no torn records
or trailing bytes, at journal generations 46/47. They contain two and six
observed load preflight/copy sequences respectively; no retained epoch or
refusal diagnostic. Journals sample progress, not every phase. They cannot
be assigned to the reported crashing routes just because the build matches.
Crash-report, journal and snapshot generation numbers are different counters.

Both archives are 12,777,824 bytes, format 19, same build and session. Their
raw snapshots are 12,771,360 bytes (12,472 KiB), snapshot generations 9/11.
Outer payload CRC and inner snapshot CRC both pass. Both complete grain
graphs pass offline inspection. These are useful intact fixtures, not proven
crash snapshots. CRC integrity does not grant session authentication or make
the captured pointers portable across boot; no such bypass was attempted.

## The important route distinction

| Operation | Runner result | Interpretation |
| --- | --- | --- |
| Parlor save, normal door to Anteroom, load | Three passes | Ordinary streaming route works in these trials |
| Storage / Astral Hall, normal travel, load both ways | Passes | Cross-floor loading is not universally broken |
| Foyer to Observatory, load Foyer, then Sue Pea / Boos | Three more minutes without failure | Useful post-load interaction coverage |
| Save, menu-warp to same or another room, load | First refusal | Scene recreation remains a distinct state boundary |
| Save AGAIN in the new scene, warp and load | Crashes | Not equivalent to retrying the first refused state |
| Load/save while a door transition is active | Refuses, later stable load works | Preserve those safety gates |

Do not describe these results as "all cross-floor states work" or as a fixed
three/four-room distance limit. Scene recreation and its object ownership
are the stronger reproducible boundary in this report.

## Crash corrections now supported by native evidence

### B: element-meter ownership

PC `8003BBB0`, DAR `3E`: the element-gauge background wrapper `803C4850`
points to picture `81364738`, whose texture pointer is null. This is a
different fixed owner table from the GBH table added in 0.3.34. Its native
creation, cleanup and draw lifetimes establish an omitted snapshot range.

A sibling audit also identifies Goodnight, native timer and Boo-radar picture
owners. Three precisely bounded ranges add **840 bytes**. Rewinding those
owners with their GAME objects is the proposed correction; skipping the null
draw would leave later destruction through the stale pointer unresolved.
See [HUD audit](lm-0.3.35-crash-b-hud-audit.md). Seven new tests authenticate
the relevant retail instructions and boundaries; they are not runtime proof.

### A: particle link and incomplete validation

PC `80129DA8`, DAR `23`: after submitting a particle, the first grain manager
follows a link to `FFFFFFFF`. Current validation covers only the second
manager's controller sentinels, not either complete particle graph. Its
successful validation record therefore did not cover the failing structure.

A fourth lazy model-effect manager is also definitely omitted: a separate
**684-byte** owner including its initialized flag. It updates models before
grain drawing and is a plausible stale writer, but this report does not
prove that it wrote the corrupt link. Capture that owner and validate both
complete graphs; keep the causality uncertainty explicit. See
[grain audit](lm-0.3.35-crash-a-grain-analysis.md).

Combined proposed owner additions total 1,524 bytes before layout alignment,
not megabytes or a larger MEM2 reservation. Integration requires a snapshot
format bump, fresh test states and tests for the new offsets and checks.
Neither proposal has been installed in this intake.

## Other actionable findings

- **SD feedback:** tests 44/45 round-trip successfully, but missing IDs and
  post-reboot imports reportedly say success while preserving the old state.
  Treat this as a real user-visible failure. The current import service keeps
  its failure result through rollback; a specific root cause has not yet been
  demonstrated. Trace confirmation, request/ack and displayed result together;
  include archive ID and operation sequence in new diagnostics. Do not remove
  authentication to make old-session imports appear successful.
- **Input display:** D-pad geometry is absent. Holding Z explicitly skips
  `LMTools::draw` in `lm_diag.cpp`; this explains the disappearing display.
  Separate memory-panel hiding from input/pump/metadata display visibility.
- **FIRE:** the tank setter checks story flags 43/44/45, matching extracted
  GaddWarp event27. The runner owns medals but FIRE refuses. Verify native
  inventory ownership and any reset-induced story-bit changes rather than
  swapping IDs or granting medals speculatively. WATER/ICE/NONE passed.
- **Encounter reset:** Parlor and Astral Hall candles do not launch their
  encounters; upper/lower Foyer can desynchronize; Nursery boss entry fails;
  restarted Bowser stands idle. Room arrival and lights-off alone are not a
  completed reset. These are prerequisites for dojo/rush encounter work.
- **Lag/metadata:** Armory did not increment the reported lag counters.
  This does not establish that Nintendont has no lag. Check sampled counters
  and timing definition. Clarify which metadata fields should change and
  provide actual speed units rather than treating map/room/HP as counters.
- **Positive regressions to retain:** both resident slots, colour presets
  and custom RGB, R-only popup behavior on the stock controller, blackout
  toggling, safe door refusal, and normal navigation all received passes.
- **Checklist clarity:** explain clear recipes, Boo-safe entry differences
  and timing-reference recording with concrete examples. The current timing
  tool is not an authenticated pearl-dupe/Chauncey success detector.

## Prioritized follow-through

1. Integrate and test the bounded ownership corrections and full grain graph
   validation without weakening I/O gates. Reproduce the exact save-again
   post-menu-warp sequence, followed by movement, a normal door and another
   load; retain the passing natural cross-floor routes as regressions.
2. Correct SD operation feedback and input-display behavior; investigate FIRE
   ownership and special encounter resets with native evidence.
3. Implement confirmed dojo/rush demand after functional encounter startup,
   retry and completion. Follow the [reconstruction plan](lm-dojo-rush-reconstruction.md):
   Boss Rush first, dojo wave controller, then Portrait Rush route variants.
4. Keep reusable cross-reboot SD states and persistent settings as explicit
   work, not capabilities inferred from same-session export/import passes.

The next shipped build still needs a ZIP and a longer versioned checklist.
For the next crashing sequence, collect files before further saves rotate the
two attempt journals. Do not overwrite or discard the current evidence.
