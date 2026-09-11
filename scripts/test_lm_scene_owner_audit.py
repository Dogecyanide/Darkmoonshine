"""Native evidence for scene cleanup coverage and the plain model draw context."""

from pathlib import Path
import re
import unittest

try:
    from scripts import test_lm_hud_state as retail
    from scripts import test_lm_render_targets as render_tests
except ModuleNotFoundError:
    import test_lm_hud_state as retail
    import test_lm_render_targets as render_tests

ROOT = Path(__file__).resolve().parents[1]
CONTEXT, CONTEXT_END = 0x803C4A10, 0x803C4A50


class SceneOwnerRetailEvidence(unittest.TestCase):
    setUpClass = classmethod(retail.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail.AuthenticatedRetailTests.data
    word = retail.AuthenticatedRetailTests.word
    words = retail.AuthenticatedRetailTests.words
    call = retail.AuthenticatedRetailTests.call

    def test_model_context_is_matrix_pointer_and_scalar_state(self):
        self.words({0x80057688: 0x38634A10, 0x80057690: 0x90030038,
                    0x8005769C: 0x38844A10, 0x800576A0: 0x9864003C,
                    0x800576AC: 0x38844A10, 0x800576B0: 0x9864003D,
                    0x800576C0: 0x9C034A10, 0x800576C4: 0x9803003C,
                    0x800576C8: 0x9803003D,
                    0x80058AB4: 0x38834A10, 0x80058ABC: 0x38840004})
        self.call(0x80058AC0, 0x801DC66C)
        self.assertEqual(4 + 12 * 4, 0x34)
        self.assertEqual(CONTEXT_END - CONTEXT, 0x40)

    def test_mesh_paths_publish_borrowed_matrix_before_material_callback(self):
        self.words({0x80058B80: 0x90050034, 0x80058CAC: 0x90050034,
                    0x80058DA4: 0x90030034, 0x8005B32C: 0x38634A10,
                    0x8005B35C: 0x80830034})
        # This is a per-model draw context, not an independently deleted object.
        self.call(0x80059128, 0x80058B3C)
        self.call(0x8005943C, 0x80058C5C)
        self.call(0x80059878, 0x80058D64)

    def test_cleanup_after_room_prop_family_uses_captured_effect_roots(self):
        self.call(0x8000BE78, 0x80011650)
        self.call(0x8000BE7C, 0x80160E30)
        self.call(0x8000BE88, 0x8015EAAC)
        self.words({0x80160E44: 0x3BE4E0F0, 0x80160EEC: 0x387F021C,
                    0x80160EFC: 0x387F03B0, 0x80160F08: 0x3863E7D0,
                    0x8000BE84: 0x3863D4FC})
        self.call(0x8000BE9C, 0x801851A0)
        self.assertEqual(self.word(0x801851A0), 0x4E800020)
        self.call(0x8000BEA0, 0x80056E08)
        self.assertEqual(self.word(0x80056E18), 0x83ED0720)

    def test_async_archive_request_is_not_a_scene_owner(self):
        self.call(0x80066180, 0x801CBE40)
        self.assertEqual(self.word(0x80066184), 0x907E0010)
        self.call(0x800661C4, 0x801CC138)
        # +10 of 803C8450 is a live DVD request, despite its GAME address.
        self.assertEqual(0x803C8450 + 0x10, 0x803C8460)


class SceneOwnerCaptureIntegration(unittest.TestCase):
    setUpClass = classmethod(render_tests.RenderTargetsIntegrationTests.setUpClass.__func__)
    value = render_tests.RenderTargetsIntegrationTests.value

    def ranges(self):
        manifest = re.search(r'kStateStaticRanges\[\]\s*=\s*\{(.*?)\n\};',
                             self.source, re.S).group(1)
        return [(self.value(a), self.value(n)) for a, n in
                re.findall(r'\{([^,{}]+),([^{}]+)\}', manifest)]

    def covered(self, start, size):
        return any(base <= start and start + size <= base + length
                   for base, length in self.ranges())

    def test_exact_plain_draw_context_has_one_capture(self):
        self.assertEqual(self.value('kModelRenderContextStateStart'), CONTEXT)
        self.assertEqual(self.value('kModelRenderContextStateEnd'), CONTEXT_END)
        self.assertEqual(self.ranges().count((CONTEXT, CONTEXT_END - CONTEXT)), 1)
        self.assertIn('(kModelRenderContextStateEnd - kModelRenderContextStateStart)',
                      self.source)

    def test_tail_cleanup_fixed_and_small_data_roots_are_all_covered(self):
        for address, size in ((0x803CE0F0, 0xA10), (0x803CD4FC, 0xBF4),
                              (0x804A0AE0 + 0xE88, 12),
                              (0x804A0AE0 + 0xEE8, 4),
                              (0x804A0AE0 + 0xE80, 4),
                              (0x804A0AE0 + 0x720, 4)):
            self.assertTrue(self.covered(address, size), hex(address))

    def test_keep_boot_double_buffer_and_disc_request_excluded(self):
        self.assertFalse(self.covered(0x803989E0, 4))
        self.assertFalse(self.covered(0x803C8460, 4))


if __name__ == '__main__':
    unittest.main()
