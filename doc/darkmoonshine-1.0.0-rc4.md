# DarkMoonshine — V1.0.0 Frozen in Time. Release candidate 4

Authors: **Dogecyanide, Nintendont Team**.

## Changes

- Adapt Moonshine V2.3.0's independent 128 KiB LZ4-block approach for the
  shared-resource companion and temporary SD-import rollback. Incompressible
  blocks fall back to raw storage. If the fast stream exceeds available
  capacity, retry the existing denser compressor before refusing.
- Replace the bit-at-a-time CRC calculation with an equivalent table lookup.
  Keep every checksum pass, dry-decode validation, authentication check,
  owner/allocator proof, and cache synchronization in the existing flow.
  Bad data or insufficient capacity must not replace a working RAM state.
- Compile only the codec unit for speed. The rest of the payload retains
  size optimization. No heap, MEM2 reservation or workspace was enlarged.
- Keep **one complete RAM slot**, with the existing named SD archive browser.
  Across three real captures a second densely compressed complete state
  needs 6.45–6.64 MiB, exceeding the best-case separate safe space of about
  5.5 MiB. Two slots need a storage redesign, not just a larger slot count.

Three real captures restored byte-for-byte in host tests. The shared-parent
compression step was 23–31.5 times faster on the PC; its compressed data was
about 27.5% larger but still fit with over 1 MiB of headroom. This is **not**
an end-to-end or Wii speed claim. Raw game-state copying, SD access and native
synchronization remain. Wii pause measurements and the included ten focused
tests are required. See [benchmark details](lm-compression-benchmark.md) in
the source tree for exact sizes, method and limitations.

## Install and compatibility

Put the ZIP's `moonshine_luigis_mansion` folder inside SD **`Apps`**. Put its
separate **`Darkmoonshine_Theme` folder on the SD root**, alongside `Apps`.
Keep an existing custom Darkmoonshine theme if preferred. Install the
matching `boot.dol` and `mod_lmj.bin` together.

Create **fresh RC4 savestates**. Snapshot format is now **28**: the companion
explicitly identifies a fast/legacy-decoding envelope. Earlier archives are
not converted or deleted and cannot bypass build authentication by renaming.
The Wii launcher still boots an unpatched Japanese GLMJ01 ISO. Storage
protocol 6 and preferences version 2 remain unchanged. Settings, themes,
normal saves, keys, archive names and log locations are preserved.

RC3's native-menu timer option, Reset Room combo and on-demand launcher/theme
support remain included. No new runtime acceptance of those features is
claimed here. Compilation and host checks are not a Wii/Dolphin gameplay pass.
Secret Altar's unmatched event-resource refusal remains; no universal boss
or mansion-wide compatibility is claimed. Dojo/rush and verified trick-success
detectors remain deferred.

The ZIP includes the miniz and LZ4 license notices. No retail ISO, normal game
save, runner archive or private key is distributed.
