"""Scene-owned GX targets must rewind with their GAME allocations."""

import ctypes
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
OWNER = 0x803C4B6C
SIZE = 0x114
FIXTURES = [ROOT / "build-lm-emu" / name / "mem1.bin" for name in
            ("diagnostic-capture-0.3.30", "diagnostic-capture-0.3.30-newreport")]


class RenderTargetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if not compiler and Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise unittest.SkipTest("Native C compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="lm-render-targets-")
        output = Path(cls.temp.name) / ("targets.dll" if os.name == "nt" else "targets.so")
        command = [compiler, "-shared", "-O2", "-std=c99", "-Wall", "-Werror", "-I",
                   str(ROOT / "include"), str(ROOT / "scripts/lm_render_targets_harness.c"),
                   "-o", str(output)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        env = os.environ.copy()
        env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
        compiled = subprocess.run(command, capture_output=True, env=env)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr.decode(errors="replace"))
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.run.argtypes = [ctypes.c_void_p] + [ctypes.c_uint] * 6
        cls.lib.run.restype = ctypes.c_int
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(24 * 1024 * 1024)
        self.start, self.end = 0x80BE4560, 0x817FB140
        self.buffers = [self.start + 0x1000, self.start + 0x1000 + 0x96020]
        self.head, self.tail = self.start + 0x100, self.buffers[1] - 16
        nodes = [self.head] + [p - 16 for p in self.buffers]
        for i, (node, size) in enumerate(zip(nodes, (0x40, 0x96000, 0x20000))):
            for offset, value in enumerate((0x484D000D, size,
                                           nodes[i - 1] if i else 0,
                                           nodes[i + 1] if i < 2 else 0)):
                self.put(node + 4 * offset, value)
        for i, (width, height) in enumerate(((640, 480), (256, 256))):
            obj = OWNER + 0x60 + 0x20 * i
            self.put(OWNER + 0xA8 + i * 4, self.buffers[i])
            self.put(obj + 8, 0x89000000 | (3 << 20) | ((height - 1) << 10) | (width - 1))
            self.put(obj + 12, 0x95000000 | ((self.buffers[i] >> 5) & 0x1FFFFF))
            self.put(obj + 20, 3)
            self.put(obj + 28, ((width // 4) * (height // 4) << 16) | 0x0202)

    def get(self, address):
        return struct.unpack_from(">I", self.image, address - 0x80000000)[0]

    def put(self, address, value):
        struct.pack_into(">I", self.image, address - 0x80000000, value)

    def run_case(self, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        answer = self.lib.run(data, len(data), self.start, self.end,
                              self.head, self.tail, fail)
        self.assertLess(self.lib.metric(0), 4 * 8192 + 20)
        self.assertEqual(self.lib.metric(1), 0, "followed an out-of-range pointer")
        if answer:
            self.assertEqual(self.lib.metric(2), 0)
            self.assertEqual(self.lib.metric(3), 0)
        return answer

    def refuses_values(self, address, values):
        original = self.get(address)
        for value in values:
            with self.subTest(address=hex(address), value=hex(value)):
                self.put(address, value)
                self.assertEqual(self.run_case(), 0)
        self.put(address, original)

    def test_valid_endpoint_is_read_only(self):
        before = hashlib.sha256(self.image).digest()
        self.assertEqual(self.run_case(), 1)
        self.assertEqual(hashlib.sha256(self.image).digest(), before)

    def test_null_reader_fails(self):
        self.assertEqual(self.lib.nullReader(), 0)

    def test_buffers_require_bounded_aligned_game_payloads(self):
        for i in range(2):
            self.refuses_values(OWNER + 0xA8 + i * 4,
                                (0, 0xFFFFFFFF, 0x80000000, self.start,
                                 self.buffers[i] + 4, self.end - 32, self.end))

    def test_buffers_and_allocator_headers_cannot_overlap(self):
        self.refuses_values(OWNER + 0xAC, (self.buffers[0], self.buffers[0] + 32,
                                         self.buffers[0] + 0x96000))

    def test_gx_encoded_destination_must_match_full_pointer(self):
        for offset in (0x6C, 0x8C):
            self.refuses_values(OWNER + offset, (0, 0xFFFFFFFF, self.get(OWNER + offset) ^ 1))
            # Changing only the BP register ID is an ordinary GXLoadTexObj operation.
            self.put(OWNER + offset, self.get(OWNER + offset) ^ 0x1F000000)
            self.assertEqual(self.run_case(), 1)

    def test_gx_format_dimensions_and_tile_count_must_match(self):
        for i in range(2):
            obj = OWNER + 0x60 + i * 0x20
            self.refuses_values(obj + 8, (0, self.get(obj + 8) ^ 1,
                                          self.get(obj + 8) ^ (1 << 10),
                                          self.get(obj + 8) ^ (1 << 20)))
            self.refuses_values(obj + 20, (0, 4, 6, 0x13))
            self.refuses_values(obj + 28, (0, self.get(obj + 28) ^ (1 << 16),
                                           self.get(obj + 28) ^ 1,
                                           self.get(obj + 28) ^ (1 << 8)))

    def test_exact_payload_size_group_and_hm_required(self):
        for i, size in enumerate((0x96000, 0x20000)):
            node = self.buffers[i] - 16
            self.refuses_values(node, (0, 0xFFFFFFFF, 0x484D000C, 0x484D000E))
            self.refuses_values(node + 4, (0, size - 4, size + 4, 0xFFFFFFFF))

    def test_alignment_padding_flags_do_not_change_group_identity(self):
        self.put(self.buffers[0] - 16, 0x484D0C0D)
        self.put(self.buffers[1] - 16, 0x484D8C0D)
        self.assertEqual(self.run_case(), 1)

    def test_forged_interior_hm_or_missing_list_membership_fails(self):
        # Headers still look perfect but are no longer in the allocator's list.
        self.put(self.head + 12, 0)
        self.tail = self.head
        self.assertEqual(self.run_case(), 0)
        self.put(self.head + 12, self.buffers[1] - 16)
        self.put(self.buffers[1] - 8, self.head)
        self.tail = self.buffers[1] - 16
        self.assertEqual(self.run_case(), 0)

    def test_reciprocal_links_invalid_next_and_tail_fail_safely(self):
        self.refuses_values(self.head + 12, (0xFFFFFFFF, 1, self.end, self.head))
        self.refuses_values(self.buffers[0] - 8, (0, self.head + 4, 0xFFFFFFFF))
        self.tail += 4
        self.assertEqual(self.run_case(), 0)

    def test_overlapping_used_allocation_is_rejected(self):
        self.put(self.head + 4, self.buffers[0] - self.head)
        self.assertEqual(self.run_case(), 0)

    def test_callback_failure_reports_exact_field(self):
        for address in (OWNER + 0xA8, OWNER + 0x88, self.head,
                        self.buffers[0] - 12, self.buffers[1] - 4):
            self.assertEqual(self.run_case(address), 0)
            self.assertEqual(self.lib.metric(2), address)

    def test_native_captures_admit_each_endpoint_but_not_mixed_owners(self):
        if not all(p.exists() for p in FIXTURES):
            self.skipTest("Optional private MEM1 captures unavailable")
        images = [p.read_bytes() for p in FIXTURES]
        pointers = []
        for image in images:
            self.image = bytearray(image)
            heap = self.get(0x804A0B98)
            self.start, self.end = self.get(heap + 0x30), self.get(heap + 0x34)
            self.head, self.tail = self.get(heap + 0x7C), self.get(heap + 0x80)
            self.assertEqual(self.run_case(), 1)
            pointers.append((self.get(OWNER + 0xA8), self.get(OWNER + 0xAC)))
        self.assertNotEqual(*pointers)
        # Old GAME image + later complete, internally consistent GX owner.
        self.image = bytearray(images[0])
        heap = self.get(0x804A0B98)
        self.head, self.tail = self.get(heap + 0x7C), self.get(heap + 0x80)
        offset = OWNER - 0x80000000
        self.image[offset:offset + SIZE] = images[1][offset:offset + SIZE]
        self.assertEqual(self.run_case(), 0)


class RenderTargetsNativeProofTests(unittest.TestCase):
    def test_clean_jp_lifecycle_and_gx_writes_are_authenticated(self):
        dol = ROOT / "build-lm-diag/clean_glmj_main.dol"
        if not dol.exists():
            self.skipTest("Private clean Japanese executable unavailable")
        data = dol.read_bytes()
        self.assertEqual(hashlib.sha1(data).hexdigest(),
                         "722005ea9c1eab54b114f814734d8f327e5614ee")
        offsets = struct.unpack_from(">18I", data, 0)
        bases = struct.unpack_from(">18I", data, 0x48)
        sizes = struct.unpack_from(">18I", data, 0x90)

        def word(address):
            for offset, base, size in zip(offsets, bases, sizes):
                if base <= address < base + size:
                    return struct.unpack_from(">I", data, offset + address - base)[0]
            self.fail(f"Unmapped native address {address:08X}")

        expected = {
            0x8000C0C8: 0x3860000D, 0x8000C0D0: 0x48050D71,
            0x8005CE4C: 0x38634B6C, 0x8005CE54: 0x4BFFF945,
            0x8005C7D8: 0x38600280, 0x8005C7AC: 0x9421FFE0,
            0x8005C7B0: 0x38A00013, 0x8005C7EC: 0x48194981,
            0x8005C804: 0x38800020, 0x8005C808: 0x38A00000,
            0x8005C80C: 0x4816C699, 0x8005C810: 0x907E00A8,
            0x8005C874: 0x387E0060, 0x8005C880: 0x38E00003,
            0x8005C918: 0x4816C58D, 0x8005C91C: 0x907E00AC,
            0x8005CAD4: 0x387E0080, 0x8005CAD8: 0x38A00100,
            0x8005CEBC: 0x807F00A8, 0x8005CEC0: 0x4816C05D,
            0x8005CEC4: 0x807F00AC, 0x8005CECC: 0x4816C051,
            0x80060614: 0x4BFFC88D,
            0x8005D014: 0x806300A8, 0x8005DD8C: 0x4BFFF281,
            0x8005DDE4: 0x387D0000, 0x8005DDEC: 0x481927E1,
            0x8005CCD0: 0x807F00A8, 0x8005CCD8: 0x481938F5,
            0x8005D418: 0x3865000C, 0x8005D428: 0x48197DA1,
        }
        for address, instruction in expected.items():
            self.assertEqual(word(address), instruction, f"Native proof changed at {address:08X}")


class RenderTargetsIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        cls.source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
        cls.constants = dict(re.findall(
            r"constexpr u32 (\w+)\s*=\s*([^;]+);", cls.source))

    def value(self, expression):
        expression = expression.replace("sizeof(u32)", "4")
        expression = re.sub(r"\b(0x[\da-fA-F]+|\d+)[uU]\b", r"\1", expression)
        expression = re.sub(r"\bk\w+\b", lambda m: str(self.value(
            self.constants[m.group()])), expression)
        self.assertRegex(expression, r"\A[\s\dxa-fA-F()+*~&|<>/-]+\Z")
        return eval(" ".join(expression.split()), {"__builtins__": {}})

    def body(self, name):
        match = re.search(r"\b(?:void|bool|int)\s+" + name +
                          r"\([^;{}]*\)\s*\{", self.source)
        self.assertIsNotNone(match, name)
        begin, depth = match.end(), 1
        for end in range(begin, len(self.source)):
            depth += (self.source[end] == "{") - (self.source[end] == "}")
            if not depth:
                return re.sub(r"\s+", " ", self.source[begin:end]).strip()
        self.fail(f"Unclosed function {name}")

    def test_format22_manifest_captures_exact_owner_without_destructor_record(self):
        self.assertEqual(self.value("kSnapshotVersion"), 28)
        self.assertEqual(self.value("kDepthTextureOwnerStateStart"), OWNER)
        self.assertEqual(self.value("kDepthTextureOwnerStateEnd"), OWNER + SIZE)
        manifest = re.search(r"kStateStaticRanges\[\]\s*=\s*\{(.*?)\n\};",
                             self.source, re.S).group(1)
        ranges = [(self.value(start), self.value(size)) for start, size in
                  re.findall(r"\{([^,{}]+),([^{}]+)\}", manifest)]
        self.assertEqual(ranges.count((OWNER, SIZE)), 1)
        for address, size in ranges:
            self.assertFalse(address < OWNER and OWNER - 12 < address + size,
                             "Captured a global-destructor registration link")
        self.assertEqual(sum(size for _, size in ranges), self.value("kStateStaticsSize"))
        for name, expected in (("kStateStaticsSize", 0x18D6C),
                               ("kCameraObjectStateOffset", 0x18EB4),
                               ("kHeapDataOffset", 0x191C0)):
            self.assertEqual(self.value(name), expected, name)

    def test_capture_restore_and_writeback_share_complete_manifest(self):
        for name in ("captureStaticRanges", "restoreStaticRanges", "storeStaticRanges"):
            body = self.body(name)
            self.assertIn("i < kStateStaticRangeCount", body)
            self.assertIn("const StaticRange &range = kStateStaticRanges[i]", body)
            self.assertIn("range.address", body)
            self.assertIn("range.size", body)
        self.assertIn("copyBytes(reinterpret_cast<void *>(kSnapshotBase + offset), "
                      "reinterpret_cast<void *>(range.address), range.size)",
                      self.body("captureStaticRanges"))
        self.assertIn("copyBytes(reinterpret_cast<void *>(range.address), "
                      "reinterpret_cast<void *>(kSnapshotBase + offset), range.size)",
                      self.body("restoreStaticRanges"))
        self.assertIn("kDCStoreRangeAddr", self.body("storeStaticRanges"))

    def test_reader_and_heap_list_anchors_select_same_saved_or_live_image(self):
        guard = self.body("renderTargetsValid")
        self.assertIn("LmRenderTargetsValidate(const_cast<SnapshotHeader *>(header), "
                      "grainReadWord,", guard)
        for saved, live in (("heapStart", "heapStart"), ("heapEnd", "heapEnd"),
                            ("usedHead", "heapUsedHead"), ("usedTail", "heapUsedTail")):
            self.assertIn(f"header ? header->{saved} : live.{live}", guard)
        reader = self.body("grainReadWord")
        self.assertIn("if (!header) { *value = readWord(address); return 1; }", reader)
        self.assertIn("offset = kHeapDataOffset + address - header->heapStart", reader)
        self.assertIn("const StaticRange &range = kStateStaticRanges[i]", reader)
        self.assertIn("offset = packed + address - range.address", reader)
        self.assertIn("*value = readWord(kSnapshotBase + offset)", reader)
        save = self.body("saveState")
        self.assertIn("header->usedHead = live.heapUsedHead", save)
        self.assertIn("header->usedTail = live.heapUsedTail", save)

    def test_all_three_guards_precede_snapshot_commit_or_first_restore_write(self):
        save, load = self.body("saveState"), self.body("loadState")
        saved_guard = "!renderTargetsValid(header, preflight)"
        live_guard = "!renderTargetsValid(nullptr, live)"
        self.assertEqual(save.count(live_guard), 1)
        self.assertEqual(load.count(saved_guard), 1)
        self.assertEqual(load.count(live_guard), 1)
        self.assertEqual(len(re.findall(r"\brenderTargetsValid\(", self.source)), 4)
        self.assertLess(save.index("freezeBegin()"), save.index(live_guard))
        self.assertLess(save.index(live_guard), save.index("header->magic = 0u"))
        self.assertLess(save.index("header->magic = 0u"), save.index("captureStaticRanges()"))
        self.assertLess(save.index("captureStaticRanges()"), save.rindex("freezeEnd(freeze)"))
        self.assertLess(load.index(saved_guard), load.index("freezeBegin()"))
        self.assertLess(load.index("freezeBegin()"), load.index(live_guard))
        first_write = load.index("restoreSharedArchive(header, sharedLive)")
        self.assertLess(load.index(live_guard), first_write)
        self.assertLess(first_write, load.index("copyWords("))
        self.assertLess(load.index("copyWords("), load.index("restoreStaticRanges()"))
        self.assertLess(load.index("restoreStaticRanges()"), load.index("storeStaticRanges()"))
        self.assertLess(load.index("storeStaticRanges()"), load.index("freezeEnd(freeze, true)"))


if __name__ == "__main__":
    unittest.main()
