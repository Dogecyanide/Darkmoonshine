# GLMJ01 MEM1 diagnostic

This payload is the first game-side test for the Japanese Luigi's Mansion
revision (`GLMJ01`, revision 0). It reserves the Moonshine 512 KiB MEM1 window
at `0x804B8400-0x80538400` and renders a heap report directly into LM's copied
640x480 YUYV framebuffer with the retail `JUTDirectPrint` bitmap renderer. It
does not depend on a resource font, heap allocation, projection, or scene GX
state. The panel and raw checkerboard are inset from the top to survive normal
capture overscan; the checkerboard remains visible even if the text renderer is
unavailable. The launcher authenticates the clean DOL layout and
every hook word before it copies or patches anything; another revision runs
unmodified.

The overlay rows are:

```text
LM STATE X0.3.18 F:<floor> C:<canary> H:<heap check> X<cross-room guard>
S:<state status> ST<stable frames> SZ<snapshot KiB> G:<gate> <gate value>
E:<first epoch field> M<mismatch mask> <saved value>><live value>
V:<topology> S<saved count>>L<live count> -<removed> +<added> F<save>/<live fault>
V-<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
V-<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
V-<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
V+<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
V+<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
V+<archive name> <object>/<backing owner> O<object> R<RARC> <size bytes>
VR O<object reuse mask> R<RARC reuse mask>
VC <saved current volume>><live current volume> D<saved dir>><live dir>
RM F<save/live fault> A<active mask> R<record mask> L<layout> G<map> K<backing> M<marks>
RA <slot>:<saved active ID>><live active ID> <slot>:<saved ID>><live ID>
RW <saved/live wanted count> -<removed> +<added> Q<sequence change> <first removed>><first added>
MM F<save/live fault> N<changed indices> P<saved/live primary hash> R<saved/live registry hash>
M<index><P/R/B> <name> S<saved/live state> H<saved/live handle> R<saved/live parsed root>
ROOT <root> <start>-<end>
SYS  <system> L/T/M <largest>/<total>/<minimum total KiB>
GAME <game>   L/T/M <largest>/<total>/<minimum total KiB>
CUR <current> G<group> A <raw low>><raised low> H<initial high>
```

`WAIT` means that the relevant heap has not existed long enough to test.
`OK` means the condition has been observed and remains valid. `BAD` is latched
after a real floor, canary, or `JKRExpHeap::check` failure; a normal room-load
gap does not turn the heap check bad.

The final `X` byte is the guarded cross-room decision breadcrumb. It remains
`X00` until a mismatched load reaches that path and becomes `XA0` when every
guard accepts it. `X01` through `X08` identify the check that refused the load:

```text
X01 epoch mask       X02 census generation  X03 volume census
X04 list topology    X05 archive ownership  X06 room streamer
X07 model census     X08 model shape         XA0 accepted
```

Diagnostic packages also force Nintendont's `/ndebug.log` on the game-source
device (the SD card for the current `path_jp=sd:` setup). It records payload
validation, the observed DOL tuple, every preflight word, and successful hook
installation, giving an independent answer if the capture contains no
checkerboard. Full-state builds also append cache-coherent `Susamune: phase`
records while saving, loading, and traversing the restored-frame trace window.
The ARM writes and syncs these independently, so the last record survives a
PowerPC hard lock that never reaches the exception dumper.

Version `0.3.9` additionally captures GLMJ01's standalone `0x270`-byte
camera/viewport state block at `0x80398770-0x803989E0`. The normal-room draw
path reads its projection, viewport, scissor, and matrix fields directly. The
following display object is deliberately excluded because it owns live
double-buffer pointers.

For `0.3.10`, `80` means the load returned at the true post-presenter boundary.
The immediate main-loop tail is bracketed by `90/91`, `92/93`, and `94/95`;
the next update uses `88/89`, `8A/8B`, optionally `8C/8D`, and `8E/8F`.
`81` through `86` bracket the following complete presenter, diagnostic copy,
and tick. This repeats for eight restored presentations and retains the loop
tail/update that follows the eighth. For loop markers, `arg0` is the number of
restored presentations already completed; for `81` through `86`, it is the
one-based presentation ordinal (`80` uses zero). The ARM logger may miss fast
intermediate values, but an entry value remains the final record when its
corresponding retail call hard-locks.

Inside `8A/8B`, `A0/A1` bracket the first position-matrix upload, `A2/A3`
bracket the final normal-matrix upload, `A4/A5` bracket the active scene's
draw callback, and `A6/A7` bracket the final orthographic-view reset. A last
`A1` therefore means one of the intervening matrix uploads stalled; a last
`A4` identifies the scene draw callback itself. In `0.3.9`, `A4/A5` record the
main draw state in `arg0` and callback address in `arg1`. `B0/B1` bracket each
direct call in the Main Game draw dispatcher, while `C0/C1` bracket every
direct call in its normal-room renderer. For those records, `arg0` is the
retail call site and `arg1` is its original callee; a final `B0` or `C0`
therefore identifies the exact call that did not return.

Version `0.3.10` adds `D0/D1` around every direct call inside the central
per-view routine at `0x8000BA64`, which `0.3.9` isolated. These records use the
same `arg0` call-site and `arg1` callee convention.

Version `0.3.11` captures the adjacent grain-effect managers at
`0x803CBAF0-0x803CC460`. Their circular-list sentinels are static while their
nodes live in the gameplay heap, so both sides of each list now rewind as one
timeline.

Version `0.3.12` leaves all restore gates intact and diagnoses a preflight
`EPOCH` refusal precisely. The `E:` tag is the highest-priority differing
field; `M` is a complete 22-bit mismatch mask; and the last two words are that
field's saved and live values. The same record is written to `/ndebug.log` as
`Susamune: epoch ...`, so it survives even when the overlay cannot be read.
Mask bits from low to high are:

```text
00 MAPV  01 SCNV  02 SCNP  03 PEND  04 LOOP  05 AUDS  06 MARC
07 VOLN  08 VOLH  09 VOLT  10 MISS  11 GMOD  12 SIMP  13 MCOL
14 ENTY  15 HEAP  16 HBEG  17 HEND  18 ROOT  19 SYSP  20 AUDO
21 DRAW
```

`VOLH`, `VOLT`, and `VOLN` are the mounted-volume list head, tail, and count.
Matching list endpoints are only coarse sentinels: they do not prove that
interior resource or allocator nodes are unchanged. A future relaxation needs
a full member census rather than relying on this row alone.

Version `0.3.13` adds that read-only census without relaxing the gate. It
validates at most 32 complete `JKRFileLoader` links, rereads the list header to
reject a concurrent change, copies each volume name into mod-owned memory, and
records the GLMJ `JKRMemArchive` object and RARC backing metadata. `HEAD1`,
`HEAD2`, or `HEADN` means the complete live list is exactly the saved list with
that many leading members absent; `ORDER` means surviving members retain their
relative order but the change is not a pure head removal; `MIX` means even the
common order changed. `SBAD` or `LBAD` means the bounded traversal refused an
invalid saved or live list. Owner letters are `G`ame, `S`ystem, `R`oot, or `?`.
`O` is the archive object's recorded allocator (falling back to its address
range); `B` is the heap range that actually contains the validated RARC bytes.
The `Fsave/live` values are zero for valid censuses. Nonzero faults are:

```text
1 capacity  2 empty/header  3 endpoint  4 node  5 parent list  6 object
7 embedded link  8 previous link  9 duplicate  10 tail  11 end  12 changed
```

Version `0.3.14` reserved separate rows for the first three removed and first
three added archives, adds each archive object's RARC header and size, and
reports object/RARC allocation reuse. It also captures a generation-keyed,
read-only census of LM's seven-slot streamed room-archive manager. `A` and `R`
are seven-bit slot mismatch masks; `L` and `G` report manager-layout and room
map changes; `K` flags fixed backing-pointer invariant failures; and `M` shows
transient reconcile marks. `RA` identifies the first two changed active slots,
while `RW` compares the bounded wanted-room set and flags any exact ordered
sequence change with `Q`. This room manager is separate from the model-archive owners named by the
`V` rows. Resource faults are:

```text
1 slot count  2 wanted capacity  3 record range  4 bulk range  5 slot size
```

The same version also took a generation-keyed, read-only census of the two
262-entry model-resource tables at `0x803435AC-0x80346AE4` and
`0x8037EC70-0x80382DF0`. Those tables own the lifecycle state behind model
archives such as `tenjyo`, `bat`, `rat`, and `door`; they are distinct from the
seven-slot `RM` room streamer. `MM` gives the saved/live census faults, the
number of model indices whose complete entries changed, and whole-table hashes.
The next four rows identify the first changes by model name. `P`, `R`, and `B`
mean the primary descriptor, secondary registry, or both changed. On `P`/`B`
rows, state values are `0` unloaded, `1` load pending, `2` cancel pending, and
`3` loaded/parsed. `H` is the low 25 bits of its archive/load handle and `R` is
the low 25 bits of its parsed model root; that preserves every variable bit of
a MEM1 address. Names are capped at eight characters. A registry-only row instead shows
its full archive-pointer pair followed by complete entry hashes. Model fault `1` means a table
changed while the bounded copy was being verified. No model table is restored
and no epoch gate was relaxed in that diagnostic build.

Version `0.3.18` keeps the aggressive cross-room raw-rewind attempt from
`0.3.17`. It adds one-pass tracing around every direct call in `MAIN GAME`'s
update routine. Both initial `0.3.17` cross-room tests completed the restore
and then hard-locked inside that routine before the next framebuffer was
presented; the final `E0`/`E1` journal record now gives the precise callsite
and retail target responsible. The snapshot includes the fixed state identified
by the `0.3.14` captures:

```text
803435AC-80346AE4  primary model descriptors
8037EC70-80382DF0  secondary model registry
803C86A0-803C97C4  primary model output arrays
803E3088-803E3CF8  secondary model output arrays
80398C50-80398FC8  room map, slots, backing pointers, marks, and wanted IDs
80494754-80494760  mounted-volume list header
804A2038-804A203C  current mounted volume
804A2040-804A2044  current directory ID
```

Together with the earlier ranges, the static payload is `0x12D78` bytes and
the aligned game-heap payload begins at snapshot offset `0x12EC0`. Snapshot
format version 7 prevents an older MEM2 slot from being mistaken for this
layout.

Same-room loads still require an exact epoch match. The experimental
cross-room exception requires the epoch mismatch mask to be exactly
`M00000180`: only mounted-volume count and head may differ; the list tail and
all other identity fields must remain exact. It then requires a valid
generation-matched census, an exact ordered common-list subsequence with no
more than four total removals/additions at arbitrary list positions,
captured game-heap ranges for every changed archive object and RARC backing,
stable current-volume/directory values, valid room-manager layout and backing
pointers with no queued resource request, and at most four model changes with
no load/cancel-pending state. A failed condition keeps
the existing clean `EPOCH` refusal.

For an accepted load, the implementation rewinds the gameplay heap and the
fixed tables as one raw snapshot. It then reconstructs every saved mounted
volume node's object, parent-list, previous, and next fields from the protected
save-time census and stores the repaired links before resuming. It deliberately
does not call archive unload/load or room-resource reconcile functions. Both
the GX vertex cache and texture cache are invalidated after the restored bytes
are made coherent.

If the inner game loop exits during that window, `96/97` identify loop
entry/return, `98/99` bracket outer cleanup, and `9A/9B` bracket its restart.
The invocation containing the load can only emit `97` because tracing was not
armed at its entry. A final `97` isolates the following scene-table virtual
call.

For the `0.3.18` hardware pass, repeat either known failing resource shape:
save at the foyer bottom and load at the top, or save immediately before a
foyer door and load after entering it. After a hard lock, preserve
`/ndebug.log`; its final `E0` record identifies the update call that was
entered and did not return, while a final `E1` proves that call completed.

Build the Homebrew Channel package with:

```text
cmake --preset diagnostic_console
cmake --build --preset diagnostic
```

The resulting ZIP contains only `boot.dol`, `icon.png`, `meta.xml`, and the
authenticated `mod_lmj.bin`; it does not patch the ISO.
