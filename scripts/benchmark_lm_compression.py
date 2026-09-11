"""Benchmark production LM codecs on private format-27 archives, without writes.

Only anonymous fixture hashes, byte counts, and host timings are emitted.
No retail payload, archive names, or authentication identifiers leave memory.
This is not a Wii timing prediction or a native save/load acceptance test.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import struct
import subprocess
import tempfile
import time
import zlib

try:
    from scripts.lm_codec_fixture import load_companion
    from scripts.lm_quick_codec import decode_fast
except ModuleNotFoundError:
    from lm_codec_fixture import load_companion
    from lm_quick_codec import decode_fast

ROOT = Path(__file__).resolve().parents[1]


class NativeBenchmark:
    def __init__(self, evict_mib=64):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler and Path("C:/msys64/mingw64/bin/g++.exe").is_file():
            compiler = "C:/msys64/mingw64/bin/g++.exe"
        if not compiler:
            raise RuntimeError("native C++ compiler required")
        self.temp = tempfile.TemporaryDirectory(prefix="lm-codec-benchmark-")
        binary = Path(self.temp.name) / ("codec.dll" if os.name == "nt" else "codec.so")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        self.flags = ["-shared", "-O2", "-std=c++17", "-fPIC"]
        sources = [ROOT / "lm_diag/src/lm_state_deflate.cpp",
                   ROOT / "scripts/lm_codec_benchmark_bridge.cpp"]
        subprocess.run([compiler, *self.flags, "-I", str(ROOT / "include"),
                        *(str(p) for p in sources), "-o", str(binary)],
                       env=env, check=True, capture_output=True)
        self.compiler = subprocess.check_output([compiler, "--version"], env=env,
                                                text=True).splitlines()[0]
        self.source_hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sources}
        for path in (ROOT / "include/susamune/lm_state_deflate.h", ROOT / "include/susamune/lm_crc32.h",
                     *sorted((ROOT / "lm_diag/vendor").rglob("*.c")),
                     *sorted((ROOT / "lm_diag/vendor").rglob("*.h"))):
            self.source_hashes[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
        self.lib = ctypes.CDLL(str(binary))
        u, p = ctypes.c_uint, ctypes.c_void_p
        self.lib.lm_bench_constant.argtypes = [u]
        self.lib.lm_bench_constant.restype = u
        self.lib.lm_bench_pack.argtypes = [u, p, u, p, u, p]
        self.lib.lm_bench_pack.restype = u
        self.lib.lm_bench_unpack.argtypes = [p, u, p, u, p]
        self.lib.lm_bench_unpack.restype = ctypes.c_int
        self.lib.lm_bench_load.argtypes = [p, u, p, u, p]
        self.lib.lm_bench_load.restype = ctypes.c_int
        self.lib.lm_bench_copy.argtypes = [p, p, u]
        self.lib.lm_bench_copy.restype = None
        for name in ("lm_bench_crc_bitwise", "lm_bench_crc_table", "lm_bench_evict"):
            getattr(self.lib, name).argtypes = [p, u]
            getattr(self.lib, name).restype = u
        self.workspace_size = self.lib.lm_bench_constant(0)
        self.staging_size = self.lib.lm_bench_constant(1)
        self.payload_limit = self.lib.lm_bench_constant(2)
        self.fast_available = bool(self.lib.lm_bench_constant(3))
        self.cache_max = self.lib.lm_bench_constant(4)
        self.evict = ctypes.create_string_buffer(evict_mib * 1024 * 1024)
        self.lib.lm_bench_evict(self.evict, len(self.evict))

    def close(self):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(self.lib._handle)
        self.temp.cleanup()

    def measure(self, action, samples, evicted):
        # Eviction/page touch is excluded. This perturbs host caches, not Wii MEM2.
        action()
        elapsed = []
        for _ in range(samples):
            if evicted:
                self.lib.lm_bench_evict(self.evict, len(self.evict))
            start = time.perf_counter_ns()
            action()
            elapsed.append((time.perf_counter_ns() - start) / 1e6)
        return {"median_ms": statistics.median(elapsed), "min_ms": min(elapsed),
                "max_ms": max(elapsed), "samples": samples}

    def timing(self, action, warm, cold):
        return {"warm": self.measure(action, warm, False),
                "evicted": self.measure(action, cold, True)}

    def run(self, fixture, warm, cold):
        n = len(fixture.parent)
        source = ctypes.create_string_buffer(fixture.parent, n)
        core = ctypes.create_string_buffer(fixture.core, len(fixture.core))
        core_copy = ctypes.create_string_buffer(len(fixture.core))
        restored = ctypes.create_string_buffer(n + 32)
        workspace = ctypes.create_string_buffer(self.workspace_size + 32)
        ctypes.memset(ctypes.addressof(workspace) + self.workspace_size, 0xCD, 32)
        ctypes.memset(ctypes.addressof(restored) + n, 0xAD, 32)
        result = {**fixture.identity, "staging_bytes": self.staging_size,
                  "codecs": {}, "raw": {}, "crc": {}}
        companion_streams = {}

        def timed(action):
            return self.timing(action, warm, cold)

        for fast, name in ((0, "deflate_128"), (1, "lz4_fast")):
            if fast and not self.fast_available:
                continue
            bound = n + n // 255 + 4096
            packed = ctypes.create_string_buffer(bound + 32)
            ctypes.memset(ctypes.addressof(packed) + bound, 0xAB, 32)
            pack = lambda: self.lib.lm_bench_pack(fast, source, n, packed, bound, workspace)
            packed_size = pack()
            if not 0 < packed_size <= bound:
                raise RuntimeError("unexpected codec size/capacity failure")
            validate = lambda: self.lib.lm_bench_unpack(packed, packed_size, None, n, workspace)
            restore = lambda: self.lib.lm_bench_unpack(packed, packed_size, restored, n, workspace)
            load = lambda: self.lib.lm_bench_load(packed, packed_size, restored, n, workspace)
            if not validate() or not restore() or restored.raw[:n] != fixture.parent:
                raise RuntimeError("codec validation/byte-exact restore failed")
            if not fast and zlib.decompress(packed.raw[:packed_size]) != fixture.parent:
                raise RuntimeError("independent zlib verification failed")
            if fast and decode_fast(packed.raw[:packed_size], n) != fixture.parent:
                raise RuntimeError("independent LZ4 verification failed")
            packed_copy = ctypes.create_string_buffer(packed_size)
            def save_components(crc):
                if pack() != packed_size:
                    raise RuntimeError("unstable packed size")
                self.lib.lm_bench_copy(packed_copy, packed, packed_size)
                return crc(packed_copy, packed_size)

            def load_components(crc):
                checksum = crc(packed, packed_size)
                if not load():
                    raise RuntimeError("load validation failed")
                return checksum

            operations = {"compress": pack, "validate_only": validate, "restore_only": restore,
                          "validate_then_restore": load,
                          "commit_copy": lambda: self.lib.lm_bench_copy(packed_copy, packed, packed_size),
                          "packed_crc_bitwise": lambda: self.lib.lm_bench_crc_bitwise(packed, packed_size),
                          "packed_crc_table": lambda: self.lib.lm_bench_crc_table(packed, packed_size)}
            for kind in ("bitwise", "table"):
                crc = getattr(self.lib, "lm_bench_crc_" + kind)
                if crc(packed, packed_size) != zlib.crc32(packed.raw[:packed_size]):
                    raise RuntimeError("packed CRC parity failed")
                operations["save_compress_commit_crc_" + kind] = lambda crc=crc: save_components(crc)
                operations["load_crc_validate_restore_" + kind] = lambda crc=crc: load_components(crc)
            metrics = {key: timed(action) for key, action in operations.items()}
            if pack() != packed_size or not load() or restored.raw[:n] != fixture.parent:
                raise RuntimeError("codec output changed between benchmark iterations")
            rounded = (packed_size + 31) & ~31
            capacity = min(result["companion_capacity_bytes"], self.staging_size)
            metrics.update(packed_bytes=packed_size, packed_percent=100.0 * packed_size / n,
                           capacity_bytes=capacity, fits=rounded <= capacity,
                           remaining_payload_bytes=result["companion_capacity_bytes"] - rounded,
                           max_core_bytes=(self.payload_limit - 64 - rounded) & ~31,
                           resulting_archive_bytes=64 + len(fixture.core) + 64 + rounded + fixture.trailer_size,
                           exact_roundtrip=True,
                           equals_original_stream=packed.raw[:packed_size] == fixture.original_packed)
            result["codecs"][name] = metrics
            companion_streams[name] = packed.raw[:packed_size]
            if packed.raw[bound:] != b"\xAB" * 32:
                raise RuntimeError("packed output guard overwritten")

        for label, data, target, size in (("companion", source, restored, n),
                                          ("core", core, core_copy, len(fixture.core))):
            result["raw"][label + "_copy"] = timed(lambda: self.lib.lm_bench_copy(target, data, size))
            expected_crc = zlib.crc32(fixture.parent if label == "companion" else fixture.core)
            for kind in ("bitwise", "table"):
                fn = getattr(self.lib, "lm_bench_crc_" + kind)
                if fn(data, size) != expected_crc:
                    raise RuntimeError("CRC parity failed")
                result["crc"][label + "_" + kind] = timed(lambda: fn(data, size))
        result["raw"].update(companion_fits=n <= result["companion_capacity_bytes"] and n <= self.staging_size,
                             raw_total_bytes=len(fixture.core) + 64 + n,
                             raw_over_payload_bytes=max(0, len(fixture.core) + 64 + n - self.payload_limit))
        if workspace.raw[self.workspace_size:] != b"\xCD" * 32 or restored.raw[n:] != b"\xAD" * 32:
            raise RuntimeError("workspace or restore guard overwritten")
        result["two_slot_capacity"] = self.capacity_analysis(fixture, companion_streams, workspace)
        return result

    def capacity_analysis(self, fixture, streams, workspace):
        """Size-only model, not a slot allocator or a claim of transactional safety."""
        result = {"raw_active_with_dense_parent_bytes": len(fixture.core) + 64 +
                  ((len(streams["deflate_128"]) + 31) & ~31) + fixture.trailer_size,
                  "inactive_suffix_max_before_live_prefix_bytes": self.cache_max,
                  "inactive_suffix_best_case_bytes": self.cache_max - 32,
                  "shared_staging_not_persistent_bytes": self.staging_size,
                  "live_prefix_is_runtime_dependent": True,
                  "compressed_core": {}, "compressed_entire_payload": {},
                  "separate_core_parent": {}}
        def pack_bytes(data, fast):
            size = len(data)
            source = ctypes.create_string_buffer(data, size)
            bound = size + size // 255 + 4096
            output = ctypes.create_string_buffer(bound)
            needed = self.lib.lm_bench_pack(fast, source, size, output, bound, workspace)
            if not 0 < needed <= bound:
                raise RuntimeError("size-analysis compression failed")
            if not self.lib.lm_bench_unpack(output, needed, None, size, workspace):
                raise RuntimeError("size-analysis stream validation failed")
            return needed

        for fast, algorithm in ((0, "deflate_128"), (1, "lz4_fast")):
            if fast and not self.fast_available:
                continue
            core_size = pack_bytes(fixture.core, fast)
            result["compressed_core"][algorithm] = core_size
            for parent_name, parent_stream in streams.items():
                descriptor = bytearray(fixture.descriptor)
                struct.pack_into(">I", descriptor, 4, 2 if parent_name == "lz4_fast" else 1)
                struct.pack_into(">I", descriptor, 16, len(parent_stream))
                struct.pack_into(">I", descriptor, 20, 0)
                struct.pack_into(">I", descriptor, 20, zlib.crc32(parent_stream, zlib.crc32(descriptor)))
                parent = bytes(descriptor) + parent_stream + bytes((-len(parent_stream)) % 32)
                payload = fixture.core + parent + fixture.trailer
                packed_size = pack_bytes(payload, fast)
                key = algorithm + "_with_" + parent_name + "_parent"
                result["compressed_entire_payload"][key] = {
                    "input_bytes": len(payload), "packed_bytes": packed_size,
                    "fits_best_case_suffix": packed_size <= self.cache_max - 32,
                    "remaining_for_live_prefix_bytes": self.cache_max - packed_size,
                    "active_plus_inactive_bytes": len(payload) + packed_size}
                # Two explicit compressed extents need descriptors and census too.
                result["separate_core_parent"][key] = (
                    ((core_size + 31) & ~31) + len(parent) + fixture.trailer_size)
        return result


def report_text(report):
    print("Host-only LM companion codec benchmark; all times are milliseconds.")
    print(report["compiler"])
    print("Cache-evicted uses a separate 64+ MiB host sweep; not hardware cache flushing.")
    for fixture in report["fixtures"]:
        print(f"\nFixture {fixture['archive_sha256'][:12]}: format {fixture['format']}, "
              f"raw core {fixture['raw_core_bytes']:,}, companion {fixture['parent_bytes']:,} bytes")
        print(f"  Companion capacity {fixture['companion_capacity_bytes']:,}; staging {fixture['staging_bytes']:,}")
        print("  Codec          packed bytes    save warm/cold    validate+restore warm/cold    remaining")
        for name, metric in fixture["codecs"].items():
            save, load = metric["compress"], metric["validate_then_restore"]
            print(f"  {name:14} {metric['packed_bytes']:11,d}    "
                  f"{save['warm']['median_ms']:7.3f}/{save['evicted']['median_ms']:7.3f}    "
                  f"{load['warm']['median_ms']:7.3f}/{load['evicted']['median_ms']:7.3f}    "
                  f"{metric['remaining_payload_bytes']:,}")
        print(f"  Raw companion fits: {fixture['raw']['companion_fits']}; "
              f"overflow {fixture['raw']['raw_over_payload_bytes']:,} bytes")
        for key, metric in fixture["raw"].items():
            if isinstance(metric, dict):
                print(f"  {key}: {metric['warm']['median_ms']:.3f} warm / {metric['evicted']['median_ms']:.3f} evicted")
        for key, metric in fixture["crc"].items():
            print(f"  CRC {key}: {metric['warm']['median_ms']:.3f} warm / {metric['evicted']['median_ms']:.3f} evicted")
        capacity = fixture["two_slot_capacity"]
        print(f"  Inactive suffix best case: {capacity['inactive_suffix_best_case_bytes']:,} bytes; actual live prefix may reduce it")
        print(f"  Compressed core: {capacity['compressed_core']}")
        for key, item in capacity["compressed_entire_payload"].items():
            print(f"  Whole {key}: {item['packed_bytes']:,} bytes; fits suffix={item['fits_best_case_suffix']}; "
                  f"active+inactive={item['active_plus_inactive_bytes']:,}")
    if "pair_capacity" in report:
        print("\nCompressed-pair sizes alone (NOT sufficient to implement safe slot switching):")
        for key, item in report["pair_capacity"].items():
            print(f"  {key}: largest pair {item['largest_distinct_pair_bytes']:,}; "
                  f"payload-only fits={item['pair_fits_payload_only']}; exclusive inactive suffix fits="
                  f"{item['all_inactive_fit_best_case_suffix']}")


def pair_capacity(fixtures, payload_limit):
    if not fixtures:
        return {}
    keys = set.intersection(*(set(f["two_slot_capacity"]["compressed_entire_payload"]) for f in fixtures))
    result = {}
    for key in sorted(keys):
        sizes = sorted((f["two_slot_capacity"]["compressed_entire_payload"][key]["packed_bytes"]
                        for f in fixtures), reverse=True)
        pair = sum(sizes[:2]) if len(sizes) > 1 else sizes[0] * 2
        result[key] = {"largest_distinct_pair_bytes": pair, "same_largest_twice_bytes": sizes[0] * 2,
                       "payload_limit_bytes": payload_limit, "pair_fits_payload_only": pair <= payload_limit,
                       "all_inactive_fit_best_case_suffix": all(
                           f["two_slot_capacity"]["compressed_entire_payload"][key]["fits_best_case_suffix"]
                           for f in fixtures),
                       "does_not_account_for_safe_switch_decode_or_rollback": True}
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--warm", type=int, default=7)
    parser.add_argument("--cold", type=int, default=3)
    parser.add_argument("--evict-mib", type=int, default=64)
    parser.add_argument("--json", action="store_true", help="anonymous metrics only, on stdout")
    args = parser.parse_args(argv)
    if not (1 <= args.warm <= 100 and 1 <= args.cold <= 100 and 64 <= args.evict_mib <= 512):
        parser.error("samples must be 1..100, cache sweep 64..512 MiB")
    bench = NativeBenchmark(args.evict_mib)
    try:
        report = {"host": platform.platform(), "processor": platform.processor(),
                  "compiler": bench.compiler, "flags": bench.flags,
                  "source_sha256": bench.source_hashes, "evict_mib": args.evict_mib,
                  "fast_available": bench.fast_available, "fixtures": []}
        for path in args.archives:
            report["fixtures"].append(bench.run(load_companion(path), args.warm, args.cold))
        report["pair_capacity"] = pair_capacity(report["fixtures"], bench.payload_limit + 0x2000)
        if args.json:
            print(json.dumps(report, indent=2))
        else:
            report_text(report)
    finally:
        bench.close()


if __name__ == "__main__":
    main()
