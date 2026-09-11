# Secret Altar: RC1 savestate boundary

## Observed .44 result

The user reports no crashes in the latest .44 testing. Both SD crash reports
are unchanged earlier reports: A is .43 at 19:50, B is .42 at 18:54. They are
not new .44 crashes.

Fresh journal B is generation 53, build CRC `B080F446`, with 1438 valid records,
zero invalid records and no trailing bytes. Journal H is generation 52 from the
same build, with 318 valid records. The exported Parlor archive is format 27,
generation 1, and passes the read-only decoder's integrity/layout checks.
No archive authentication keys were read; CRC verification is not authentication.

The Secret Altar menu warp itself succeeds:

| Journal B sequence | Meaning |
| --- | --- |
| 38120 | Request destination index 49, Secret Altar. |
| 38122–38124 | Accepted and dispatched: map 2, spawn point 55. |
| 38140–38144 | Appearance initialized and settling completed. |
| 38146 | Arrived, native room 70 (`0x46`). |
| 38174–38198 | Earlier Parlor savestate load refused with X08. |
| 38398–38422 | Repeat load refused for the same unmatched archive. |
| 43746–43770 | Later load refused for another unmatched event archive. |

No outgoing menu-warp request follows the Altar arrival in this journal. The
user confirmed the unsuccessful action was loading the earlier savestate, not
selecting a destination in Room Warps.

## Exact refusal

This is `EPOCH`, guard X08, mask `0x180`, ownership fault `0x13`
(`unmatched-volume`). Saved and live map/scene identifiers are all 2; this is
not a floor mismatch. The first two attempts find a changed live volume with
name hash `EED6AF11`, object `815816B4`, backing `815817C0`. The later attempt
finds hash `EAC55FA4`, object `815D69B0`, backing `815D6A40`.

`captureVolumeName` computes lowercase FNV-1a hashes. The first hash matches
`event06`, the second matches `event74`; both names exist as
`Event/event06.szp` and `Event/event74.szp` in the supplied Japanese disc's file
table. The saved Parlor census contains neither. This identifies event-resource
changes associated with this Altar sequence; the journal does not contain a
complete live event-owner object image and cannot prove those owners are safe
to discard or restore.

`modelReplacementMatches` accounts for changed model archives, the VR archive
and Mission's map archive, then requires every remaining changed volume to have
an ownership proof. These event archives have no accepted proof in this path,
so `everyChangedVolumeMatched` refuses before the destructive restore. Heap
grain validation of the saved image succeeded. Bypassing X08 would not establish
that event callbacks, cursors and resource lifetimes are coherent.

## RC1 policy and focused test

Keep this guard unchanged for RC1. Do not promise mansion-wide event/boss
restores based on successful ordinary-room loading.

One focused boundary case for runners:

1. Export a known-good named Parlor state, then menu-warp to Secret Altar.
2. Try loading the Parlor state once. Record whether it loads or is safely
   refused; `EPOCH` here is a known limitation, not an expected crash.
3. Once the game is idle and no cutscene is running, separately try Room Warps
   → Parlor/Foyer, then attempt the same state again. This recovery route needs
   testing; it is not yet verified by the supplied journal.
4. If an event prevents leaving, retain the exported archive and restart the
   game into ordinary gameplay before importing/loading it. Reboot loading has
   passed earlier user tests, but this exact Altar recovery sequence remains
   unverified. Preserve the entire `lm_dumps` folder if anything differs.

Avoid repeatedly forcing a load during an active event or treating a refusal
as evidence that the stored state was overwritten. No production change was
made for this triage.
