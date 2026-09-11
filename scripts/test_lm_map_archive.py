"""Native MissionMode map mount ownership; saved captures are data, not code."""
import ctypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

try:
    from scripts import compare_lm_archives as archives
except ModuleNotFoundError:
    import compare_lm_archives as archives

ROOT = Path(__file__).resolve().parents[1]
START, END, HEAP = 0x80BE4560, 0x817FB140, 0x80BE44D0


class MapRoots(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ('mission', 'archive', 'backing', 'bytes', 'heap', 'start', 'end', 'usedHead', 'usedTail')]


class MapArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('gcc') or shutil.which('clang')
        if not compiler and Path('C:/msys64/mingw64/bin/gcc.exe').exists():
            compiler = 'C:/msys64/mingw64/bin/gcc.exe'
        if not compiler:
            raise unittest.SkipTest('Native C compiler required')
        cls.temp = tempfile.TemporaryDirectory(prefix='lm-map-archive-')
        target = Path(cls.temp.name) / ('map.dll' if os.name == 'nt' else 'map.so')
        command = [compiler, '-shared', '-O2', '-std=c99', '-Wall', '-Werror',
                   '-I', str(ROOT / 'include'), str(ROOT / 'scripts/lm_map_archive_harness.c'),
                   '-o', str(target)]
        if os.name != 'nt': command.insert(2, '-fPIC')
        env = os.environ.copy()
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
        subprocess.run(command, env=env, check=True, capture_output=True)
        cls.lib = ctypes.CDLL(str(target))
        cls.lib.mapValidate.argtypes = [ctypes.c_void_p, ctypes.c_uint,
            ctypes.POINTER(MapRoots), ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
        cls.lib.mapValidate.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        if os.name == 'nt':
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.image = bytearray(24 * 1024 * 1024)
        self.roots = MapRoots(0x81000010, 0x81000110, 0x81000220, 0x400, HEAP,
                             START, END, 0x81000000, 0x81000210)
        self.make_graph()

    def put(self, address, value):
        struct.pack_into('>I', self.image, address - 0x80000000, value)

    def get(self, address):
        return struct.unpack_from('>I', self.image, address - 0x80000000)[0]

    def make_graph(self):
        r = self.roots
        for address, value in ((0x804A17C8, r.mission), (0x804A17B0, r.mission),
            (r.mission, 0x8034F080), (r.mission + 0x18, r.archive),
            (r.archive, 0x80388D5C), (r.archive + 4, r.heap),
            (r.archive + 0x2C, 0x52415243), (r.archive + 0x38, r.heap),
            (r.archive + 0x40, r.backing), (r.archive + 0x44, r.backing + 0x20),
            (r.archive + 0x5C, r.backing), (r.archive + 0x60, r.backing + 0x100),
            (r.backing, 0x52415243), (r.backing + 4, r.bytes),
            (r.backing + 8, 0x20), (r.backing + 0xC, 0xE0)):
            self.put(address, value)
        targets, sizes = (r.mission, r.archive, r.backing), (0x24, 0x68, r.bytes)
        for i, (base, size) in enumerate(zip(targets, sizes)):
            node = base - 16
            for offset, value in ((0, 0x484D000B), (4, size),
                (8, targets[i - 1] - 16 if i else 0),
                (12, targets[i + 1] - 16 if i < 2 else 0)):
                self.put(node + offset, value)

    def check(self, fail=0):
        data = (ctypes.c_ubyte * len(self.image)).from_buffer(self.image)
        results = (ctypes.c_uint * 4)()
        okay = self.lib.mapValidate(data, len(data), ctypes.byref(self.roots), fail, results)
        self.assertLess(results[2], 8192 * 5 + 64)
        self.assertEqual(results[3], 0)
        return okay, list(results)

    def test_complete_native_owner_and_backing(self):
        self.assertEqual(self.check()[0], 1)
        self.assertEqual(self.lib.mapNull(), 0)

    def test_separate_saved_live_images_can_have_different_backing(self):
        self.assertTrue(self.check()[0])
        self.roots.backing += 0x1000
        self.roots.usedTail = self.roots.backing - 16
        self.make_graph()
        self.assertTrue(self.check()[0])

    def test_owner_and_all_backing_edges_must_match(self):
        r = self.roots
        fields = (0x804A17C8, 0x804A17B0, r.mission, r.mission + 0x18,
                  r.archive, r.archive + 4, r.archive + 0x2C, r.archive + 0x38,
                  r.archive + 0x40, r.archive + 0x44, r.archive + 0x5C,
                  r.archive + 0x60, r.backing, r.backing + 4, r.backing + 8)
        for address in fields:
            old = self.get(address)
            self.put(address, old ^ 4)
            self.assertFalse(self.check()[0], hex(address))
            self.put(address, old)
            self.assertFalse(self.check(fail=address)[0], hex(address))

    def test_partial_unaligned_and_foreign_targets_are_rejected_before_read(self):
        for field, size in (('mission', 0x24), ('archive', 0x68), ('backing', 0x400)):
            old = getattr(self.roots, field)
            for value in (0, START, END, END - size + 4, 0xFFFFFFFF, old + 1):
                setattr(self.roots, field, value)
                self.assertFalse(self.check()[0], (field, hex(value)))
            setattr(self.roots, field, old)

    def test_archive_sizes_and_rarc_data_offsets_are_bounded(self):
        old = self.roots.bytes
        for size in (0, 0x20, 0x401, 0xFFFFFFFF):
            self.roots.bytes = size
            self.assertFalse(self.check()[0])
        self.roots.bytes = old
        for offset in (old, 0xFFFFFFFF):
            self.put(self.roots.backing + 0xC, offset)
            self.assertFalse(self.check()[0])

    def test_native_sizes_groups_tags_and_padding_are_required(self):
        for base in (self.roots.mission, self.roots.archive, self.roots.backing):
            for offset, values in ((-16, (0, 0x484D0001, 0x484D010B)),
                                   (-12, (0, 0xFFFFFFFF, self.get(base - 12) + 4))):
                old = self.get(base + offset)
                for value in values:
                    self.put(base + offset, value)
                    self.assertFalse(self.check()[0])
                self.put(base + offset, old)

    def test_every_owner_must_be_in_complete_reciprocal_list(self):
        for base in (self.roots.mission, self.roots.archive, self.roots.backing):
            for offset in (-8, -4):
                old = self.get(base + offset)
                self.put(base + offset, old ^ 0x100)
                self.assertFalse(self.check()[0])
                self.put(base + offset, old)
        self.roots.usedTail = self.roots.archive - 16
        self.assertFalse(self.check()[0])

    def test_orphan_and_cyclic_list_are_refused(self):
        self.put(self.roots.mission - 4, self.roots.backing - 16)
        self.put(self.roots.backing - 8, self.roots.mission - 16)
        self.assertFalse(self.check()[0])
        self.make_graph()
        self.put(self.roots.backing - 4, self.roots.mission - 16)
        self.assertFalse(self.check()[0])

    def test_foreign_allocations_cannot_overlap_proven_owner_extents(self):
        node = self.roots.backing + 0x100
        self.put(self.roots.backing - 4, node)
        for offset, value in ((0, 0x484D0001), (4, 0x20),
                             (8, self.roots.backing - 16), (12, 0)):
            self.put(node + offset, value)
        self.roots.usedTail = node
        self.assertFalse(self.check()[0])

    def test_optional_runner_archives_prove_actual_saved_map_allocations(self):
        base = ROOT.parent / 'sd-captures/lm-0.3.41-runner-20260908'
        paths = sorted(base.glob('*/extracted/lm_states/*.lms'))
        if not paths: self.skipTest('Optional private runner archives absent')
        for path in paths:
            result = archives.inspect_archive(path)
            payload, h = path.read_bytes()[64:], result.header
            self.image = bytearray(24 * 1024 * 1024)
            self.image[h['heapStart'] - 0x80000000:h['heapEnd'] - 0x80000000] = \
                payload[h['heapDataOffset']:h['totalSize']]
            offset = h['stateStaticsOffset']
            for _, address, size in archives.STATIC_RANGES:
                self.image[address - 0x80000000:address - 0x80000000 + size] = payload[offset:offset + size]
                offset += size
            backing = self.get(h['mapArchive'] + 0x5C)
            self.roots = MapRoots(h['missionMode'], h['mapArchive'], backing, self.get(backing + 4),
                h['heap'], h['heapStart'], h['heapEnd'], h['usedHead'], h['usedTail'])
            self.assertTrue(self.check()[0], (path, self.check()[1]))

    def test_runtime_proves_both_map_endpoints_before_requiring_all_volumes(self):
        source = (ROOT / 'lm_diag/src/lm_state.cpp').read_text()
        method = source.split('bool modelReplacementMatches(', 1)[1].split('bool resourceReplacementMatches(', 1)[0]
        for saved in ('true', 'false'):
            self.assertIn('matchMapArchive(header, live, ' + saved, method)
        self.assertLess(method.index('matchMapArchive'), method.index('everyChangedVolumeMatched'))
        self.assertIn('header->mapArchive == live.mapArchive', source)


if __name__ == '__main__': unittest.main()
