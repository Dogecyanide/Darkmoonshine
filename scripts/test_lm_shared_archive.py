"""Run the actual bounded shared-resource validator against private MEM1 fixtures."""

import ctypes
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SIZE = 0x419B00
FIXTURES = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in
            ("diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]


class Descriptor(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ("owner", "base", "size", "systemHeap", "systemStart", "systemEnd",
                 "parentBlockTag", "ownerBlockTag", "usedSignature", "freeSignature")]


class SharedArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-shared-")
        output = Path(cls.temp.name) / ("shared.dll" if os.name == "nt" else "shared.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror", "-I",
                   str(ROOT / "include"), str(ROOT / "scripts/lm_shared_archive_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        compiled = subprocess.run(command, capture_output=True, env=env)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]
        cls.lib.run.restype = ctypes.c_int
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint
        cls.lib.descriptor.argtypes = [ctypes.POINTER(Descriptor)]
        cls.lib.contains.argtypes = [ctypes.POINTER(Descriptor), ctypes.c_uint, ctypes.c_uint]
        cls.lib.contains.restype = ctypes.c_int
        cls.lib.same.argtypes = [ctypes.POINTER(Descriptor), ctypes.POINTER(Descriptor)]
        cls.lib.same.restype = ctypes.c_int
        cls.fixture = next((p.read_bytes() for p in FIXTURES if p.exists()), None)

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(self.fixture or bytes(24 * 1024 * 1024))

    def require_fixture(self):
        if self.fixture is None:
            self.skipTest("Optional private MEM1 fixture needed for complete native ownership graph")
        self.assertEqual(len(self.image), 24 * 1024 * 1024)
        self.heap = self.get(0x804A0B94)
        self.owner = self.get(0x804A12B0)
        self.base = self.get(self.owner + 0x5C)
        self.assertEqual(self.run_case(), 1)

    def get(self, address):
        return struct.unpack_from(">I", self.image, address - 0x80000000)[0]

    def put(self, address, value):
        struct.pack_into(">I", self.image, address - 0x80000000, value)

    def output(self):
        result = Descriptor()
        self.lib.descriptor(ctypes.byref(result))
        return result

    def run_case(self, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        answer = self.lib.run(data, len(data), fail)
        self.assertLess(self.lib.metric(0), 10000)
        self.assertEqual(self.lib.metric(1), 0, "foreign callback read")
        if not answer:
            self.assertEqual(bytes(self.output()), bytes(40), "refusal exposed partial descriptor")
        return answer

    def change_refuses(self, address, values):
        old = self.get(address)
        for value in values:
            self.put(address, value)
            self.assertEqual(self.run_case(), 0, (hex(address), hex(value)))
        self.put(address, old)

    def test_descriptor_is_exact40bytes(self):
        self.assertEqual(ctypes.sizeof(Descriptor), 40)

    def test_null_reader_refuses_and_clears_output(self):
        self.assertEqual(self.lib.nullReader(), 0)
        self.assertEqual(bytes(self.output()), bytes(40))

    def test_untrusted_global_pointers_are_not_followed_outside_mem1(self):
        for value in (0, 0xFFFFFFFF, 0x7FFFFFFC, 0x817FFFFC, 0x80000001):
            self.put(0x804A0B94, value)
            self.assertEqual(self.run_case(), 0)
            self.assertEqual(self.lib.metric(0), 1)

    def test_containment_is_inside_data_not_owner_header_or_sys_in_general(self):
        parent = Descriptor(0x80BD4AF0, 0x807BAFE0, SIZE, 0x805384C0)
        self.assertEqual(self.lib.contains(ctypes.byref(parent), 0x809132E0, 0x2AAAC0), 1)
        self.assertEqual(self.lib.contains(ctypes.byref(parent), 0x80BD4AC0, 0x20), 1)
        for base, size in ((parent.base, 0x20), (parent.base - 0x20, 0x20),
                           (0x80BD4AE0, 0x20), (0x80BD4AF0, 0x68),
                           (0x809132E1, 0x20), (0x809132E0, 0),
                           (0x809132E0, 0x21), (0x809132E0, 0xFFFFFFFF),
                           (0xFFFFFFFF, 0x20), (0x80545300, 0x80000)):
            self.assertEqual(self.lib.contains(ctypes.byref(parent), base, size), 0)
        parent.size -= 32
        self.assertEqual(self.lib.contains(ctypes.byref(parent), 0x809132E0, 0x20), 0)
        parent.size = SIZE
        parent.base = 0xFFF00000
        self.assertEqual(self.lib.contains(ctypes.byref(parent), 0xFFF10000, 0x20), 0)

    def test_identity_requires_every_descriptor_word_and_valid_kind(self):
        a = Descriptor(0x80BD4AF0, 0x807BAFE0, SIZE, 0x805384C0,
                       0x80538550, 0x80BE44C0, 0x484D0C10, 0x484D0010, 1, 2)
        b = Descriptor.from_buffer_copy(a)
        self.assertEqual(self.lib.same(ctypes.byref(a), ctypes.byref(b)), 1)
        for name, _ in Descriptor._fields_:
            old = getattr(b, name)
            setattr(b, name, old ^ 4)
            self.assertEqual(self.lib.same(ctypes.byref(a), ctypes.byref(b)), 0)
            setattr(b, name, old)
        a.size = b.size = 0
        self.assertEqual(self.lib.same(ctypes.byref(a), ctypes.byref(b)), 0)

    def test_both_actual_captures_have_same_identity_despite_mutable_content(self):
        self.require_fixture()
        results, backing_hashes = [], []
        for path in FIXTURES:
            if not path.exists():
                continue
            self.image = bytearray(path.read_bytes())
            self.assertEqual(self.run_case(), 1)
            result = self.output()
            self.assertEqual((result.owner, result.base, result.size), (0x80BD4AF0, 0x807BAFE0, SIZE))
            self.assertEqual((result.parentBlockTag, result.ownerBlockTag), (0x484D0C10, 0x484D0010))
            results.append(bytes(result))
            backing_hashes.append(hashlib.sha256(self.image[0x7BAFE0:0xBD4AE0]).digest())
        self.assertTrue(all(value == results[0] for value in results))
        if len(backing_hashes) == 2:
            self.assertNotEqual(*backing_hashes)

    def test_sys_header_and_exact_bounds(self):
        self.require_fixture()
        self.change_refuses(self.heap, (0, 0x80388D5C))
        self.change_refuses(self.heap + 0x30, (0, self.heap, self.heap + 0x87, 0xFFFFFFFF))
        self.change_refuses(self.heap + 0x34, (0, 0xFFFFFFFF, 0x81800004, self.base))
        self.change_refuses(self.heap + 0x38, (0, 0xFFFFFFFF, self.get(self.heap + 0x38) + 4))

    def test_sys_owner_and_backing_require_complete_owned_ranges(self):
        self.require_fixture()
        self.change_refuses(0x804A12B0, (0, 0xFFFFFFFF, 0x80BE44A0, self.base, self.owner + 1))
        self.change_refuses(self.owner + 0x5C, (0, 0xFFFFFFFF, 0x80BE44A0, self.owner, self.base + 4))

    def test_all_owner_identity_fields_and_table_roots(self):
        self.require_fixture()
        fields = (0, 4, 8, 0xC, 0x18, 0x1C, 0x28, 0x2C, 0x34, 0x38,
                  0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x60)
        for offset in fields:
            address = self.owner + offset
            self.change_refuses(address, (self.get(address) ^ 4,))
        for offset in (0x30, 0x3C, 0x64):
            self.change_refuses(self.owner + offset, (self.get(self.owner + offset) ^ 0xFF000000,))
        self.put(self.owner + 0x30, 0x01AABBCC)
        self.put(self.owner + 0x3C, 0x01DDEEFF)
        self.put(self.owner + 0x64, 0x00112233)
        self.assertEqual(self.run_case(), 1)

    def test_header_metadata_is_exact_but_payload_and_fetch_cache_are_mutable(self):
        self.require_fixture()
        for offset in range(0, 0x40, 4):
            self.change_refuses(self.base + offset, (self.get(self.base + offset) ^ 1,))
        for offset in (0x40, 0x44C0, 0x1E0, 0x1E0 + 855 * 20 + 12):
            self.change_refuses(self.base + offset, (self.get(self.base + offset) ^ 1,))
        # Revalidate after the last rejected mutation before comparing output.
        self.assertEqual(self.run_case(), 1)
        before = bytes(self.output())
        self.put(self.base + 0x1E0 + 0x10, 0xDEADBEEF)
        self.put(self.base + 0x6E20, 0xAABBCCDD)
        self.put(self.base + SIZE - 4, 0x11223344)
        self.assertEqual(self.run_case(), 1)
        self.assertEqual(bytes(self.output()), before)

    def test_targets_must_be_in_used_list_with_exact_sizes_and_groups(self):
        self.require_fixture()
        for address, size in ((self.base - 16, SIZE), (self.owner - 16, 0x68)):
            self.change_refuses(address, (0, 0x484D0009))
            self.change_refuses(address + 4, (0, size - 4, size + 4, 0xFFFFFFFF))
        predecessor = self.get(self.base - 8)
        successor = self.get(self.base - 4)
        self.put(predecessor + 12, successor)
        self.put(successor + 8, predecessor)
        self.assertEqual(self.run_case(), 0)

    def test_used_and_free_list_links_and_tail_are_checked(self):
        self.require_fixture()
        for offset in (0x74, 0x78, 0x7C, 0x80):
            self.change_refuses(self.heap + offset, (0xFFFFFFFF, self.get(self.heap + offset) + 4))
        for offset in (0x74, 0x7C):
            head = self.get(self.heap + offset)
            self.change_refuses(head + 8, (head, 0xFFFFFFFF))
            self.change_refuses(head + 12, (head, 0xFFFFFFFF))
            self.change_refuses(head + 4, (0xFFFFFFFF,))

    def test_other_allocations_cannot_overlap_parent_or_owner(self):
        self.require_fixture()
        previous = self.get(self.base - 8)
        self.change_refuses(previous + 4, (self.base - previous,))
        free_tail = self.get(self.heap + 0x78)
        # Place a valid free-list head inside the parent's payload.
        fake = self.base + 0x8000
        self.put(fake, 0)
        self.put(fake + 4, 0x20)
        self.put(fake + 8, 0)
        self.put(fake + 12, free_tail)
        self.put(free_tail + 8, fake)
        self.put(self.heap + 0x74, fake)
        self.assertEqual(self.run_case(), 0)

    def test_bounded_list_never_walks_unlimited_valid_fake_headers(self):
        self.require_fixture()
        nodes = [0x80600000 + i * 0x20 for i in range(129)]
        for i, node in enumerate(nodes):
            self.put(node, 0)
            self.put(node + 4, 0)
            self.put(node + 8, nodes[i - 1] if i else 0)
            self.put(node + 12, nodes[i + 1] if i + 1 < len(nodes) else 0)
        self.put(self.heap + 0x74, nodes[0])
        self.put(self.heap + 0x78, nodes[-1])
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.lib.metric(3), nodes[-1])

    def test_failed_reads_in_every_class_clear_output(self):
        self.require_fixture()
        addresses = [0x804A0B94, 0x804A12B0, self.heap, self.heap + 0x30,
                     self.heap + 0x34, self.heap + 0x38, self.base - 16,
                     self.base - 12, self.owner - 8, self.owner - 4,
                     self.base, self.base + 0x44C0, self.base + 0x1E0]
        addresses += [self.owner + i for i in range(0, 0x68, 4) if i not in (0x10, 0x14, 0x20, 0x24)]
        for address in addresses:
            self.assertEqual(self.run_case(fail=address), 0, hex(address))
            self.assertEqual(self.lib.metric(2), address)
            self.assertEqual(self.lib.metric(3), 0)


class CleanArchiveShapeTests(unittest.TestCase):
    def test_expected_metadata_is_from_the_authenticated_japanese_disc(self):
        path = Path(os.environ.get("LM_CLEAN_ISO",
                    "X:/Games and Isos/GC and WII/luigi's mansion (japan).iso"))
        if not path.exists():
            self.skipTest("Optional clean Japanese ISO absent; set LM_CLEAN_ISO")
        with path.open("rb") as stream:
            self.assertEqual(stream.read(6), b"GLMJ01")
            stream.seek(0x420)
            dol_offset, fst_offset, fst_size = struct.unpack(">III", stream.read(12))
            self.assertLessEqual(fst_size, 0x200000)
            stream.seek(dol_offset)
            header = stream.read(0x100)
            offsets = struct.unpack_from(">18I", header, 0)
            sizes = struct.unpack_from(">18I", header, 0x90)
            dol_size = max([0x100] + [a + b for a, b in zip(offsets, sizes)])
            self.assertLessEqual(dol_size, 0x800000)
            stream.seek(dol_offset)
            self.assertEqual(hashlib.sha1(stream.read(dol_size)).hexdigest(),
                             "722005ea9c1eab54b114f814734d8f327e5614ee")
            stream.seek(fst_offset)
            fst = stream.read(fst_size)
            count = struct.unpack_from(">I", fst, 8)[0]
            self.assertLessEqual(count * 12, len(fst))
            strings, stack, found = fst[count * 12:], [], None
            for index in range(1, count):
                tag, offset, size = struct.unpack_from(">III", fst, index * 12)
                while stack and index >= stack[-1][1]:
                    stack.pop()
                name_offset = tag & 0xFFFFFF
                name = strings[name_offset:strings.index(0, name_offset)].decode("ascii")
                name = "/".join([item[0] for item in stack] + [name])
                if tag >> 24:
                    stack.append((name.rsplit("/", 1)[-1], size))
                elif name == "Game/game.szp":
                    found = (offset, size)
            self.assertIsNotNone(found)
            self.assertLessEqual(found[1], 0x800000)
            stream.seek(found[0])
            source = stream.read(found[1])
        self.assertEqual(source[:4], b"Yay0")
        length, links, chunks = struct.unpack_from(">III", source, 4)
        self.assertEqual(length, SIZE)
        archive, cursor, mask, remaining = bytearray(), 16, 0, 0
        while len(archive) < length:
            if not remaining:
                mask = struct.unpack_from(">I", source, cursor)[0]
                cursor += 4
                remaining = 32
            if mask & 0x80000000:
                archive.append(source[chunks])
                chunks += 1
            else:
                link = struct.unpack_from(">H", source, links)[0]
                links += 2
                run = link >> 12
                if not run:
                    run = source[chunks] + 18
                    chunks += 1
                else:
                    run += 2
                start = len(archive) - (link & 0xFFF) - 1
                self.assertGreaterEqual(start, 0)
                self.assertLessEqual(len(archive) + run, length)
                for index in range(run):
                    archive.append(archive[start + index])
            mask = (mask << 1) & 0xFFFFFFFF
            remaining -= 1
        self.assertEqual(struct.unpack_from(">16I", archive),
            (0x52415243, SIZE, 0x20, 0x6E00, 0x412CE0, 0x412CE0, 0, 0,
             0x19, 0x20, 0x358, 0x1C0, 0x2960, 0x44A0, 0x03580100, 0))
        spans = [(0x40, 25 * 16), (0x44C0, 0x2960)]
        spans += [(0x1E0 + i * 20, 16) for i in range(856)]
        signature = 2166136261
        for offset, length in spans:
            for address in range(offset, offset + length, 4):
                signature = ((signature ^ struct.unpack_from(">I", archive, address)[0]) * 16777619) & 0xFFFFFFFF
        self.assertEqual(signature, 0x94D08ED2)


if __name__ == "__main__":
    unittest.main()
