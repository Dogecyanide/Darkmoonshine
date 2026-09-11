"""Exact X05 predicate equivalence and reason/endpoint telemetry."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VTABLE = 0x80388D5C
START, END = 0x80BE4560, 0x817FB140


class Entry(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ("node", "object", "vtable", "stateFlags", "archiveHeader", "fileLength", "ownerFlags")]


class Parent(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
               ("owner", "base", "size", "systemHeap", "systemStart", "systemEnd",
                "parentBlockTag", "ownerBlockTag", "usedSignature", "freeSignature")]


class ArchiveGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler required")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-archive-guard-")
        output = Path(cls.temp.name) / ("guard.dll" if os.name == "nt" else "guard.so")
        command = [compiler, "-shared", "-O2", "-std=gnu89", "-Wall", "-Werror", "-I", str(ROOT / "include"),
                   str(ROOT / "scripts/lm_archive_guard_harness.c"), "-o", str(output)]
        if os.name != "nt": command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        result = subprocess.run(command, capture_output=True, env=env)
        if result.returncode: raise RuntimeError(result.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.reasons.argtypes = [ctypes.POINTER(Entry)] + [ctypes.c_uint] * 3
        cls.lib.reasons.restype = ctypes.c_uint
        cls.lib.failure_value.argtypes = [ctypes.POINTER(Entry), ctypes.c_uint]
        cls.lib.failure_value.restype = ctypes.c_uint
        cls.lib.shared_backing.argtypes = [ctypes.POINTER(Entry)] + [ctypes.c_uint] * 5 + [ctypes.POINTER(Parent)]

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    @staticmethod
    def entry(**changes):
        values = dict(node=0x8157C118, object=0x8157C100, vtable=VTABLE,
                      stateFlags=0x01010107, archiveHeader=0x815A0440,
                      fileLength=147648, ownerFlags=0x1111)
        values.update(changes)
        return Entry(**values)

    def check(self, entry, start=START, end=END):
        return self.lib.reasons(ctypes.byref(entry), start, end, VTABLE)

    @staticmethod
    def old_predicate(entry, start=START, end=END):
        size, address = entry.fileLength, entry.archiveHeader
        is_mem1_bytes = size <= 0x1800000 and address >= 0x80000000 and address <= 0x81800000 - size
        in_game = size != 0 and is_mem1_bytes and address >= start and address + size <= end
        return (entry.vtable == VTABLE and entry.node == (entry.object + 0x18) & 0xFFFFFFFF and
                (entry.ownerFlags >> 8) & 15 == 1 and (entry.ownerFlags >> 12) & 15 == 1 and
                entry.stateFlags & 0x106 == 0x106 and size >= 0x20 and in_game)

    def test_individual_reasons_and_exact_endpoint(self):
        cases = [({"vtable": VTABLE + 4}, 1, VTABLE + 4),
                 ({"node": 0x8157C11C}, 2, 0x8157C11C),
                 ({"ownerFlags": 0x1211}, 4, 0x8157C100),
                 ({"ownerFlags": 0x2111}, 8, 0x815A0440),
                 ({"stateFlags": 0x102}, 16, 0x102),
                 ({"fileLength": 31}, 32, 31),
                 ({"archiveHeader": START - 32}, 64, START - 32)]
        for changes, expected, value in cases:
            with self.subTest(changes=changes):
                entry = self.entry(**changes)
                self.assertEqual(self.check(entry), expected)
                self.assertEqual(self.lib.failure_value(ctypes.byref(entry), expected), value)

    def test_all_reasons_accumulate_but_first_endpoint_is_stable(self):
        entry = self.entry(vtable=1, node=2, ownerFlags=0, stateFlags=0, fileLength=0, archiveHeader=3)
        self.assertEqual(self.check(entry), 0x7F)
        for mask, value in ((0x7F, 1), (0x7E, 2), (0x7C, entry.object), (0x78, 3),
                            (0x70, 0), (0x60, 0), (0x40, 3), (0, 0)):
            self.assertEqual(self.lib.failure_value(ctypes.byref(entry), mask), value)

    def test_allocator_pointer_owner_nibbles_remain_irrelevant(self):
        for low in range(256):
            self.assertEqual(self.check(self.entry(ownerFlags=0xFFFF1100 | low)), 0)

    def test_backing_range_boundaries_and_overflow(self):
        for address, size, valid in ((START, 32, True), (END - 32, 32, True),
                                     (END - 31, 32, False), (START - 1, 32, False),
                                     (0x80000000, 0x1800000, False), (0x817FFFF0, 32, False),
                                     (0xFFFFFFFF, 0xFFFFFFFF, False), (0, 0, False)):
            entry = self.entry(archiveHeader=address, fileLength=size)
            self.assertEqual(self.check(entry) == 0, valid)
        self.assertEqual(self.check(self.entry(archiveHeader=0x80000000, fileLength=0x1800000),
                                    0x80000000, 0x81800000), 0)

    def test_preserves_prior_byte_range_not_new_alignment_policy(self):
        self.assertEqual(self.check(self.entry(archiveHeader=0x815A0441)), 0)
        self.assertEqual(self.check(self.entry(stateFlags=0xFFFFFFFF)), 0)
        self.assertEqual(self.check(self.entry()), 0)

    def test_observed_system_backed_endpoint_class_still_refuses(self):
        # Independently exported .35 census tuple, not a paired refusal endpoint.
        luige = self.entry(node=0x80E6C2C8, object=0x80E6C2B0,
                           archiveHeader=0x80928540, fileLength=0x251600, ownerFlags=0x2121)
        self.assertEqual(self.check(luige), 0x48)
        self.assertEqual(self.lib.failure_value(ctypes.byref(luige), 0x48), 0x80928540)
        data = self.entry(node=0x80BD4B08, object=0x80BD4AF0,
                          archiveHeader=0x807BAFE0, fileLength=0x419B00, ownerFlags=0x2222)
        self.assertEqual(self.check(data), 0x4C)

    def test_randomized_exact_old_predicate_equivalence(self):
        rng = random.Random(0x583035)
        fields = [name for name, _ in Entry._fields_]
        for _ in range(20000):
            entry = self.entry()
            for __ in range(rng.randrange(1, 8)):
                field = rng.choice(fields)
                value = rng.choice([rng.getrandbits(32), getattr(entry, field) ^ (1 << rng.randrange(32))])
                setattr(entry, field, value)
            start, end = rng.choice([(START, END), (0x80000000, 0x81800000),
                                     (0x815A0000, 0x815E0000), (END, START)])
            self.assertEqual(self.check(entry, start, end) == 0, self.old_predicate(entry, start, end))


    @staticmethod
    def parent(**changes):
        values = dict(owner=0x80BD4AF0, base=0x807BAFE0, size=0x419B00,
                      systemHeap=0x805384C0, systemStart=0x80538550, systemEnd=0x80BE44C0)
        values.update(changes)
        return Parent(**values)

    def nested(self, **changes):
        values = dict(node=0x80E6C2C8, object=0x80E6C2B0,
                      archiveHeader=0x80928540, fileLength=0x251600, ownerFlags=0x2121)
        values.update(changes)
        return self.entry(**values)

    def shared(self, entry, parent=None, **changes):
        parent = parent or self.parent()
        fields = dict(object_owner=0x80BE44D0, archive_heap=parent.systemHeap,
                      resource_type=0x52415243, mount=entry.archiveHeader, game_heap=0x80BE44D0)
        fields.update(changes)
        return self.lib.shared_backing(ctypes.byref(entry), *fields.values(), ctypes.byref(parent))

    def test_native_nested_path_only_clears_backing_reasons(self):
        entry = self.nested()
        self.assertEqual(self.check(entry), 0x48)
        self.assertEqual(self.shared(entry), 1)
        self.assertEqual(self.check(entry) & ~0x48, 0)
        for changed, remaining in (({"vtable": 0}, 1), ({"node": 0}, 2), ({"stateFlags": 0x01010000}, 16)):
            broken = self.nested(**changed)
            self.assertEqual(self.check(broken) & ~0x48, remaining)

    def test_nested_owner_allocator_type_mount_mode_and_free_flag_are_exact(self):
        entry = self.nested()
        for changes in ({"object_owner": 0x805384C0}, {"archive_heap": 0x80BE44D0},
                        {"resource_type": 0}, {"mount": 0}, {"game_heap": 0x805384C0}):
            with self.subTest(changes=changes):
                self.assertEqual(self.shared(entry, **changes), 0)
        for flags in (0x01000107, 0x01020107, 0x00010107):
            self.assertEqual(self.shared(self.nested(stateFlags=flags)), 0)
        for nibble in range(4):
            for value in range(16):
                flags = (0x2121 & ~(15 << (nibble * 4))) | (value << (nibble * 4))
                self.assertEqual(self.shared(self.nested(ownerFlags=flags)), int(flags == 0x2121))

    def test_parent_headers_tables_allocators_and_external_backings_stay_forbidden(self):
        parent = self.parent()
        start, end = parent.base + 0x6E20, parent.base + parent.size
        for base, size, valid in ((start, 32, True), (end - 32, 32, True),
                                  (parent.base, 32, False), (start - 32, 32, False),
                                  (end, 32, False), (end - 32, 64, False),
                                  (parent.owner, 32, False), (parent.base - 32, 32, False),
                                  (0xFFFFFFFF, 0xFFFFFFFF, False), (start + 1, 32, False),
                                  (start, 31, False), (start, 0, False)):
            with self.subTest(base=hex(base), size=size):
                self.assertEqual(self.shared(self.nested(archiveHeader=base, fileLength=size)), int(valid))
        self.assertEqual(self.shared(self.nested(), self.parent(size=0x419B20)), 0)
        self.assertEqual(self.shared(self.nested(), self.parent(base=0x817FFFF0)), 0)
        parent_owner = self.nested(node=parent.owner + 0x18, object=parent.owner,
                                   archiveHeader=parent.base, fileLength=parent.size, ownerFlags=0x2222)
        self.assertEqual(self.shared(parent_owner), 0)


if __name__ == "__main__":
    unittest.main()
