"""Execute the PPC-shared complete grain graph validator against bounded data."""

import ctypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
START, END = 0x80BE4560, 0x817FB140
MANAGERS = ((0x803CBAF0, 0x81000000, 15, 0x81010000),
            (0x803CBF48, 0x81200000, 80, 0x81300000))
CSTRIDE, PSTRIDE, PCOUNT = 0x1B8, 0x54, 1500
NONE = 0xFFFFFFFF


class GrainNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-grain-")
        output = Path(cls.temp.name) / ("grain.dll" if os.name == "nt" else "grain.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror",
                   "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_grain_state_harness.c"), "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_uint] * 3
        cls.lib.run.restype = ctypes.c_int
        cls.lib.setWord.argtypes = [ctypes.c_uint] * 2
        cls.lib.setWord.restype = ctypes.c_int
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint
        cls.lib.nullReader.restype = ctypes.c_int
        cls.lib.runMemory.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                     ctypes.c_uint, ctypes.c_uint]
        cls.lib.runMemory.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.lib.reset()

    def put(self, address, value):
        self.assertEqual(self.lib.setWord(address, value), 1)

    def run_case(self, start=START, end=END, fail=0):
        result = self.lib.run(start, end, fail)
        self.assertEqual(self.lib.metric(1), 0, "callback saw a foreign read")
        self.assertLess(self.lib.metric(0), 7000, "traversal did not remain bounded")
        return result

    def fault(self):
        return [self.lib.metric(i) for i in (3, 4)]

    def link(self, sentinel, nodes, next_offset, prev_offset):
        chain = [sentinel, *nodes, sentinel]
        for previous, following in zip(chain, chain[1:]):
            self.put(previous + next_offset, following)
            self.put(following + prev_offset, previous)

    def test_valid_empty_particle_lists_cover_both_pools(self):
        self.assertEqual(self.run_case(), 1)
        self.assertEqual(self.fault(), [0, 0])
        self.assertGreater(self.lib.metric(0), 6000)

    def test_valid_populated_controller_and_particle_lists(self):
        for manager, pool, count, particles in MANAGERS:
            nodes = [pool + i * CSTRIDE for i in range(count)]
            self.link(manager + 0x64, nodes[1:-1], 0x140, 0x144)
            self.link(manager + 0x220, [nodes[0], nodes[-1]], 0x140, 0x144)
            self.link(manager + 0xC,
                      [particles + i * PSTRIDE for i in range(3, PCOUNT)], 0x4C, 0x50)
            self.link(nodes[0] + 0x40, [particles, particles + PSTRIDE], 0x4C, 0x50)
            self.link(nodes[-1] + 0x40, [particles + 2 * PSTRIDE], 0x4C, 0x50)
        self.assertEqual(self.run_case(), 1)

    def test_each_controller_self_sentinel_is_checked(self):
        for _, pool, count, _ in MANAGERS:
            for index in range(count):
                with self.subTest(pool=hex(pool), index=index):
                    self.lib.reset()
                    field = pool + index * CSTRIDE + 0x94
                    self.put(field, NONE)
                    self.assertEqual(self.run_case(), 0)
                    self.assertEqual(self.fault(), [field, NONE])

    def test_full_particle_pool_can_belong_to_one_active_controller(self):
        for manager, pool, count, particles in MANAGERS:
            self.link(manager + 0x64, [], 0x140, 0x144)
            self.link(manager + 0x220, [pool + i * CSTRIDE for i in range(count)],
                      0x140, 0x144)
            self.link(manager + 0xC, [], 0x4C, 0x50)
            self.link(pool + 0x40, [particles + i * PSTRIDE for i in range(PCOUNT)],
                      0x4C, 0x50)
        self.assertEqual(self.run_case(), 1)

    def test_wrong_mapped_or_neighbor_sentinel_is_rejected(self):
        for manager, pool, _, _ in MANAGERS:
            for value in (0, START, manager + 0x220, pool + 0x40 + CSTRIDE,
                          pool + 0x40 - 4, 0xCC000000):
                self.lib.reset()
                self.put(pool + 0x94, value)
                self.assertEqual(self.run_case(), 0)
                self.assertEqual(self.fault(), [pool + 0x94, value])

    def test_manager_sentinel_identity_is_exact(self):
        for manager, _, _, _ in MANAGERS:
            for offset in (0x60, 0x21C, 0x3D8):
                self.lib.reset()
                self.put(manager + offset, NONE)
                self.assertEqual(self.run_case(), 0)
                self.assertEqual(self.fault(), [manager + offset, NONE])

    def test_all_four_invalid_pool_ranges_never_dereferenced(self):
        for manager, _, _, _ in MANAGERS:
            for field in (manager, manager + 4):
                for pool in (0, NONE, 0xFFFFFF00, 0xCC000000, 0x90000000,
                             0x817FFFFC, START - 4, END, END + 4, START + 1):
                    self.lib.reset()
                    self.put(field, pool)
                    self.assertEqual(self.run_case(), 0)
                    self.assertLessEqual(self.lib.metric(0), 4)
                    self.assertEqual(self.fault(), [field, pool])

    def test_exact_pool_end_is_valid_and_one_word_short_is_refused(self):
        end = MANAGERS[1][3] + ((PCOUNT * PSTRIDE + 31) & ~31)
        self.assertEqual(self.run_case(end=end), 1)
        self.assertEqual(self.run_case(end=end - 4), 0)
        self.assertEqual(self.fault(), [MANAGERS[1][0] + 4, MANAGERS[1][3]])

    def test_pool_overlap_is_refused_before_pool_reads(self):
        fields = [MANAGERS[0][0], MANAGERS[0][0] + 4,
                  MANAGERS[1][0], MANAGERS[1][0] + 4]
        bases = [MANAGERS[0][1], MANAGERS[0][3], MANAGERS[1][1], MANAGERS[1][3]]
        for i in range(1, 4):
            for j in range(i):
                self.lib.reset()
                self.put(fields[i], bases[j] + 4)
                self.assertEqual(self.run_case(), 0)
                self.assertLessEqual(self.lib.metric(0), 4)
                self.assertEqual(self.fault(), [fields[i], bases[j] + 4])
        self.lib.reset()
        self.put(fields[0], 0x803CBAF0)
        self.assertEqual(self.run_case(start=0x80003100), 0)
        self.assertEqual(self.lib.metric(0), 1)

    def test_invalid_game_bounds_prevent_even_manager_read(self):
        for start, end in ((0, END), (START, NONE), (0xCC000000, 0xCC100000),
                           (END, START), (START, START), (START + 1, END),
                           (START, END - 1), (0xFFFFFF00, 0x1000)):
            self.assertEqual(self.run_case(start=start, end=end), 0)
            self.assertEqual(self.lib.metric(0), 0)
            self.assertEqual(self.fault(), [start, end])
        self.assertEqual(self.lib.nullReader(), 0)

    def test_pool_overlap_in_native_allocation_padding_is_rejected(self):
        manager, pool, count, particles = MANAGERS[0]
        self.put(manager + 4, pool + count * CSTRIDE)
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.fault(), [manager + 4, pool + count * CSTRIDE])
        self.assertEqual(self.lib.metric(0), 2)
        self.lib.reset()
        self.put(MANAGERS[1][0], particles + PCOUNT * PSTRIDE)
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.fault(), [MANAGERS[1][0], particles + PCOUNT * PSTRIDE])
        self.assertEqual(self.lib.metric(0), 3)

    def test_first_and_last_controller_links_and_reciprocal_links(self):
        for _, pool, count, _ in MANAGERS:
            for index in (0, count - 1):
                for offset in (0x140, 0x144):
                    self.lib.reset()
                    field = pool + index * CSTRIDE + offset
                    self.put(field, NONE)
                    self.assertEqual(self.run_case(), 0)
                    self.assertEqual(self.fault(), [field, NONE])

    def test_first_and_last_particle_links_and_reciprocal_links(self):
        for _, _, _, pool in MANAGERS:
            for index in (0, PCOUNT - 1):
                for offset in (0x4C, 0x50):
                    self.lib.reset()
                    field = pool + index * PSTRIDE + offset
                    self.put(field, NONE)
                    self.assertEqual(self.run_case(), 0)
                    self.assertEqual(self.fault(), [field, NONE])

    def test_links_must_be_exact_pool_members_not_just_mapped_addresses(self):
        for manager, pool, count, particles in MANAGERS:
            for value in (pool, particles + 4, particles + PCOUNT * PSTRIDE,
                          manager + 0x64, MANAGERS[1][3] if manager == MANAGERS[0][0]
                          else MANAGERS[0][3]):
                self.lib.reset()
                self.put(particles + 0x4C, value)
                self.assertEqual(self.run_case(), 0)
                self.assertEqual(self.fault(), [particles + 0x4C, value])

    def test_no_sentinel_cycles_terminate_and_report_incoming_link(self):
        for _, pool, count, particles in MANAGERS:
            for last, first, offset in ((pool + (count - 1) * CSTRIDE, pool, 0x140),
                                        (particles + (PCOUNT - 1) * PSTRIDE,
                                         particles, 0x4C)):
                self.lib.reset()
                self.put(last + offset, first)
                self.assertEqual(self.run_case(), 0)
                self.assertEqual(self.fault(), [last + offset, first])

    def test_missing_controller_and_particle_coverage_is_rejected(self):
        for manager, pool, count, particles in MANAGERS:
            self.lib.reset()
            self.link(manager + 0x64, [pool + i * CSTRIDE for i in range(1, count)],
                      0x140, 0x144)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.fault(), [manager, count - 1])
            self.lib.reset()
            self.link(manager + 0xC, [particles + i * PSTRIDE for i in range(1, PCOUNT)],
                      0x4C, 0x50)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.fault(), [manager + 4, PCOUNT - 1])

    def test_duplicate_controller_membership_is_rejected(self):
        for manager, pool, _, _ in MANAGERS:
            self.lib.reset()
            self.put(manager + 0x220 + 0x140, pool)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.fault(), [manager + 0x220 + 0x140, pool])

    def test_duplicate_particle_membership_between_controllers_is_rejected(self):
        for manager, pool, _, particles in MANAGERS:
            self.lib.reset()
            self.link(manager + 0xC, [particles + i * PSTRIDE for i in range(1, PCOUNT)],
                      0x4C, 0x50)
            self.link(pool + 0x40, [particles], 0x4C, 0x50)
            other = pool + CSTRIDE + 0x40
            self.put(other + 0x4C, particles)
            self.put(other + 0x50, particles)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.fault(), [other + 0x4C, particles])

    def test_saved_controller_self_sentinels_can_pass_while_particle_link_is_bad(self):
        manager, pool, _, particles = MANAGERS[0]
        self.link(manager + 0xC, [particles + i * PSTRIDE for i in range(1, PCOUNT)],
                  0x4C, 0x50)
        self.link(pool + 0x40, [particles], 0x4C, 0x50)
        self.put(particles + 0x4C, NONE)
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.fault(), [particles + 0x4C, NONE])

    def test_callback_failures_stop_at_the_failed_read(self):
        for manager, pool, count, particles in MANAGERS:
            for field in (manager, manager + 4, manager + 0x60, pool + 0x94,
                          pool + (count - 1) * CSTRIDE + 0x94,
                          particles + 1499 * PSTRIDE + 0x4C):
                self.assertEqual(self.run_case(fail=field), 0)
                self.assertEqual(self.fault(), [field, 0])
                self.assertEqual(self.lib.metric(2), field)

    def check_memory(self, raw):
        data = (ctypes.c_ubyte * len(raw)).from_buffer(raw)
        self.assertEqual(self.lib.runMemory(data, len(raw), START, END), 1)
        self.assertEqual(self.lib.metric(1), 0)
        for manager, _, _, _ in MANAGERS:
            particles = struct.unpack_from(">I", raw, manager + 4 - 0x80000000)[0]
            field = particles + 1499 * PSTRIDE + 0x4C
            old = raw[field - 0x80000000:field + 4 - 0x80000000]
            struct.pack_into(">I", raw, field - 0x80000000, NONE)
            self.assertEqual(self.lib.runMemory(data, len(raw), START, END), 0)
            raw[field - 0x80000000:field + 4 - 0x80000000] = old
        self.assertEqual(self.lib.runMemory(data, len(raw), START, END), 1)

    def test_actual_old_dolphin_mem1_captures(self):
        paths = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in
                 ("diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]
        available = [path for path in paths if path.exists()]
        if not available:
            self.skipTest("Optional captured MEM1 fixtures unavailable")
        for path in available:
            with self.subTest(capture=path.parent.name):
                raw = bytearray(path.read_bytes())
                self.assertEqual(len(raw), 24 * 1024 * 1024)
                self.check_memory(raw)

    def test_runner_format19_archives_as_read_only_graph_fixtures(self):
        directory = Path(os.environ.get("LM_RUNNER_ARCHIVES",
            ROOT.parent / "sd-captures/lm-0.3.35-runner-20260907"))
        paths = sorted(directory.glob("archive_0000000[12].lms"))
        if not paths:
            self.skipTest("Optional runner archives unavailable; set LM_RUNNER_ARCHIVES")
        for path in paths:
            with self.subTest(archive=path.name):
                archived = path.read_bytes()
                outer = struct.unpack_from(">16I", archived)
                self.assertEqual(outer[:3], (0x4C4D5341, 1, 64))
                self.assertEqual(outer[4:6], (0x474C4D4A, 19))
                self.assertEqual(outer[3], len(archived) - 64)
                payload = archived[64:]
                self.assertEqual(zlib.crc32(payload), outer[10])
                h = struct.unpack_from(">64I", payload)
                self.assertEqual(h[:4], (0x4C4D5354, 19, 256, 0x474C4D4A))
                self.assertEqual(h[8:11], (START, END, END - START))
                self.assertEqual(h[13:17], (0x148, 0x16FF4, 0x17440, END - START))
                self.assertEqual(h[4], outer[8])
                self.assertEqual(h[15] + h[16], h[4])
                raw = bytearray(24 * 1024 * 1024)
                raw[START - 0x80000000:END - 0x80000000] = payload[h[15]:h[4]]
                # Fixed format19 offset of the two grain managers, no code/import.
                raw[0x3CBAF0:0x3CC460] = payload[0xB0D8:0xB0D8 + 0x970]
                self.check_memory(raw)


if __name__ == "__main__":
    unittest.main()
