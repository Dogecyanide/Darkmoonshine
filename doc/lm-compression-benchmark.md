# LM compression benchmark — private format-27 captures

The fast codec materially reduces host CPU work and fits all three measured
companions. It does **not** make a second resident state fit the current safe
inactive-slot region. These are host measurements, not Wii timing or gameplay
acceptance results.

## Reproduce

From the repository root:

```powershell
.\venv\Scripts\python.exe scripts/benchmark_lm_compression.py '<private archive.lms>' --warm 7 --cold 3
.\venv\Scripts\python.exe scripts/test_lm_compression_benchmark.py
```

Multiple private `.lms` inputs are accepted. `--json` prints detailed anonymous
metrics, compiler flags, source hashes, and pair-capacity calculations. The
script never writes archives, decompressed captures, authentication keys, or
SD contents. Only its host library is built in a temporary directory. Six
synthetic-only tests exercise extraction, corruption rejection, metadata
redaction, native round trips, CRC parity, guards, and capacity accounting.

`lm_codec_fixture.py` uses the existing full archive reader before extracting
the exact `0x419B00` shared parent. It validates envelope/core/companion CRCs,
bounded decode, census, and saved heap lists. It does **not** claim SipHash
authentication or live-owner validation; no secret key is needed. Fixture
identity is reported by SHA-256, never the runner's archive name.

## Method

Measured 2026-09-11 on the Windows x86-64 host, MSYS2 g++ 14.2.0, `-O2`.
The benchmark compiles the actual `lm_state_deflate.cpp`, including retained
128-probe miniz and the new LZ4 block codec. Both are called with the existing
`0x50000` workspace. Every codec is checked against the original decoded bytes
and an independent Python decoder. The first real capture's miniz output is
also byte-identical to its original stored companion stream.

Warm figures are seven-run medians after an untimed call. Cache-evicted figures
are three-run medians after touching a separate 64 MiB host allocation; that
sweep is outside timing. This is deliberately labelled **evicted**, not a
claim that every hardware cache is cold. Every encoder invocation initializes
its own state; warm runs never reuse a precompressed result. Allocations,
archive parsing, Python verification, and file I/O are outside timing.

Decode validation uses the production null-destination path; restore uses a
separate host destination. The combined load measurement performs both passes,
as production must. Host raw-copy numbers do not include Wii cache stores or
MEM1/MEM2 bus effects. Save/load component timings omit native owner checks,
the remaining raw-core work, SD operations, graphics/audio synchronization,
and the small companion descriptor. They are not end-to-end savestate times.

## Companion results

All three captures have a 12,778,912-byte raw core and 4,299,520-byte parent.
The companion's actual remaining payload capacity is 3,793,440 bytes, below
the 3,865,600-byte shared staging capacity. The table includes stream headers
and checksums; the runtime additionally rounds each stream to 32 bytes.

| Capture SHA-256 prefix | Deflate bytes | Fast bytes | Deflate save, warm/evicted ms | Fast save, warm/evicted ms | Deflate validation + restore, warm/evicted ms | Fast validation + restore, warm/evicted ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `91d38168a425` | 2,092,549 | 2,668,766 | 163.201 / 162.657 | 5.177 / 5.186 | 21.299 / 21.309 | 3.556 / 3.574 |
| `9bca6fc73bbb` | 2,092,641 | 2,668,870 | 161.412 / 178.797 | 5.500 / 5.511 | 20.940 / 21.413 | 3.893 / 3.722 |
| `25a95cd0e794` | 2,092,600 | 2,668,861 | 174.607 / 178.848 | 7.505 / 7.090 | 21.104 / 21.014 | 3.698 / 3.715 |

Fast saves are about 23–31.5 times faster in this host test, with about 27.5%
larger streams. All fast streams fit and leave at least **1,124,544 bytes**
of payload headroom. That is evidence for these captures, not a universal
compression-ratio guarantee. The runtime's dense fallback remains important
for poorly compressing future data. An uncompressed parent does not fit:
raw core + descriptor + parent exceeds the current shared payload limit by
506,080 bytes, even before considering the staging limit.

## Copy and checksum costs

Across the three captures, warm host medians:

- Parent raw copy: 0.151–0.157 ms; raw core copy: 0.451–0.588 ms.
- Parent CRC: old eight-bit loop 32.056–33.127 ms, new table 7.769–7.992 ms.
- Raw-core CRC per pass: old loop 95.952–99.200 ms, new table 23.207–24.427 ms.

For the first capture, a separate three-warm/one-evicted component run gave:

| Actual host operations | Old Deflate + old CRC, warm/evicted ms | Fast + table CRC, warm/evicted ms |
| --- | ---: | ---: |
| Compress + packed commit copy + packed CRC | 178.572 / 178.344 | 10.472 / 10.473 |
| Packed CRC + validate decode + restore decode | 36.928 / 38.020 | 8.721 / 8.625 |

The checksum improvement is separate from the codec improvement. Retaining
the old checksum loop would still cost 26.173 ms for that fast save component
and 24.418 ms for its load component. No integrity check is skipped.

## Second-state capacity

The reserved snapshot pool is `0xFF0000` bytes; the ARM-visible payload ceiling
is `0xFD0000` = 16,580,608 bytes. The normal shared-companion ceiling leaves
another `0x2000` for the census/profile trailer. Each captured trailer here is
7,840 bytes. A raw active state with the fast companion and trailer occupies
15,455,584–15,455,712 bytes; it is not a 2.67 MB state—the core remains raw.

The inactive/rollback file-patch suffix has an absolute maximum of
`0x57F000` bytes **before** the live patch prefix. Even the smallest possible
aligned prefix leaves only **5,763,040 bytes**. The real prefix is published
by the launcher and can make this smaller. The separate 3,865,600-byte pack
staging area is actively overwritten when saving a companion and cannot also
be treated as persistent second-state storage.

The benchmark recompresses the entire private payload, including raw core,
encoded companion, alignment, and census/profile trailer. It also measures
the raw core separately. Alternative companion representations are assembled
only in host memory with a rebound descriptor CRC; this is a capacity model,
not an exported archive or proof of native compatibility.

| Capture | Raw core compressed with Deflate | Raw core compressed fast | Whole payload, dense parent + Deflate | Whole payload, fast parent + fast codec |
| --- | ---: | ---: | ---: | ---: |
| `91d38168a425` | 4,679,155 | 5,852,056 | 6,758,939 | 8,458,843 |
| `9bca6fc73bbb` | 4,880,313 | 6,195,784 | 6,960,918 | 8,804,085 |
| `25a95cd0e794` | 4,816,751 | 6,061,721 | 6,897,288 | 8,668,212 |

Even the smallest complete dense inactive state exceeds the best-case suffix
by **995,899 bytes**; the largest exceeds it by **1,197,878 bytes**. Compressing
the core and parent separately is not a solution either: for the first
capture, both dense extents plus descriptors/alignment/trailer take
6,779,648 bytes. The script reports every fast/dense combination.

Two complete dense payloads *alone* can mathematically fit the main payload
pool: the largest distinct pair measured is 13,858,206 bytes (13,921,836 for
two copies of the largest). That does not provide the raw active image,
transaction rollback, safe switching/decode space, or protection from shared
staging writes. The largest distinct fast/fast pair is 17,472,297 bytes and
does not even pass that limited byte-count test.

Conclusion: keep one reliable resident state in this build. Two fully packed
states would require a deliberate storage/transaction redesign, not simply
raising the slot count or borrowing the existing shared staging buffer.
