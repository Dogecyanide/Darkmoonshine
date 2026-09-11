# Shared-resource snapshot companion regression

`scripts/test_lm_shared_companion.py` builds a Windows host harness that includes
the actual `lm_diag/src/lm_state_shared.inc` unchanged. The real shared-owner
validator and miniz compressor/decompressor also run. Adapters provide private
low-address RAM, big-endian MEM1 reads, the outer snapshot header fields, and an
observed no-op cache flush. No emulator, SD card, or live game memory is touched.

The suite uses the optional private `build-lm-emu/diagnostic-capture-0.3.30/mem1.bin`
capture. It skips when that fixture or a native MinGW compiler is unavailable;
the retail-memory fixture is not distributed with the tests.

```powershell
venv/Scripts/python.exe -m unittest scripts/test_lm_shared_companion.py -v
```

Coverage includes old-slot preservation on capacity/identity failure, exact
descriptor and alignment accounting, incompressible-resource overflow, header
generation/checksum/heap binding, descriptor and payload corruption, zero
padding, truncated or wrong-size zlib streams with freshly recomputed wrapper
checksums, complete parent round-trip, and live-derived restore destinations.
Invalid dry decodes must leave all private MEM1 bytes unchanged. A forged saved
destination is refused by the normal live-identity gate. A separate helper-only
case verifies that the restore function writes the validated live base even
when the saved base differs; it is not permission to omit that gate in gameplay.

These are byte-level host regressions, not certification of Wii cache behavior,
GPU/audio synchronization, cross-floor gameplay, or post-warp resource lifetime.
