# DarkMoonshine development priorities

Updated for **V1.0.0 Frozen in Time**, 2026-09-11.

## Release baseline

The user reports that **all ten RC4 Wii checklist items worked**. That covers
the exercised same-room overwrites, busy-room operation, cross-floor returns,
Parlor/Storage/Boneyard/Anteroom route, transition refusals, named SD archive
switching, failed-import protection, soft/full reboot reuse, and practice
session. V1.0.0 retains RC4's gameplay implementation and applies final
branding. This does not establish every room, boss or event boundary.

One dependable RAM state remains the priority. LZ4 companion/rollback
compression, dense capacity fallback and equivalent table checksums improve
processing without removing integrity or owner checks. Two resident states
do not fit the current safe inactive region in the measured captures; do not
borrow shared staging, the live patch prefix, or rollback space.

Named SD states, the controller keyboard/browser, successful-import versus
Load feedback, confirmed deletion, timer Creation and persistent Wii
preferences are implemented. Native-menu timer counting and the recordable
Reset Room combination are included. The mod menu must always count active
game time, while genuine native scripted stops remain authoritative.

Snapshot format stays **28**, storage protocol **6**, preferences version **2**.
Fresh final-release archives are required because branding changes the
authenticated build identity. Keep previous archives and their name/key
records as backups; do not weaken authentication to import another build.

## Remaining state work

1. Resolve **Secret Altar event-resource ownership** before expanding that
   state-load boundary. Existing evidence identifies unmatched `event06` and
   `event74` resources; menu entry succeeds but a prior mansion state can
   refuse with EPOCH. Preserve the guard until ownership/lifetime is proven.
2. Extend mansion-to-boss and other map/arena coverage using explicit native
   owners and runner evidence. A working warp list is not proof that a state
   can cross the same boundary.
3. Maintain post-load regression checks beyond the first restored frame:
   movement, a normal door crossing, later menu cleanup and repeated loads.
   Keep disc/audio/heap/renderer gates and complete failed-import rollback.
4. Consider additional resident slots only with a measured, transactional
   storage redesign. A compressed byte-count sum alone is insufficient.

## Deferred practice features

- Dojo, Boss Rush and Portrait Rush are requested and reportedly used by
  runners. They need actual mode controllers, encounter progression and
  restart/completion handling, not aliases for the existing boss warps.
- Verify pearl-dupe and Chauncey success conditions from runner references
  before presenting hit/miss judgements. Current R-pump and reference timing
  displays report measurements only.
- Precise ghost-capture angle guidance needs native event data and
  runner-defined success criteria.
- Infinite health remains distinct from the implemented health presets.

## Evidence and release discipline

Keep ZIP packages and a concise version-specific checklist with releases.
Collect the entire `lm_dumps` folder after a fault, available LM crash
reports, and the exact action sequence. Journals rotate through eight banks;
they are not eight state slots. Private captures and SD keys do not belong in
public packages or a pull request.

Hardware reports belong to the tested candidate: do not describe a host
benchmark, successful compilation, or final version-string change as a new
Wii run. Historical candidate notes and verification records remain in
`doc/darkmoonshine-1.0.0-rc*.md` and their verification files.

References:
[release notes](darkmoonshine-1.0.0.md),
[storage contract](lm-state-storage.md),
[Secret Altar evidence](lm-secret-altar-rc1-limit.md),
[compression/capacity measurements](lm-compression-benchmark.md),
[dojo/rush plan](lm-dojo-rush-reconstruction.md).
