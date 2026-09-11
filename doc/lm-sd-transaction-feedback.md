# SD archive transactions and browser transport

The runner's missing-file/old-session “imported” feedback is confirmed as a report,
not as a uniquely reproduced hardware mechanism. The old completion branch
already retained `success == false` while rolling back the previous resident
slot. Restoring that slot must not be described as a successful import.

One source-backed stale-feedback path is reproducible: after a successful import,
a new request rejected by the busy gate returned without replacing the prior
success string. The menu also ignored the request return value. The new client
replaces rejected-request feedback and identifies both the action and requested
eight-digit archive ID. Completion distinguishes file absence, incompatible
session/build/format, CRC/auth failure, malformed response, and I/O failure.
Failed import text says whether the old slot was kept or the slot is empty.

## Ownership and transaction identity

Mailbox v3 preserves the original 160-byte prefix. The ARM-owned 32-byte receipt
at offset160 carries sequence, process session, operation, requested ID and
expected payload length. ARM publishes it before the response/acknowledgement
line. PPC requires matching identity plus exact result ID/transfer length before
accepting an import. CRC, per-process authentication, snapshot validation and
trailer-generation validation still all apply. A readable old-session header is
rejected on ARM before any snapshot payload transfer. Reboot compatibility is
not enabled; archived metadata never grants permission to restore game memory.

The original selected slot is compressed outside the import destination before
reading a file. Failed reads or validations restore and revalidate those backup
bytes. A successful backup rollback does not change the failed result. Catalog
requests do not make a backup, flush the raw snapshot, or write any snapshot byte.

## Single-state shared-resource profile (snapshot format 21)

There is one resident logical state. The core header's `totalSize` and displayed
core size remain the contiguous GAME/static snapshot extent. Storage `rawSize`
includes that core plus the mandatory 64-byte shared-resource descriptor and
its aligned compressed payload; the census trailer follows this combined
extent. Exports authenticate the complete combined payload and trailer.

The storage client requires `snapshotStoredSize` to match the candidate's
combined extent and `snapshotCompanionValid` to verify the descriptor, checksum
and entire compressed stream without a game destination. It rejects a core or
descriptor outside the authenticated candidate extent before calling either
helper. It also verifies the core checksum and all trailer generations. A
successful import does not itself restore any live game memory.

Before any ARM import, the old combined state and census are compressed directly
into the admitted packed-cache pool followed by the existing codec staging
area. Both regions are outside the entire possible ARM snapshot destination;
neither the old raw tail nor prospective new raw tail is used. The two-segment
backup is CRC-checked and dry-inflated before the request starts, then retained
in place. No extra packed-copy step or secondary resident slot is needed.
Capacity failure leaves the raw state unchanged and starts no ARM request.

The ARM header gate bounds the complete candidate against the fixed payload
ceiling before its first payload read. Thus even a larger candidate or a late
validation failure cannot overwrite its rollback. Failed import restores the
old combined extent, revalidates its companion/core/census, and restores the
core display size from the restored header rather than confusing it with the
combined stored size. Old-session authentication remains mandatory.

## Catalog

Command3 lists actual regular files in `lm_states`. The request ID is an exclusive
lower bound, with0 selecting the first page. Filenames must exactly match
`archive_########.lms`, with a nonzero ID; temporary files, short IDs, extensions,
directories and nonmatching names are ignored. No file is deleted or renamed by
listing. A bounded16 directory entries are scanned per worker invocation, keeping
only the smallest8 eligible IDs in sorted fixed storage. Directory handles close
on completion, errors or process/request cancellation. Candidate header reads
are64bytes, never game snapshot payload reads.

The mailbox appends32bytes of catalog control and eight32-byte records, ending at
480bytes. The codec workspace consequently starts at cache+0x200, not+0x100;
compile-time assertions prevent overlap. Control fields are count, input cursor,
next cursor and more-pages flag. Entries contain ID, file bytes, build CRC,
session, snapshot format, generation and a header-valid flag. These are browsing
hints only: payload authentication is performed again by actual import.

The PPC validates the complete catalog response before exposing rows. The UI can
show incompatible files but should disable their import and show the specific
header mismatch. Browser status is separate from import/export status so refreshing
the list cannot erase an import failure. The final file ID is the next-page cursor;
UI-owned prior cursors allow backwards paging without an unbounded index.

## Verification

`scripts/test_lm_state_transactions.py` builds the production PPC client include,
production ARM worker, deflate and authentication into a native harness. Only
PowerPC/cache instructions and the surrounding game snapshot shape are host
fixtures. Tests cover success, missing files, empty slots, old sessions, corrupted
payload/header/auth, partial reads, stale busy feedback, mismatched receipts,
rollback, strict catalog names, pagination, malformed responses, bounded directory
scans, handle cleanup, unchanged snapshot bytes and preservation of import errors.
The format-21 profile additionally tests different old/new core and companion
sizes, authenticated malformed companion metadata, oversized core/outer extents,
two-segment rollback after a larger partial import, and compression-capacity
failure without starting I/O. The companion owner proof is a host fixture in
this transaction harness; separate native/portable tests cover that game layout.
These tests do not emulate Wii cache coherency or certify gameplay restoration.

Hardware tests should include: export and browse; import the same-session file
into an empty/occupied slot; choose a nonexistent manual ID; retry with the old
slot still usable; browse after an error; reboot and see old files clearly marked
incompatible; list enough files for two pages; and interrupt/reopen the browser
while listing. File/import refusal must never say the selected archive was loaded.
