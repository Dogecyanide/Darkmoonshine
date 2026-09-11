# LM critical telemetry and targeted crash context

## Evidence from the 0.3.31 Wii run

The copied `sd-captures/20260906-215352` generation-15 journal starts at
SAVE/7F sequence 60. Later it contains SAVE/63,64,65 at sequences
2272,2274,2276, immediately followed by POST_LOAD/E6 at 2284. The intervening
SAVE/70 and SAVE/7F publications are 2278 and 2280; post-load tracing then
replaces the shared latest-value record. The same pattern repeats at
9050,9052,9054 followed by 9062.

The matching crash report, mod CRC `A9BB4CB0`, independently contains the later
successful SAVE/7F breadcrumb and state-save event. The missing journal anchor
therefore does not mean that save failed. The ARM samples one 32-byte mailbox
about 120 times per second, only while asynchronous DI is idle, and syncs each
observed record to SD. Brief final phases can disappear before that sample.
The old sixteen-entry crash breadcrumb ring also contains no warp events.

## Bounded critical lane

The existing latest-value mailbox remains at crash reservation offset `0x800`.
Successful SAVE/7F and all WARP/action-4 phases additionally enter a 32-record
queue. Since 0.3.33, grain validation SAVE/5F and LOAD/07,6B use the same queue
to preserve the fault address and value through first-draw crashes. No snapshot
or gameplay state changes are made by this telemetry.

| Crash-reservation offset | Size | Runtime owner |
| --- | --- | --- |
| `0x000..0x800` | 2048 | Existing crash report |
| `0x800..0x820` | 32 | PPC latest-value phase |
| `0x820..0x840` | 32 | PPC producer/overflow line |
| `0x840..0x860` | 32 | ARM consumer/capability line |
| `0x860..0xC60` | 1024 | 32 individually published records |
| `0xC60..0x1000` | 928 | Unused |

ARM initialization publishes capability; the new PPC explicitly enrolls.
New PPC with an old kernel and new kernel with an old PPC retain the legacy
sampled behavior. Ordinary critical enqueue writes the entry and publishes it
before advancing the producer cursor. ARM cannot acknowledge it until after
the record has been handled. The producer never reuses an unacknowledged slot.
Producer and consumer cursors use independent cache lines and inverse fields.

At most four queued records are drained per ARM poll. While a backlog remains,
sampled phases cannot jump ahead of it. Once enrolled, sampled critical phases
are suppressed; a separate last-critical sequence check also prevents the same
save from rotating a journal twice. High-frequency post-load phases remain
sampled and do not fill the critical queue.

Full queues do not stall the game or overwrite older records. They increment a
saturating `dropped` counter, printed in `ndebug.log` when ARM next observes it.
This is bounded buffering, not a guarantee against power loss, SD write failure
or overflow. With matching new PPC/kernel code, valid controls, available queue
space and successful SD I/O, immediate post-load tracing no longer silently
replaces a successful-save anchor. Native tests reproduce the 2280 save followed
by 1,000 later noncritical publications and verify the original anchor survives.

The `.bin` journal layout and version remain unchanged: its first record must
still be a successful save, and each later successful save rotates to the next
bank. Warps before the first save of a process are printed to `ndebug.log`, not
used to create an invalid save-anchored journal. Warp lifecycle breadcrumbs are
also recorded by the warp module; capture still retains only the last sixteen
breadcrumb events. These attempt files contain diagnostics, not full savestates.

Action 4 names: request `01`, accepted `02`, dispatch `10`, appearance `20`,
prepared `21`, settling `30`, arrived `7F`, rejected `D0`. The reader retains the
exact two argument words and labels the lifecycle phase.

## Targeted effect crash capture

At the proven GLMJ01 failure `PC=801717E0`, `r26` is the effect owner and the
failed chain is owner `+8` to the first model slot, slot `+4` to a null J3DModel,
then the attempted read at model `+C`. The private effect heap is reached from
the fixed manager root at `803CE4A8`; its count is at `804A19B0`.

The report remains exactly `0x800` bytes/version 1. An LM-only `LMEX` tag occupies
268 previously unused bytes after the DVD window's existing 40-byte prefix:

- 32-byte versioned header: addresses, independent valid flags and manager count;
- effect owner: 40 bytes;
- first model slot: 36 bytes;
- private effect solid-heap header: 128 bytes;
- effect configuration: 32 bytes.

The DVD prefix, its recorded size, ARAM window and every outer field offset are
unchanged. Old readers ignore the extra tagged bytes. New kernel text reports
print each valid chunk under its own address; no noncontiguous data is labelled
as an extension of the DVD memory range.

Capture interprets r26 only at the exact known PC. Every source extent must be
aligned and wholly inside MEM1 before reading. Child pointers come from the
already captured owner, and a null J3DModel is recorded but never followed.
Unreadable owner/slot/config/heap chunks have clear validity bits and zero data.
This distinguishes a missing model from damaged owner/heap context without
performing allocation, recovery, filesystem work or a whole-RAM dump in the
exception handler.

## Verification

In 0.3.37, POST_LOAD phases `F3` (render-target proof) and `F4` (bounded
heap validation) are critical records. Their arguments are the failing
field address and observed value. `read_lm_dump.py` labels them explicitly.
Heap breadcrumb subcodes `F0/F1/F2` additionally preserve address/value/heap.
These are not the existing POST_LOAD audio `F0/F1` checkpoints; those remain
noncritical. A native queue regression checks that distinction so ordinary
audio calls cannot flood the critical fault queue.

`test_lm_crash_telemetry.py` uses compiled native helpers with separate simulated
PPC/ARM caches and backing memory. Tests cover publication order/cache ownership,
old-build fallback, full queues, repeated reuse, counter wrap, torn controls and
records, invalid/noncritical actions, saturated overflow, anchor preservation,
the exact-PC gate, null/end-crossing/MMIO pointers and capture canaries.
`test_lm_dump_journal.py` covers action-4 decoding, unchanged save-only anchors,
drain ordering, duplicate-rotation guards and all three grain-check labels.
The full project suite has 327 passing checks at 0.3.33.

The 0.3.32 Wii log retained all observed warp records and the successful-save
anchor, including warp events before the first save. The new grain records
and richer targeted exception context still require hardware validation;
native tests do not establish SD timing or reproduce the game crash itself.
