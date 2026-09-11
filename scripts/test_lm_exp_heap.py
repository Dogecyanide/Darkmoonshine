"""Run the bounded production checker and authenticate the .36 free-link DSI."""

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
except ModuleNotFoundError:
    import test_lm_hud_state as retail

ROOT = Path(__file__).resolve().parents[1]
HEAP, START, END = 0x80001000, 0x80001088, 0x80005000
FIXTURES = [ROOT / 'build-lm-emu' / directory / 'mem1.bin' for directory in (
    'diagnostic-capture-0.3.30', 'diagnostic-capture-0.3.30-newreport')]


def put(image, address, value):
    struct.pack_into('>I', image, address - 0x80000000, value)


def word(image, address):
    return struct.unpack_from('>I', image, address - 0x80000000)[0]


def fixture(padding=0):
    image = bytearray(0x1800000)
    used = START + padding
    free = used + 0x30
    for address, value in {
        HEAP: 0x8038886C, HEAP + 0x30: START, HEAP + 0x34: END,
        HEAP + 0x38: END - START, HEAP + 0x74: free, HEAP + 0x78: free,
        HEAP + 0x7C: used, HEAP + 0x80: used,
        used: 0x484D0010 | padding << 8, used + 4: 0x20,
        free + 4: END - free - 16,
    }.items():
        put(image, address, value)
    return image


class HeapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('gcc') or shutil.which('clang')
        fallback = Path('C:/msys64/mingw64/bin/gcc.exe')
        if not compiler and fallback.exists():
            compiler = str(fallback)
        if not compiler:
            raise unittest.SkipTest('Native C compiler unavailable')
        cls.temp = tempfile.TemporaryDirectory(prefix='lm-exp-heap-')
        library = Path(cls.temp.name) / ('heap.dll' if os.name == 'nt' else 'heap.so')
        command = [compiler, '-shared', '-O2', '-std=c99', '-Wall', '-Werror',
                   '-I', str(ROOT / 'include'), str(ROOT / 'scripts/lm_exp_heap_harness.c'),
                   '-o', str(library)]
        if os.name != 'nt':
            command.insert(2, '-fPIC')
        env = os.environ.copy()
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
        subprocess.run(command, check=True, capture_output=True, env=env, timeout=60)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.run.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
        cls.lib.metric.argtypes = [ctypes.c_uint]
        cls.lib.metric.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        if os.name == 'nt':
            from _ctypes import FreeLibrary
            FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def run_image(self, image, heap=HEAP, fail=0):
        memory = (ctypes.c_ubyte * len(image)).from_buffer(image)
        result = self.lib.run(memory, len(image), heap, fail)
        self.assertEqual(self.lib.metric(1), 0, 'validator issued a foreign read')
        self.assertLess(self.lib.metric(0), 70000, 'bounded linear walk exceeded budget')
        return result

    def refused(self, changes, field=None, value=None):
        image = fixture()
        for address, change in changes.items():
            put(image, address, change)
        self.assertEqual(self.run_image(image), 0)
        if field is not None:
            self.assertEqual(self.lib.metric(2), field)
        if value is not None:
            self.assertEqual(self.lib.metric(3), value)

    def test_valid_and_alignment_padding_accounted(self):
        for padding in (0, 4, 12, 124):
            with self.subTest(padding=padding):
                self.assertEqual(self.run_image(fixture(padding)), 1)
                self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (0, 0))
        image = fixture()
        put(image, START, 0x484D8010)
        self.assertEqual(self.run_image(image), 1, 'tail-allocation bit is not padding')

    def test_null_reader_and_bad_heap_before_read(self):
        self.assertEqual(self.lib.nullReader(), 0)
        for pointer in (0, 0xFFFFFFFF, 0x817FFFF0, HEAP + 1):
            self.assertEqual(self.run_image(fixture(), pointer), 0)
            self.assertEqual(self.lib.metric(0), 0)

    def test_bad_heap_shape(self):
        for offset, value in ((0, 0), (0x30, 0), (0x30, HEAP + 0x84),
                              (0x30, END), (0x34, 0x81800004), (0x34, START),
                              (0x34, END + 1), (0x38, END - START - 4)):
            with self.subTest(offset=offset, value=value):
                self.refused({HEAP + offset: value})

    def test_heads_and_tails(self):
        for offset in (0x74, 0x7C):
            for value in (0xFFFFFFFF, START - 4, END - 12, START + 1):
                self.refused({HEAP + offset: value}, HEAP + offset, value)
            self.refused({HEAP + offset: 0})
        for offset in (0x78, 0x80):
            self.refused({HEAP + offset: 0}, HEAP + offset, 0)

    def test_header_magic_size_padding(self):
        free = START + 0x30
        for address, value in ((START, 0), (free, 0x484D0000),
                                (START, 0x484D0110), (START, 0x484D0410),
                                (START + 4, 0xFFFFFFFF), (free + 4, 0xFFFFFFFF),
                                (START + 4, END - START), (free + 4, END - free),
                                (START + 4, 0x21), (free + 4, 0x21)):
            with self.subTest(address=address, value=value):
                self.refused({address: value}, address, value)

    def test_free_next_ffffffff_regression(self):
        free = START + 0x30
        self.refused({free + 12: 0xFFFFFFFF}, free + 12, 0xFFFFFFFF)
        self.refused({free + 4: 0xFFFFFFFF, free + 12: 0xFFFFFFFF},
                     free + 4, 0xFFFFFFFF)

    def test_actual_crash_addresses_with_post_check_corruption(self):
        image = bytearray(0x1800000)
        heap, start, end, node = 0x80BE44D0, 0x80BE4560, 0x817FB140, 0x812A23C0
        for address, value in {
            heap: 0x8038886C, heap + 0x30: start, heap + 0x34: end,
            heap + 0x38: end - start, heap + 0x74: node, heap + 0x78: node,
            heap + 0x7C: start, heap + 0x80: start, start: 0x484D0000,
            start + 4: node - start - 16, node + 4: end - node - 16,
        }.items():
            put(image, address, value)
        self.assertEqual(self.run_image(image, heap), 1)
        put(image, node + 12, 0xFFFFFFFF)
        self.assertEqual(self.run_image(image, heap), 0)
        self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (0x812A23CC, 0xFFFFFFFF))
        put(image, node + 4, 0xFFFFFFFF)
        self.assertEqual(self.run_image(image, heap), 0)
        self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (0x812A23C4, 0xFFFFFFFF))

    def test_prev_next_reciprocity_and_cycles(self):
        for node in (START, START + 0x30):
            self.refused({node + 8: node}, node + 8, node)
            self.refused({node + 12: 0xFFFFFFFF}, node + 12, 0xFFFFFFFF)
            self.refused({node + 12: node})
            self.refused({node + 12: node + 1}, node + 12, node + 1)

    def test_sorted_free_ranges_do_not_overlap(self):
        free = START + 0x30
        image = fixture()
        second = free + 0x20
        put(image, free + 4, 0x20)
        put(image, free + 12, second)
        put(image, second + 4, END - second - 16)
        put(image, second + 8, free)
        put(image, HEAP + 0x78, second)
        self.assertEqual(self.run_image(image), 0)
        self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (free + 12, second))

    def test_zero_payload_free_header_is_valid(self):
        image = fixture()
        free = START + 0x30
        second = free + 0x10
        put(image, free + 4, 0)
        put(image, free + 12, second)
        put(image, second + 4, END - second - 16)
        put(image, second + 8, free)
        put(image, HEAP + 0x78, second)
        self.assertEqual(self.run_image(image), 1)

    def test_coverage_total_and_overflow(self):
        self.refused({START + 0x34: END - START - 0x44})
        image = fixture()
        put(image, HEAP + 0x74, 0)
        put(image, HEAP + 0x78, 0)
        self.assertEqual(self.run_image(image), 0)
        self.assertEqual(self.lib.metric(2), HEAP + 0x38)

    def test_every_callback_failure_stays_bounded(self):
        free = START + 0x30
        for address in [HEAP + off for off in (0, 0x30, 0x34, 0x38, 0x74, 0x78, 0x7C, 0x80)] + [
                node + off for node in (START, free) for off in (0, 4, 8, 12)]:
            with self.subTest(address=address):
                self.assertEqual(self.run_image(fixture(), fail=address), 0)
                self.assertEqual((self.lib.metric(2), self.lib.metric(3)), (address, 0))

    def test_fixed_node_limit(self):
        image = fixture()
        count = 8193
        end = START + count * 16
        put(image, HEAP + 0x34, end)
        put(image, HEAP + 0x38, end - START)
        put(image, HEAP + 0x74, 0)
        put(image, HEAP + 0x78, 0)
        put(image, HEAP + 0x80, end - 16)
        for index in range(count):
            node = START + index * 16
            for off, value in ((0, 0x484D0000), (4, 0),
                               (8, node - 16 if index else 0),
                               (12, node + 16 if index + 1 < count else 0)):
                put(image, node + off, value)
        self.assertEqual(self.run_image(image), 0)

    def test_private_retail_root_sys_game_fixtures(self):
        existing = [path for path in FIXTURES if path.exists()]
        if not existing:
            self.skipTest('Private MEM1 fixture unavailable')
        for path in existing:
            image = bytearray(path.read_bytes())
            for global_address in (0x804A0B90, 0x804A0B94, 0x804A0B98):
                with self.subTest(path=path.parent.name, heap=hex(global_address)):
                    self.assertEqual(self.run_image(image, word(image, global_address)), 1)


class RetailCrashEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_free_list_dereference_explains_dar7(self):
        self.words({0x801CA630: 0x3BC30000, 0x801CA6C4: 0x80DE0074,
                    0x801CA6CC: 0x8066000C, 0x801CA6D0: 0x80860004,
                    0x801CA6E4: 0x80030008, 0x801CA720: 0x28060000,
                    0x801CA724: 0x4082FFA8})
        self.assertEqual((0xFFFFFFFF + 8) & 0xFFFFFFFF, 7)
        self.assertEqual(0x812A23C0 + 12, 0x812A23CC)

    def test_native_padding_reciprocity_and_total_accounting(self):
        self.words({0x801CA64C: 0x88660002, 0x801CA658: 0x5463067E,
                    0x801CA664: 0x7CA02A14, 0x801CA668: 0x38A50010,
                    0x801CA674: 0x8066000C, 0x801CA694: 0x80030008,
                    0x801CA6A8: 0x801E0080, 0x801CA70C: 0x801E0078,
                    0x801CA728: 0x801E0038, 0x801CA72C: 0x7C050040})
        self.call(0x801CA638, 0x801D9024)
        self.call(0x801CA73C, 0x801D9100)

    def test_early_draw_boundary_precedes_outer_scene_teardown(self):
        self.words({0x8000B538: 0x801E0000, 0x8000B53C: 0x2C000002,
                    0x8000B350: 0x806D8038, 0x8000B354: 0x8183001C,
                    0x8000B35C: 0x4E800021,
                    0x8000B644: 0x800D0148, 0x8000B648: 0x2C000000,
                    0x8000B64C: 0x41820010, 0x8000B658: 0x48000048,
                    0x8000B718: 0x806D8038, 0x8000B71C: 0x81830020,
                    0x8000B724: 0x4E800021})
        self.call(0x8000B544, 0x8000B248)
        self.call(0x8000B360, 0x800078FC)
        self.call(0x8000B714, 0x8000B4E8)
        self.assertEqual(0x804A0AE0 + 0x148, 0x804A0C28)


class IntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.state = (ROOT / 'lm_diag/src/lm_state.cpp').read_text()
        cls.diag = (ROOT / 'lm_diag/src/lm_diag.cpp').read_text()

    @staticmethod
    def body(source, marker):
        start = source.index('{', source.index(marker))
        level = 1
        for end in range(start + 1, len(source)):
            level += (source[end] == '{') - (source[end] == '}')
            if not level:
                return source[start + 1:end]
        raise AssertionError('Unclosed function: ' + marker)

    def test_native_unchecked_heap_checker_not_called(self):
        for source in (self.state, self.diag):
            self.assertNotIn('801CA61C', source)
            self.assertNotIn('ExpHeapCheckFn', source)
            self.assertNotIn('kExpHeapCheckAddr', source)
        self.assertIn('#include "susamune/lm_exp_heap.h"', self.state)

    def test_heap_wrapper_restores_interrupts_before_reporting_exact_fault(self):
        body = self.body(self.state, 'bool heapHealthy(u32 heap)')
        markers = ('kOSDisableInterruptsAddr', 'LmExpHeapValidate(',
                   'kOSRestoreInterruptsAddr', 'if (!valid)',
                   '0xF0u, fault', '0xF1u, value', '0xF2u, heap',
                   'SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xF4u, fault, value',
                   'return valid')
        positions = [body.index(marker) for marker in markers]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(body.count('LmExpHeapValidate('), 1)

    def test_post_copy_bad_heap_stops_before_loaded_or_success_marker(self):
        start = self.state.index('traceLoadPhase(0x75u, live.rootHeap)')
        end = self.state.index('LMCrash::note(kEventStateLoad,', start)
        tail = self.state[start:end]
        for heap in ('rootHeap', 'systemHeap', 'heap'):
            self.assertIn(f'LMState::heapHealthy(live.{heap})', tail)
        failure = self.body(tail, 'if (!healthyAfter)')
        self.assertIn('__builtin_trap()', failure)
        self.assertNotIn('return', failure)
        self.assertNotIn('setReject', failure)
        self.assertLess(tail.index('__builtin_trap()'), tail.index('Status::Loaded'))
        self.assertLess(tail.index('__builtin_trap()'), tail.index('traceLoadPhase(0x7Fu'))

    def test_early_draw_and_gpu_checks_stop_after_initial_trace_states(self):
        markers = ('void postLoadMilestone(u32 phase)', 'void presenterAfterDrawDone()')
        for marker in markers:
            with self.subTest(marker=marker):
                body = self.body(self.state, marker)
                check = body[:body.index('__builtin_trap()')]
                self.assertIn('sPostLoadTraceState == 1u', check)
                self.assertIn('sPostLoadTraceState == 2u', check)
                self.assertNotIn('sPostLoadTraceState == 3u', check)
                self.assertIn('!heapHealthy(readWord(kGameHeapGlobal))', check)
                self.assertIn('readWord(kMainLoopModeGlobal) == 2u', check)
                self.assertIn('readWord(kMainLoopExitGlobal) == 0u', check)
                self.assertNotIn('isExpHeap', check)
                if 'Milestone' in marker:
                    self.assertIn('phase == 0x8Bu', check)
        presenter = self.body(self.state, 'void presenterEnter()')
        self.assertIn('sPostLoadTraceFrame >= kPostLoadTraceFrameLimit', presenter)
        self.assertIn('sPostLoadTraceState = 3u', presenter)
        self.assertRegex(self.state, r'kPostLoadTraceFrameLimit\s*=\s*8u;')

    def test_checks_follow_completed_cpu_and_gpu_work(self):
        body = self.body(self.diag, 'void diagnosticMainSceneStep()')
        self.assertLess(body.index('kLMMainSceneStepAddr'), body.index('0x8Bu'))
        body = self.body(self.diag, 'void diagnosticCopyDisp(')
        self.assertLess(body.index('kGXCopyDispAddr'), body.index('kGXDrawDoneAddr'))
        self.assertLess(body.index('kGXDrawDoneAddr'), body.index('presenterAfterDrawDone'))

    def test_periodic_check_keeps_cadence_first_fault_and_stops(self):
        body = self.body(self.diag, 'void sampleHeapChecks(')
        self.assertIn('if (++sFrames < 60u)', body)
        self.assertLess(body.index('if (++sFrames < 60u)'), body.index('LMState::heapHealthy'))
        self.assertRegex(body, r'gameOk\s*=\s*systemOk\s*&&\s*LMState::heapHealthy\(game.pointer\)')
        self.assertIn('if (!systemOk || !gameOk) __builtin_trap()', body)

    def test_fault_phase_does_not_alias_audio_checkpoints(self):
        telemetry = (ROOT / 'include/susamune/lm_crash_telemetry.h').read_text()
        reader = (ROOT / 'scripts/read_lm_dump.py').read_text()
        self.assertIn('trace->phase == 0xF4u', telemetry)
        self.assertIn('trace->phase == 0xF3u', telemetry)
        self.assertNotIn('trace->phase == 0xF0u', telemetry)
        self.assertIn('record.phase in (0xF4, 0xF3)', reader)
        self.assertIn('record.phase == 0xF4', reader)
        self.assertIn('postLoadDetail(0xF0u, 0x8000B618u, 0x801867B4u)', self.diag)


if __name__ == '__main__':
    unittest.main()
