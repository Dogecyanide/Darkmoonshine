# RC1 runner triage — 2026-09-10

## Outcome and priorities

The runner reports no crashes throughout the RC1 suite. The submitted
`lm backups.zip` contains eight attempt journals and two exported states/name
sidecars, but no crash reports. All 8602 journal records are structurally valid,
with no torn records or trailing bytes. Both format-27 states pass the offline
integrity/layout/allocation checks. This supports the reported stability; it is
not proof that every unrecorded game subsystem or future route is safe.

The critical issue is timer correctness, not a crash: test 21 reports that the
timer stops while the mod menu is open although gameplay continues. A menu must
not change elapsed-time accounting unless the game actually stops advancing.
Root is handling the clock policy separately. The runner also reports overlay
flicker in native pause/Z menus and a flickering bottom edge during gameplay;
those display symptoms are not established by these binary journals.

## Evidence provenance

Private capture: `../sd-captures/lm-1.0.0-rc1-runner-20260910/`.
The original ZIP is preserved and SHA-256 verified:
`7861493763abafeb8971b3c395f4270a73abf16ca53584b13a4f121626d816f7`.

ZIP paths, duplicate names, types and bounded expanded sizes were checked before
extraction. Only diagnostic/state/name data was extracted. The two private key
entries remain unopened in the opaque original; no keys were extracted,
displayed or used for authentication. Each extracted entry's SHA-256 matches
its ZIP-entry stream. No attached content was executed, and no SD was modified.

All journals and both states report build CRC `68AD7DDA` (RC1):

| Journal | Generation | Valid records |
| --- | ---: | ---: |
| H | 24 | 581 |
| A | 25 | 246 |
| B | 26 | 2454 |
| C | 27 | 887 |
| D | 28 | 534 |
| E | 29 | 1851 |
| F | 30 | 710 |
| G | 31 | 1339 |

The retained journals contain 16 successful restored-copy grain-validation
records and no recorded grain/render-target/heap failure telemetry. They are
bounded, sampled journals rather than a complete frame-by-frame execution log;
absence of an exception file alone cannot certify there was no crash.

Archive 4 is format 27, generation 9, map/scene 2/2, SHA-256
`9bca6fc73bbb970fbea337a89843bdea925f76ffdded2f57fa8e688e05f0680c`.
Archive 5 is format 27, generation 10, map/scene 2/2, SHA-256
`25a95cd0e794ef0c27051b112149f4bd570c99ea9d266915db9fdef0cfd25e23`.
CRC checks were performed; SipHash authentication was intentionally not attempted.

## Known event-resource refusal remains bounded

Latest journal G contains seven X08 refusals, each for an unmatched live volume
`event06` (name hash `EED6AF11`, object `815816B4`, backing `815817C0`). The
refusal sequences are 41398, 41420, 41642, 41972, 43062, 60080 and 76478.
These are the same unsupported event-resource ownership class previously
observed around Secret Altar, not the fixed FurnitureInfo door crash.

Importantly, the same journal proves the escape route works in this run:
Secret Altar menu warp arrives at sequence 29998 (destination 49, room 70),
and a later menu warp to Artist's Studio arrives at 76686 (destination 48,
room 57), after the refusals. This does not prove loading an older state from
every boss/event phase is supported. Preserve X08; do not force an unproven
archive restore. See [the earlier Altar analysis](lm-secret-altar-rc1-limit.md).

Journal C also records door safety refusals: gate `0x27` is Door, with one
`0x20` Stability wait. This agrees with test 20's report that attempts during
the transition preserved the original savestate.

## Element preset feedback correction

Test 18 reports `WAIT / RELEASE SUCTION AND SPRAY` in Foyer and Sitting Room,
then successful filling after entering a hallway. That old message did not
establish that a trigger was held: the implementation returned the same Busy
result for an active warp, readiness failure, native event cursor, nonpositive
health, or a non-idle Poltergust action. No element-specific refusal telemetry
was stored, so these archives cannot isolate the original branch.

The feedback-only change splits those reasons without changing predicate order,
removing guards, cancelling native actions or adding game writes:

| Existing guard | New message |
| --- | --- |
| Active mod warp | WAIT FOR ROOM TRANSITION |
| Readiness/streaming gate | WAIT FOR GAME / STREAMING |
| Native event text cursor nonzero | WAIT FOR EVENT / CUTSCENE |
| Player health not positive | WAIT FOR LUIGI TO RECOVER |
| Native Poltergust action nonzero | WAIT: SUCTION / SPRAY ACTIVE |

Only a validated idle player receives the existing two writes: tank fuel and
tank type. The existing numeric Applied/Busy/Invalid values (0/1/3) are
preserved; new reasons are appended. All seven element tests pass, including
native pickup evidence, no-partial-write planning, unchanged two-write scope,
and distinct reason routing. This corrects misleading feedback; it does not
claim the Foyer/Sitting Room restriction itself has been reproduced or removed.

## Focused next verification

Prioritize the menu-timer abuse reproduction, pause/Z-menu and bottom-edge
flicker, and a repeat element preset attempt in Foyer/Sitting Room noting the
new exact wait reason. Keep the successful reboot/multi-warp/door routes as
short sanity checks, without treating the known Altar refusal as a regression.
