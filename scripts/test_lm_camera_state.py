"""Native camera lifetime, exact GAME ownership, and controlled reboot evidence."""
import ctypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

try:
    from scripts import test_lm_hud_state as retail
    from scripts import compare_lm_archives as archives
    from scripts import test_lm_scene_owner_audit as capture
except ModuleNotFoundError:
    import test_lm_hud_state as retail
    import compare_lm_archives as archives
    import test_lm_scene_owner_audit as capture

ROOT = Path(__file__).resolve().parents[1]
TABLE, ACTIVE = 0x80399BE0, 0x804A0DB8
PAIR = ROOT.parent / 'sd-captures/lm-0.3.40-reboot-pair-20260907/lm_states'


class CameraRetailEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_three_exact_plain_blocks_use_current_heap(self):
        self.words({0x80020054: 0x3BE39BE0, 0x80020058: 0x386000EC,
                    0x80020088: 0x386000EC, 0x800200B4: 0x386000EC,
                    0x80020084: 0x93DF0000, 0x800200B0: 0x93DF0004,
                    0x800200DC: 0x93DF0008, 0x801C931C: 0x800D1514,
                    0x801C9330: 0x38A00004, 0x801C9334: 0x818C000C})
        for site in (0x80020060, 0x8002008C, 0x800200B8): self.call(site, 0x801C9308)
        for site in (0x80020080, 0x800200AC, 0x800200D8): self.call(site, 0x8001FA24)

    def test_scene_setup_and_cleanup_recreate_not_boot_pin_targets(self):
        self.words({0x8000BEE4: 0x38600001, 0x800060F0: 0x806D00B8})
        self.call(0x8000BEE8, 0x800060CC)
        self.call(0x8000BEEC, 0x80010B5C)
        self.call(0x80010B68, 0x80020040)
        self.call(0x80010AE0, 0x80020110)
        self.call(0x8000BE78, 0x80011650)
        self.call(0x80011674, 0x80020538)
        self.words({0x8002054C: 0x3BE39BE0, 0x80020550: 0x807F0000,
                    0x80020558: 0x807F0004, 0x80020560: 0x807F0008})
        for site in (0x80020554, 0x8002055C, 0x80020564): self.call(site, 0x801C9508)

    def test_constructor_does_not_initialize_a_vtable_or_all_callback_slots(self):
        self.words({0x8001FA48: 0x93E30004, 0x8001FA4C: 0xB3E30000,
                    0x8001FA98: 0x93FE00D0, 0x8001FA9C: 0x93FE00D4,
                    0x8001FAA0: 0x909E00D8, 0x8001FAA4: 0x901E00DC,
                    0x8001FABC: 0x93FE00E0})
        self.call(0x8001FAB8, 0x8000A184)
        # +8, +E4, and camera2's unused +E8 retain allocation bytes; not a vtable.
        for address in range(0x8001FA24, 0x8001FAD8, 4):
            instruction = self.word(address)
            if instruction >> 26 == 36 and (instruction >> 16) & 31 in (3, 30):
                self.assertNotIn(instruction & 0xFFFF, (0, 8, 0xE4, 0xE8))

    def test_callbacks_are_selected_by_flags_not_all_required_live(self):
        self.words({0x80020308: 0x806D02D8, 0x8002030C: 0xA0030000,
                    0x80020310: 0x540007FF, 0x80020318: 0x81830008,
                    0x8002031C: 0x280C0000, 0x80020328: 0x4E800021,
                    0x80020330: 0x81830004, 0x8002033C: 0x8063000C,
                    0x80020344: 0x4E800021,
                    0x8001FAEC: 0x90830004, 0x8001FAF0: 0x9003000C,
                    0x8001FB88: 0xB01D0000, 0x8001FB8C: 0x93DD0008})

    def test_camera_descriptors_and_render_outputs_use_captured_fixed_state(self):
        self.words({0x80020140: 0x38048C00, 0x80020144: 0x900300E8,
                    0x80020180: 0x38048C00, 0x80020184: 0x900300E8,
                    0x80020154: 0x38041464, 0x80020194: 0x380419FC,
                    0x80020270: 0x38632A48, 0x80020274: 0x90640004,
                    0x80021A08: 0x806D02D8, 0x80021A0C: 0x800300E8,
                    0x80021A10: 0x900D0218, 0x80020404: 0x83CD02D8,
                    0x80020418: 0x3BE38780, 0x8002050C: 0x38A50040})

    def test_active_alias_can_select_an_embedded_view_outside_standalone_table(self):
        self.words({0x800202A8: 0x906D02D8, 0x80025698: 0x800D0350,
                    0x800256A0: 0x7F20DA14, 0x8002573C: 0x3B19011C,
                    0x80025798: 0x38780000, 0x800257A4: 0x932D0354,
                    0x800257D8: 0x3B7B024C})
        self.call(0x800257A8, 0x800202A8)
        self.assertEqual(0x804A0AE0 + 0x2D8, ACTIVE)
        self.assertLessEqual(0x11C + 0xEC, 0x24C)


class CameraCaptureCoverage(capture.SceneOwnerCaptureIntegration):
    def test_camera_owner_table_active_alias_descriptors_and_embedded_views_are_captured(self):
        for address, size in ((TABLE, 12), (ACTIVE, 4), (0x80398C00, 0x50),
                              (0x804A0CF8, 4), (0x804A0E30, 8), (0x80398780, 0x25C)):
            self.assertTrue(self.covered(address, size), hex(address))

    def test_changed_targets_require_both_images_and_zero_sidecars(self):
        source = self.source.split('bool cameraObjectsValid(', 1)[1].split('void captureCameraObjects(', 1)[0]
        self.assertIn('if (!matchSnapshot || !replaced) return true;', source)
        self.assertIn('capturedSize != (inGameHeap ? 0u : kCameraObjectSize)', source)
        self.assertIn('cameraObjectRecordAddress(i) + 4u) != 0u', source)
        self.assertIn('const_cast<SnapshotHeader *>(header), grainReadWord', source)
        self.assertIn('savedTargets, header->heapStart, header->heapEnd', source)
        self.assertIn('nullptr, grainReadWord, liveTargets', source)
        self.assertIn('identity.heapUsedHead,', source)
        self.assertIn('identity.heapUsedTail,', source)
        self.assertIn('0xF4u + i', source)
        self.assertIn('0xF7u, fault, value', source)
        self.assertIn('0xF8u, fault, value', source)
        for mutation in ('writeWord(', 'copyBytes(', 'restoreCameraObjects('):
            self.assertNotIn(mutation, source)

    def test_saved_reader_cannot_fall_back_to_live_state(self):
        source = self.source.split('int grainReadWord(void *context,', 2)[2]
        source = source.split('bool grainStateValid(', 1)[0]
        self.assertIn('if (!header)', source)
        self.assertIn('if (!found) return 0;', source)
        self.assertIn('sizeof(u32) > header->totalSize - offset', source)
        self.assertIn('readWord(kSnapshotBase + offset)', source)


class CameraHelperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('g++') or 'C:/msys64/mingw64/bin/g++.exe'
        if not Path(compiler).exists(): raise unittest.SkipTest('Native compiler unavailable')
        cls.temp = tempfile.TemporaryDirectory(prefix='lm-camera-')
        path = Path(cls.temp.name) / ('camera.dll' if os.name == 'nt' else 'camera.so')
        env = os.environ.copy()
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
        command = [compiler, '-shared', '-O2', '-std=c++17', '-Wall', '-Werror',
                   '-I', str(ROOT / 'include'), str(ROOT / 'scripts/lm_camera_state_harness.c'),
                   '-o', str(path)]
        if os.name != 'nt': command.insert(2, '-fPIC')
        subprocess.run(command, check=True, capture_output=True, env=env, timeout=60)
        cls.lib = ctypes.CDLL(str(path))
        wordptr = ctypes.POINTER(ctypes.c_uint)
        cls.lib.camera_validate.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint,
            wordptr, wordptr, ctypes.c_uint, ctypes.c_uint, wordptr]

    @classmethod
    def tearDownClass(cls):
        if os.name == 'nt':
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.start = 0x80010000
        self.heap = bytearray(0x1000)
        self.nodes = [self.start + i * 0xFC for i in range(3)]
        self.targets = [node + 16 for node in self.nodes]
        self.roots = self.targets[:]
        self.head, self.tail = self.nodes[0], self.nodes[-1]
        for i, node in enumerate(self.nodes):
            self.put(node, 0x484D0001, 0xEC, self.nodes[i - 1] if i else 0,
                     self.nodes[i + 1] if i + 1 < 3 else 0)

    def put(self, address, *values):
        struct.pack_into('>' + 'I' * len(values), self.heap, address - self.start, *values)

    def check(self):
        roots = (ctypes.c_uint * 3)(*self.roots)
        targets = (ctypes.c_uint * 3)(*self.targets)
        diagnostics = (ctypes.c_uint * 4)(0xFFFFFFFF, 0xFFFFFFFF, 0, 0)
        memory = ctypes.create_string_buffer(bytes(self.heap))
        valid = self.lib.camera_validate(memory, len(self.heap), self.start,
            roots, targets, self.head, self.tail, diagnostics)
        self.assertLessEqual(diagnostics[2], 8192 * 4 + 6)
        self.assertEqual(diagnostics[3], 0, 'Helper followed an out-of-range address')
        return bool(valid), tuple(diagnostics)

    def test_three_exact_allocations_pass_without_interpreting_inactive_bytes(self):
        for target in self.targets: self.put(target, 0xFE00, 0x80021464, 0xAA9C3962)
        valid, diagnostic = self.check()
        self.assertTrue(valid)
        self.assertEqual(diagnostic[:2], (0, 0))

    def test_duplicate_interior_unaligned_and_cross_boundary_targets_refuse(self):
        for target in (self.targets[0], self.targets[1] + 4, self.targets[1] + 1,
                       self.start - 4, self.start + len(self.heap) - 4, 0xFFFFFFF0):
            self.setUp(); self.targets[1] = self.roots[1] = target
            self.assertFalse(self.check()[0], hex(target))

    def test_camera_record_must_match_its_own_fixed_manager(self):
        self.roots[2] += 4
        valid, diagnostic = self.check()
        self.assertFalse(valid)
        self.assertEqual(diagnostic[:2], (TABLE + 8, self.roots[2]))

    def test_wrong_magic_size_group_and_used_membership_refuse(self):
        for field, value in ((0, 0x00000001), (0, 0x484D0002), (4, 0xE8), (4, 0xF0)):
            self.setUp(); self.put(self.nodes[1] + field, value)
            self.assertFalse(self.check()[0])
        self.setUp()
        self.put(self.nodes[0] + 12, self.nodes[2]); self.put(self.nodes[2] + 8, self.nodes[0])
        self.assertFalse(self.check()[0])

    def test_reciprocal_links_tail_cycles_and_foreign_links_refuse(self):
        for address, value in ((self.nodes[1] + 8, 0), (self.nodes[2] + 12, self.head),
                               (self.nodes[1] + 12, 0xFFFFFFFF),
                               (self.nodes[1] + 12, self.start + len(self.heap))):
            self.setUp(); self.put(address, value)
            self.assertFalse(self.check()[0])
        self.setUp(); self.tail += 4
        self.assertFalse(self.check()[0])

    def test_other_used_allocation_cannot_overlap_camera_payload_or_header(self):
        self.put(self.nodes[0] + 12, self.nodes[0] + 0x40)
        self.put(self.nodes[0] + 0x40, 0x484D0002, 16, self.nodes[0], self.nodes[1])
        self.assertFalse(self.check()[0])

    def test_alignment_padding_cannot_overlap_cameras_or_extend_before_heap(self):
        other = self.nodes[-1] + 0xFC
        self.put(self.nodes[-1] + 12, other)
        self.put(other, 0x484D0402, 16, self.nodes[-1], 0)
        self.tail = other
        self.assertFalse(self.check()[0], 'Other allocation padding overlaps camera tail')
        self.setUp(); self.put(self.nodes[0], 0x484D0401)
        self.assertFalse(self.check()[0], 'Camera padding extends before heap')
        self.setUp(); self.put(self.nodes[1], 0x484D0101)
        self.assertFalse(self.check()[0], 'Unaligned padding')
        self.setUp(); self.put(self.nodes[1], 0x484D0401)
        self.assertFalse(self.check()[0], 'Camera padding overlaps preceding camera')

    def test_bounded_padding_in_real_gap_is_not_misclassified_as_overlap(self):
        self.heap = bytearray(0x1000)
        self.nodes = [self.start + 0x20 + i * 0x120 for i in range(3)]
        self.targets = [node + 16 for node in self.nodes]
        self.roots = self.targets[:]
        self.head, self.tail = self.nodes[0], self.nodes[-1]
        for i, node in enumerate(self.nodes):
            self.put(node, 0x484D0401, 0xEC, self.nodes[i - 1] if i else 0,
                     self.nodes[i + 1] if i < 2 else 0)
        self.assertTrue(self.check()[0])

    def test_long_used_list_hits_bounded_cap_without_foreign_reads(self):
        self.heap = bytearray(0x30000)
        nodes = self.nodes + [self.start + 0x400 + i * 20 for i in range(8190)]
        self.tail = nodes[-1]
        for i, node in enumerate(nodes):
            self.put(node, 0x484D0001 if i < 3 else 0x484D0002, 0xEC if i < 3 else 4,
                     nodes[i - 1] if i else 0, nodes[i + 1] if i + 1 < len(nodes) else 0)
        valid, diagnostic = self.check()
        self.assertFalse(valid)
        self.assertEqual(diagnostic[2], 8192 * 4 + 6)

    def test_controlled_same_build_reboot_pair_both_pass_at_different_addresses(self):
        paths = [PAIR / f'archive_{i:08}.lms' for i in (3, 4)]
        if not all(path.exists() for path in paths): self.skipTest('Optional private .40 reboot pair missing')
        captures, targets, sessions = [], [], []
        for path in paths:
            archive = archives.inspect_archive(path)
            self.assertEqual(archive.envelope['buildCrc'], 0xE0700BF2)
            self.assertEqual(archive.envelope['snapshotVersion'], 24)
            sessions.append(archive.envelope['session'])
            payload = path.read_bytes()[64:]
            static_spans, offset = [], archives.STATICS_OFFSET
            for _, address, size in archives.STATIC_RANGES:
                static_spans.append((address, size, offset))
                offset += size
            def static_word(address):
                for start, size, offset in static_spans:
                    if start <= address and address + 4 <= start + size:
                        return struct.unpack_from('>I', payload, offset + address - start)[0]
                self.fail(f'Uncaptured static word {address:08X}')
            self.start = archive.header['heapStart']
            self.heap = bytearray(payload[archives.GAME_OFFSET:
                archives.GAME_OFFSET + archive.header['heapSize']])
            self.targets = [archive.facts[f'camera.{i}.target'] for i in range(3)]
            self.roots = self.targets[:]
            self.head, self.tail = archive.header['usedHead'], archive.header['usedTail']
            active, array, current = (static_word(address) for address in
                (ACTIVE, 0x804A0E30, 0x804A0E34))
            self.assertEqual((active, current), (array + 0x11C, array))
            self.assertNotIn(active, self.targets)
            for i in range(3):
                self.assertEqual(archive.facts[f'camera.{i}.capturedBytes'], 0)
                self.assertEqual(archive.facts[f'camera.{i}.allocationSize'], 0xEC)
                self.assertEqual(archive.facts[f'camera.{i}.allocationOffset'], 0)
                self.assertEqual(archive.facts[f'camera.{i}.allocationTag'], 0x484D0001)
            self.assertTrue(self.check()[0])
            captures.append((self.start, self.heap, self.head, self.tail, self.targets[:]))
            targets.append(self.targets[:])
        self.assertNotEqual(sessions[0], sessions[1])
        self.assertTrue(all(a != b for a, b in zip(*targets)))
        # A foreign manager plus this GAME image must not receive ownership approval.
        self.start, self.heap, self.head, self.tail, self.targets = captures[0]
        self.roots = targets[1]
        self.assertFalse(self.check()[0])
        self.targets = targets[1]
        self.assertFalse(self.check()[0])


if __name__ == '__main__': unittest.main()
