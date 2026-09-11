"""Execute the bounded GLMJ01 scene-root admission helper, never imported states."""

import ctypes
import hashlib
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
GLOBALS = (0x804A17C8, 0x804A17B0, 0x804A17D0, 0x804A17D8, 0x804A17E8)
VTABLES = (0x8034F080, 0x80388D5C, 0x8034F0CC, 0x8034F180, 0x803560A8, 0x80358D68)
SIZES = (0x24, 0x68, 0xC10, 0x14, 8, 0xE48, 0x31DC0)


class Roots(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ("missionMode", "mapArchive", "gameMode", "simpleModeler", "mapCol", "enTypesManager")]


class ResourceRoots(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ("recordBase", "bulkBase", "slotCount", "slotSize")]


class RootNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-roots-")
        output = Path(cls.temp.name) / ("roots.dll" if os.name == "nt" else "roots.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror", "-I",
                   str(ROOT / "include"), str(ROOT / "scripts/lm_state_roots_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(Roots)] + [ctypes.c_uint] * 3
        cls.lib.run.restype = ctypes.c_int
        cls.lib.resourceRun.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ResourceRoots)] + [ctypes.c_uint] * 3
        cls.lib.resourceRun.restype = ctypes.c_int
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint
        for name in ("maskAllowed", "rootOnly"):
            getattr(cls.lib, name).argtypes = [ctypes.c_uint]
            getattr(cls.lib, name).restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(24 * 1024 * 1024)
        self.make_graph()
        self.make_resources()

    def get(self, address):
        return struct.unpack_from(">I", self.image, address - 0x80000000)[0]

    def put(self, address, value):
        struct.pack_into(">I", self.image, address - 0x80000000, value)

    def make_graph(self, delta=0):
        self.bases = [value + delta for value in
                      (0x81000000, 0x81000100, 0x81000200, 0x81001000,
                       0x81001100, 0x81002000, 0x81100000)]
        m, a, s, c, t, e, table = self.bases
        self.roots = Roots(m, a, m, s, c, t)
        for address, value in zip(GLOBALS, (m, m, s, c, t)):
            self.put(address, value)
        for address, vtable in zip(self.bases, VTABLES):
            self.put(address, vtable)
        self.put(m + 4, t)
        self.put(m + 8, e)
        self.put(m + 0x18, a)
        self.put(t + 4, table + 8)
        self.put(table, 0x218)
        self.put(table + 4, 0x17D)

    def run_case(self, start=START, end=END, fail=0, image_size=None):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        result = self.lib.run(data, len(data) if image_size is None else image_size,
                              ctypes.byref(self.roots), start, end, fail)
        self.assertLess(self.lib.metric(0), 32)
        return result

    def make_resources(self, delta=0):
        record, bulk = 0x80C00008 + delta, 0x80C10000 + delta
        self.resources = ResourceRoots(record, bulk, 7, 0x70800)
        for address, value in ((0x804A0D08, record), (0x804A0D0C, bulk),
                               (0x804A0D10, 7 << 24), (0x804A0D14, 0x70800),
                               (record - 8, 0x40), (record - 4, 7)):
            self.put(address, value)
        for i in range(7):
            self.put(0x80398ECC + i * 4, bulk + i * 0x70800)

    def resource_case(self, start=START, end=END, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        result = self.lib.resourceRun(data, len(data), ctypes.byref(self.resources), start, end, fail)
        self.assertLess(self.lib.metric(0), 20)
        self.assertEqual(self.lib.metric(1), 0)
        return result

    def test_original_and_independently_relocated_graphs(self):
        self.assertEqual(self.run_case(), 1)
        self.make_graph(0x100000)
        self.assertEqual(self.run_case(), 1)
        self.assertEqual(self.lib.metric(1), 0)

    def test_only_documented_root_and_volume_bits_are_admissible(self):
        roots, allowed = 0x7C00, 0x7D80
        self.assertEqual(self.lib.maskAllowed(allowed), 1)
        self.assertEqual(self.lib.rootOnly(roots), 1)
        self.assertEqual(self.lib.rootOnly(allowed), 0)
        for bit in range(32):
            self.assertEqual(self.lib.maskAllowed(1 << bit), int(bool(allowed & (1 << bit))))
            self.assertEqual(self.lib.rootOnly(1 << bit), int(bool(roots & (1 << bit))))
            if not allowed & (1 << bit):
                self.assertEqual(self.lib.maskAllowed(roots | (1 << bit)), 0)
        for mask in (0, 0xFFFFFFFF):
            self.assertEqual(self.lib.maskAllowed(mask), 0)
            self.assertEqual(self.lib.rootOnly(mask), 0)

    def test_null_reader_and_invalid_heap_extents(self):
        self.assertEqual(self.lib.nullReader(), 0)
        for start, end in ((END, START), (START, START), (START - 1, END),
                           (START, END + 1), (0, END), (START, 0xFFFFFFFF)):
            self.assertEqual(self.run_case(start, end), 0)
            self.assertEqual(self.lib.metric(0), 0)

    def test_each_root_requires_complete_aligned_game_extent(self):
        for name, size in zip(("missionMode", "mapArchive", "simpleModeler", "mapCol", "enTypesManager"), SIZES):
            for value in (0, 0xFFFFFFFF, START - 4, END, END - size + 4,
                          getattr(self.roots, name) + 1, 0x80538560):
                old = getattr(self.roots, name)
                setattr(self.roots, name, value)
                self.assertEqual(self.run_case(), 0, (name, hex(value)))
                self.assertEqual(self.lib.metric(0), 0)
                setattr(self.roots, name, old)

    def test_game_mode_is_exact_mission_alias(self):
        self.roots.gameMode = self.roots.mapCol
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.lib.metric(0), 0)

    def test_captured_globals_must_equal_the_header_roots(self):
        for address in GLOBALS:
            old = self.get(address)
            self.put(address, old + 4)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.put(address, old)

    def test_all_six_native_vtables_are_required(self):
        for address in self.bases[:6]:
            old = self.get(address)
            self.put(address, old ^ 4)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.put(address, old)

    def test_mission_child_links_match_captured_owners(self):
        for offset in (4, 0x18):
            address = self.roots.missionMode + offset
            old = self.get(address)
            self.put(address, old + 4)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.put(address, old)

    def test_child_pointers_are_bounded_before_dereference(self):
        for address, size, prefix in ((self.roots.missionMode + 8, 0xE48, 0),
                                      (self.roots.enTypesManager + 4, 0x31DC0, 8)):
            old = self.get(address)
            for value in (0, 4, 0xFFFFFFFF, START - 4, END - size + prefix + 4, END, 0x80538560):
                self.put(address, value)
                self.assertEqual(self.run_case(), 0)
                self.assertEqual(self.lib.metric(2), address)
                self.assertEqual(self.lib.metric(1), 0)
            self.put(address, old)

    def test_complete_table_array_metadata_is_required(self):
        for offset in (0, 4):
            address = self.bases[6] + offset
            old = self.get(address)
            for value in (0, old - 1, old + 1, 0xFFFFFFFF):
                self.put(address, value)
                self.assertEqual(self.run_case(), 0)
            self.put(address, old)

    def test_distinct_owners_and_child_allocations_cannot_overlap(self):
        old = self.roots.mapCol
        self.roots.mapCol = self.roots.simpleModeler + 0xC00
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.lib.metric(0), 0)
        self.roots.mapCol = old
        self.put(self.roots.missionMode + 8, self.roots.simpleModeler + 0xC00)
        self.assertEqual(self.run_case(), 0)
        self.put(self.roots.missionMode + 8, self.bases[5])
        self.put(self.roots.enTypesManager + 4, self.bases[5] - 0x31DC0 + 12)
        self.assertEqual(self.run_case(), 0)

    def test_reader_failure_on_every_read_field_is_not_accepted(self):
        fields = list(GLOBALS) + self.bases[:6] + [self.roots.missionMode + n for n in (4, 8, 0x18)]
        fields += [self.roots.enTypesManager + 4, self.bases[6], self.bases[6] + 4]
        for address in fields:
            self.assertEqual(self.run_case(fail=address), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.assertEqual(self.lib.metric(3), 0)

    def test_resource_schema_can_relocate_with_all_roots_and_backings(self):
        self.assertEqual(self.resource_case(), 1)
        self.make_resources(0x80000)
        self.assertEqual(self.resource_case(), 1)

    def test_resource_shape_changes_are_not_address_relocation(self):
        for field in ("slotCount", "slotSize"):
            old = getattr(self.resources, field)
            for value in (0, old - 1, old + 1, 0xFFFFFFFF):
                setattr(self.resources, field, value)
                self.assertEqual(self.resource_case(), 0)
                self.assertEqual(self.lib.metric(0), 0)
            setattr(self.resources, field, old)

    def test_resource_ranges_include_array_prefix_and_complete_bulk(self):
        for field, size, prefix in (("recordBase", 7 * 0x40 + 8, 8),
                                   ("bulkBase", 7 * 0x70800, 0)):
            old = getattr(self.resources, field)
            for value in (0, 4, 0xFFFFFFFF, START + prefix - 4, END,
                          END - size + prefix + 4, old + 1, 0x80538560):
                setattr(self.resources, field, value)
                self.assertEqual(self.resource_case(), 0)
                self.assertEqual(self.lib.metric(0), 0)
            setattr(self.resources, field, old)
        self.resources.recordBase = self.resources.bulkBase + 4
        self.assertEqual(self.resource_case(), 0)
        self.assertEqual(self.lib.metric(0), 0)

    def test_resource_metadata_and_each_backing_must_match(self):
        fields = [0x804A0D08, 0x804A0D0C, 0x804A0D10, 0x804A0D14,
                  self.resources.recordBase - 8, self.resources.recordBase - 4]
        fields += [0x80398ECC + i * 4 for i in range(7)]
        for address in fields:
            old = self.get(address)
            self.put(address, old ^ 0xFF000001)
            self.assertEqual(self.resource_case(), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.put(address, old)
            self.assertEqual(self.resource_case(fail=address), 0)
            self.assertEqual(self.lib.metric(2), address)
            self.assertEqual(self.lib.metric(3), 0)

    def test_resource_slot_byte_does_not_claim_unrelated_padding(self):
        self.put(0x804A0D10, 0x07AABBCC)
        self.assertEqual(self.resource_case(), 1)

    def use_image_resources(self):
        self.resources = ResourceRoots(self.get(0x804A0D08), self.get(0x804A0D0C),
                                       self.get(0x804A0D10) >> 24, self.get(0x804A0D14))

    def use_image_roots(self):
        m = self.get(0x804A17C8)
        self.roots = Roots(m, self.get(m + 0x18), self.get(0x804A17B0),
            self.get(0x804A17D0), self.get(0x804A17D8), self.get(0x804A17E8))

    def test_optional_original_dolphin_captures(self):
        paths = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in
                 ("diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]
        paths = [path for path in paths if path.exists()]
        if not paths:
            self.skipTest("Optional Dolphin captures absent")
        for path in paths:
            self.image = bytearray(path.read_bytes())
            self.assertEqual(len(self.image), 24 * 1024 * 1024)
            self.use_image_roots()
            self.assertEqual(self.run_case(), 1, path.name)
            self.use_image_resources()
            self.assertEqual(self.resource_case(), 1, path.name)

    def test_optional_runner_archives_are_only_read_as_graph_fixtures(self):
        directory = Path(os.environ.get("LM_RUNNER_ARCHIVES",
            ROOT.parent / "sd-captures/lm-0.3.35-runner-20260907"))
        paths = sorted(directory.glob("archive_0000000[12].lms"))
        if not paths:
            self.skipTest("Optional runner archives absent")
        for path in paths:
            archived = path.read_bytes()
            outer = struct.unpack_from(">16I", archived)
            payload = archived[64:]
            self.assertEqual(outer[:3], (0x4C4D5341, 1, 64))
            self.assertEqual(outer[3:6], (len(payload), 0x474C4D4A, 19))
            self.assertEqual(zlib.crc32(payload), outer[10])
            h = struct.unpack_from(">64I", payload)
            self.assertEqual(h[:4], (0x4C4D5354, 19, 256, 0x474C4D4A))
            self.assertEqual(h[8:11], (START, END, END - START))
            self.assertEqual(h[13:17], (0x148, 0x16FF4, 0x17440, END - START))
            self.assertEqual(h[4], h[15] + h[16])
            self.image = bytearray(24 * 1024 * 1024)
            self.image[START - 0x80000000:END - 0x80000000] = payload[h[15]:h[4]]
            # All five fixed roots are in the final format19 captured SBSS range.
            sbss_offset = h[13] + h[14] - 20 - (0x804A1D10 - 0x804A0CB0)
            self.image[0x4A0CB0:0x4A1D10] = payload[sbss_offset:sbss_offset + 0x1060]
            resource_offset = h[13] + sum((0x14, 0xC, 0x270, 0x58, 0x620, 0x100, 262 * 0x34, 262 * 0x40))
            self.image[0x398C50:0x398FC8] = payload[resource_offset:resource_offset + 0x378]
            self.use_image_roots()
            self.assertEqual(self.run_case(), 1, path.name)
            self.use_image_resources()
            self.assertEqual(self.resource_case(), 1, path.name)


class AuthenticatedRootAllocationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get("LM_CLEAN_DOL", ROOT / "build-lm-diag/clean_glmj_main.dol"))
        if not path.exists():
            raise unittest.SkipTest("Optional clean Japanese DOL absent")
        cls.raw = path.read_bytes()
        if hashlib.sha1(cls.raw).hexdigest() != "722005ea9c1eab54b114f814734d8f327e5614ee":
            raise AssertionError("Not the authenticated retail Japanese DOL")
        cls.sections = []
        for count, offbase, addrbase, sizebase in ((7, 0, 0x48, 0x90), (11, 0x1C, 0x64, 0xAC)):
            for i in range(count):
                off, addr, size = (struct.unpack_from(">I", cls.raw, base + i * 4)[0]
                                   for base in (offbase, addrbase, sizebase))
                if size:
                    cls.sections.append((addr, addr + size, off))

    def words(self, expected):
        for address, word in expected.items():
            entries = [(a, o) for a, b, o in self.sections if a <= address and address + 4 <= b]
            self.assertEqual(len(entries), 1)
            base, offset = entries[0]
            self.assertEqual(struct.unpack_from(">I", self.raw, offset + address - base)[0], word,
                             f"retail {address:08X}")

    def test_mission_game_mode_size_vtable_and_alias(self):
        self.words({0x800B9220: 0x38600024, 0x800B9238: 0x93ED0CE8,
                    0x800B8430: 0x48000DD5, 0x800B8434: 0x906D0CD0,
                    0x800B8FF0: 0x3C608035, 0x800B8FF8: 0x3803F080,
                    0x800B8FFC: 0x901F0000, 0x800B9034: 0x901F0004,
                    0x800B9054: 0x93E40008})

    def test_map_archive_owner_has_bounded_native_size_and_vtable(self):
        self.words({0x80006770: 0x38600068, 0x80006774: 0x38800004,
                    0x80006778: 0x4BFFF81D, 0x801CEA54: 0x3C608039,
                    0x801CEA58: 0x38038D5C, 0x801CEA5C: 0x901F0000})

    def test_simple_mapcol_and_types_native_extents_vtables_globals(self):
        self.words({0x800B9D04: 0x38600C10, 0x800B9D24: 0x3C608035,
                    0x800B9D28: 0x3803F0CC, 0x800B9D48: 0x93CD0CF0,
                    0x800BA1D0: 0x38600014, 0x800BA1E8: 0x3C808035,
                    0x800BA1EC: 0x3804F180, 0x800BA214: 0x906D0CF8,
                    0x800D90DC: 0x38600008, 0x800D90F8: 0x3C608035,
                    0x800D90FC: 0x380360A8, 0x800D9134: 0x93ED0D08})

    def test_child_extents_and_array_metadata(self):
        self.words({0x800B9030: 0x38600E48, 0x800E2C90: 0x3C608036,
                    0x800E2C98: 0x38038D68, 0x800E2CA0: 0x901F0000,
                    0x800D9108: 0x3C600003, 0x800D9110: 0x38631DC0,
                    0x800D9124: 0x38C00218, 0x800D9128: 0x38E0017D,
                    0x800D9130: 0x907F0004, 0x801F5528: 0x939E0000,
                    0x801F5530: 0x93BE0004, 0x801F5534: 0x3BDE0008})
        self.assertEqual(381 * 0x218 + 8, 0x31DC0)

    def test_scene_resource_arrays_are_reallocated_with_fixed_shape(self):
        self.words({0x8001245C: 0x3C600007, 0x80012470: 0x38830800,
                    0x80012474: 0x38600007, 0x80012478: 0x4BFFF219,
                    0x800116E4: 0x57C43032, 0x800116E8: 0x38640008,
                    0x80011704: 0x38C00040, 0x80011714: 0x906D0228,
                    0x80011718: 0x7C6401D6, 0x8001171C: 0x38800020,
                    0x80011720: 0x4BFF4875, 0x8001172C: 0x906D022C,
                    0x800141C0: 0x806D022C, 0x800141C4: 0x4BFF1E89,
                    0x80014234: 0x806D0228, 0x8001423C: 0x481E101D})


if __name__ == "__main__":
    unittest.main()
