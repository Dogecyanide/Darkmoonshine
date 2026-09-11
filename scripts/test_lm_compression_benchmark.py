"""Synthetic-only checks for private-fixture loading and benchmark accounting."""
import hashlib
from pathlib import Path
import tempfile
import unittest

try:
    from scripts import benchmark_lm_compression as benchmark
    from scripts import lm_codec_fixture as fixtures
    from scripts.test_lm_archive_compare import fixture25, reseal, put
except ModuleNotFoundError:
    import benchmark_lm_compression as benchmark
    import lm_codec_fixture as fixtures
    from test_lm_archive_compare import fixture25, reseal, put


class CompressionBenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lm-private-fixture-test-")
        self.path = Path(self.temp.name) / "synthetic.lms"
        self.data = fixture25(version=27)
        self.path.write_bytes(self.data)

    def tearDown(self):
        self.temp.cleanup()

    def test_real_parser_extracts_exact_companion_without_writing(self):
        before = self.path.read_bytes()
        fixture = fixtures.load_companion(self.path)
        self.assertEqual(fixture.parent, bytes(0x419B00))
        self.assertEqual(fixture.archive_sha256, hashlib.sha256(before).hexdigest())
        self.assertEqual(self.path.read_bytes(), before)
        self.assertEqual(list(Path(self.temp.name).iterdir()), [self.path])
        self.assertEqual(fixture.identity["raw_core_bytes"], len(fixture.core))

    def test_rejects_bad_integrity_before_benchmark(self):
        self.data[-20] ^= 1
        self.path.write_bytes(self.data)
        with self.assertRaises(fixtures.archives.ArchiveError):
            fixtures.load_companion(self.path)

    def test_rejects_noncurrent_capture_and_wrong_companion_size(self):
        self.path.write_bytes(fixture25(version=26))
        with self.assertRaisesRegex(fixtures.archives.ArchiveError, "format-27"):
            fixtures.load_companion(self.path)
        core = fixtures.archives.words(self.data, 80, 1)[0]
        put(self.data, 64 + core + 32, 0x419AFF)
        reseal(self.data)
        self.path.write_bytes(self.data)
        with self.assertRaises(fixtures.archives.ArchiveError):
            fixtures.load_companion(self.path)

    def test_report_metadata_omits_private_names_and_authentication(self):
        identity = fixtures.load_companion(self.path).identity
        text = repr(identity)
        for forbidden in ("synthetic.lms", "authHigh", "authLow", "session", "keyId", "path"):
            self.assertNotIn(forbidden, text)

    def test_native_synthetic_benchmark_roundtrip_crc_and_capacity_accounting(self):
        # No private fixture is necessary to verify executable harness contracts.
        bench = benchmark.NativeBenchmark()
        try:
            fixture = fixtures.load_companion(self.path)
            result = bench.run(fixture, 1, 1)
            self.assertEqual(result["parent_bytes"], 0x419B00)
            for metric in result["codecs"].values():
                self.assertTrue(metric["exact_roundtrip"])
                self.assertTrue(metric["fits"])
                rounded = (metric["packed_bytes"] + 31) & ~31
                self.assertEqual(metric["remaining_payload_bytes"],
                                 result["companion_capacity_bytes"] - rounded)
                self.assertEqual(metric["resulting_archive_bytes"],
                                 128 + len(fixture.core) + rounded + fixture.trailer_size)
                self.assertEqual(metric["compress"]["warm"]["samples"], 1)
                self.assertEqual(metric["validate_then_restore"]["evicted"]["samples"], 1)
                self.assertGreater(metric["compress"]["warm"]["median_ms"], 0)
        finally:
            bench.close()

    def test_pair_bytes_do_not_imply_safe_or_available_second_slot(self):
        fixtures = [{"two_slot_capacity": {"compressed_entire_payload": {
            "dense": {"packed_bytes": size, "fits_best_case_suffix": False}}}}
            for size in (6758939, 6960918, 6897288)]
        item = benchmark.pair_capacity(fixtures, 0xFD0000)["dense"]
        self.assertEqual(item["largest_distinct_pair_bytes"], 13858206)
        self.assertEqual(item["same_largest_twice_bytes"], 13921836)
        self.assertTrue(item["pair_fits_payload_only"])
        self.assertFalse(item["all_inactive_fit_best_case_suffix"])
        self.assertTrue(item["does_not_account_for_safe_switch_decode_or_rollback"])


if __name__ == "__main__":
    unittest.main()
