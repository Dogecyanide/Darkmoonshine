"""Offline archive parser tests use synthetic bounded images, never an emulator."""
import ast
import contextlib
import io
from pathlib import Path
import re
import struct
import tempfile
import unittest
import zlib

try:
    from scripts import compare_lm_archives as tool
except ModuleNotFoundError:
    import compare_lm_archives as tool

ROOT = Path(__file__).resolve().parents[1]


def put(data, offset, *values):
    struct.pack_into(f">{len(values)}I", data, offset, *values)


def fixture(version=24):
    ranges, camera_offset, game_offset = tool.schema(version)
    h = dict.fromkeys(tool.HEADER_FIELDS, 0)
    h.update(magic=0x4C4D5354, version=version, headerSize=256, gameId=0x474C4D4A,
             totalSize=game_offset + 0x400, generation=1, heap=0x80EFFF00,
             heapStart=0x81000000, heapEnd=0x81000400, heapSize=0x400,
             heapMetadataOffset=256, heapMetadataSize=0x48, stateStaticsOffset=0x148,
             stateStaticsSize=camera_offset - 0x148, heapDataOffset=game_offset,
             heapDataSize=0x400, rootHeap=0x804FF000, rootHeapStart=0x80500000,
             rootHeapEnd=0x81700000, rootHeapSize=0x1200000, systemHeap=0x80500000,
             systemHeapStart=0x80510000, systemHeapEnd=0x80D00000, systemHeapSize=0x7F0000,
             currentHeap=0x80EFFF00, missionMode=0x81000010, gameMode=0x81000010,
             gameModeCount=1, currentScene=0x80300000, usedHead=0x81000000, usedTail=0x81000000,
             mainLoopMode=2, audioBasic=0x803E3CF8)
    core = bytearray(h["totalSize"])
    put(core, 0, *(h[key] for key in tool.HEADER_FIELDS))
    put(core, 0x138, 0, 0, h["usedHead"], h["usedTail"])
    put(core, game_offset, 0x484D000D, 0x3F0, 0, 0)
    cursor = tool.STATICS_OFFSET
    for label, address, size in ranges:
        if label == "cameraManager":
            put(core, cursor + 0x80, 0x80B00100, 0x80B00200, 0x80B00300)
        cursor += size
    for index, target in enumerate((0x80B00100, 0x80B00200, 0x80B00300)):
        put(core, camera_offset + index * 0x100, target, 0xEC)
    put(core, 20, tool.crc_zeroed(core, ((0, 4), (20, 24))))
    packed = zlib.compress(bytes(tool.SHARED_SIZE))
    companion = bytearray(64) + packed
    put(companion, 0, 0x4C4D5343, 2 if version >= 28 else 1, 1, tool.words(core, 20, 1)[0], len(packed), 0,
        0x80B00000, 0x80600000, tool.SHARED_SIZE, h["systemHeap"], h["systemHeapStart"],
        h["systemHeapEnd"], 0x484D0010, 0x484D0010, 123, 456)
    put(companion, 20, tool.crc_zeroed(companion, ((20, 24),)))
    companion.extend(bytes((-len(packed)) % 32))
    trailer = bytearray(tool.TRAILER_SIZE)
    put(trailer, 0, 1, 1, 0, 0)
    put(trailer, 0x1828, 1, 1, 0, 7, 0)
    put(trailer, 0x18EC, 1, 1, 0, 0, 0)
    payload = core + companion + trailer
    outer = bytearray(64)
    put(outer, 0, 0x4C4D5341, 1, 64, len(payload), 0x474C4D4A, version, 0x1234, 0x5678,
        len(core) + len(companion), tool.TRAILER_SIZE, zlib.crc32(payload), 1, 99, 100, 0, 0)
    return outer + payload


def reseal(data):
    """Make deliberate structural mutations pass integrity checks, not authentication."""
    core = tool.words(data, 64 + 16, 1)[0]
    put(data, 64 + 20, tool.crc_zeroed(memoryview(data)[64:64 + core], ((0, 4), (20, 24))))
    put(data, 64 + core + 12, tool.words(data, 64 + 20, 1)[0])
    packed = tool.words(data, 64 + core + 16, 1)[0]
    put(data, 64 + core + 20, tool.crc_zeroed(memoryview(data)[64 + core:64 + core + 64 + packed], ((20, 24),)))
    put(data, 40, zlib.crc32(memoryview(data)[64:]))


def fixture25(envelope=2, profile=True, version=25):
    """Synthetic wire profile; zero anchors intentionally make no native proof."""
    data = fixture(version)
    put(data, 4, envelope)
    put(data, 20, version)
    put(data, 64 + 4, version)
    put(data, 36, tool.TRAILER_SIZE + tool.PROFILE_SIZE)
    raw = tool.words(data, 32, 1)[0]
    p = bytearray(tool.PROFILE_SIZE)
    if profile:
        node = 0x80501000
        for name in ("rootUsedHead", "rootUsedTail"):
            put(data, 64 + tool.HEADER_FIELDS.index(name) * 4, node)
        put(p, 0, tool.PROFILE_MAGIC, 1, 1, 0, 0x12345678, 0x804FF000, 0x80500000, 1)
        put(p, 288, 0x804FF000, node, 0x484D0001, 32, 0, 0)
        put(p, 12, tool.crc_zeroed(p, ((12, 16),)))
    data.extend(p)
    put(data, 12, len(data) - 64)
    if envelope == 2:
        put(data, 28, 0xBEEFF00D)
        put(data, 56, 0x4C4D5031, 0x12345678)
    assert len(data) == 64 + raw + tool.TRAILER_SIZE + tool.PROFILE_SIZE
    reseal(data)
    return data


def reseal_profile(data):
    pos = 64 + tool.words(data, 32, 1)[0] + tool.TRAILER_SIZE
    put(data, pos + 12, tool.crc_zeroed(memoryview(data)[pos:pos + tool.PROFILE_SIZE], ((12, 16),)))
    reseal(data)


class ArchiveCompareTests(unittest.TestCase):
    def setUp(self):
        self.data = fixture()

    def test_valid_synthetic_file_reports_limits_and_no_auth_claim(self):
        archive = tool.inspect_bytes(self.data)
        self.assertEqual(archive.facts["game.usedBlocks"], 1)
        self.assertIn("unavailable", archive.facts["camera.0.allocationEvidence"])
        self.assertIn("NOT verified", archive.warnings[0])
        self.assertFalse(tool.compare(archive, archive)["differences"])

    def test_each_integrity_layer_rejects_independent_corruption(self):
        core = tool.words(self.data, 80, 1)[0]
        cases = [(64 + tool.GAME_OFFSET + 20, "payload CRC"), (64 + 200, "core CRC"),
                 (64 + core + 64, "companion CRC")]
        for position, message in cases:
            data = bytearray(self.data); data[position] ^= 1
            if message != "payload CRC":
                put(data, 40, zlib.crc32(memoryview(data)[64:]))
            with self.subTest(message=message), self.assertRaisesRegex(tool.ArchiveError, message):
                tool.inspect_bytes(data)

    def test_truncation_extension_and_huge_declared_size_are_bounded(self):
        for cut in (0, 63, 64, 319, len(self.data) - 1):
            with self.assertRaises(tool.ArchiveError):
                tool.inspect_bytes(self.data[:cut])
        with self.assertRaises(tool.ArchiveError):
            tool.inspect_bytes(self.data + b"x")
        put(self.data, 12, 0xFFFFFFFF)
        with self.assertRaises(tool.ArchiveError):
            tool.inspect_bytes(self.data)

    def test_structural_core_census_and_camera_faults_survive_recomputed_crcs(self):
        raw = tool.words(self.data, 32, 1)[0]
        cases = [(64 + 4, 25), (64 + 60, tool.GAME_OFFSET + 32),
                 (64 + tool.CAMERA_OFFSET + 4, 1), (64 + tool.CAMERA_OFFSET, 0x817FFFFC),
                 (64 + tool.CAMERA_OFFSET + 0x100, 0x80B00100),
                 (64 + tool.GAME_OFFSET + 12, 0x81000000),
                 (64 + tool.GAME_OFFSET, 0xFFFF0000),
                 (64 + raw + 12, 65), (64 + raw + 0x1828 + 16, 25),
                 (64 + raw + 0x18EC, 2)]
        for position, value in cases:
            data = bytearray(self.data); put(data, position, value); reseal(data)
            with self.subTest(position=position), self.assertRaises(tool.ArchiveError):
                tool.inspect_bytes(data)

    def test_saved_camera_table_disagreement_is_rejected(self):
        data = bytearray(self.data)
        put(data, 64 + tool.CAMERA_OFFSET, 0x80B00400); reseal(data)
        with self.assertRaisesRegex(tool.ArchiveError, "static owner disagreement"):
            tool.inspect_bytes(data)

    def test_companion_descriptor_padding_and_zlib_bounds(self):
        core = tool.words(self.data, 80, 1)[0]
        for offset, value in ((0, 0), (4, 2), (32, 0xFFFFFFFF), (28, 0x81700000), (36, 0)):
            data = bytearray(self.data); put(data, 64 + core + offset, value); reseal(data)
            with self.subTest(offset=offset), self.assertRaises(tool.ArchiveError):
                tool.inspect_bytes(data)
        for packed in (zlib.compress(b"short"), zlib.compress(bytes(tool.SHARED_SIZE + 1)),
                       zlib.compress(bytes(tool.SHARED_SIZE)) + b"tail", b"not zlib"):
            with self.assertRaises(tool.ArchiveError):
                tool.decode_parent(packed)

    def test_different_build_is_never_a_controlled_comparison(self):
        left = tool.inspect_bytes(self.data)
        put(self.data, 24, 0x9999)
        result = tool.compare(left, tool.inspect_bytes(self.data))
        self.assertFalse(result["controlledBuildComparison"])
        self.assertIn("uncontrolled", result["warning"])

    def test_cli_reads_only_and_provides_json_without_authentication_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.lms"
            path.write_bytes(self.data)
            before = path.read_bytes()
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(tool.main([str(path), "--json"]), 0)
            self.assertIn("SipHash authentication NOT verified", output.getvalue())
            self.assertEqual(path.read_bytes(), before)

    def test_frozen_capture_manifest_matches_current_production_format(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        source = re.sub(r"//[^\n]*", "", source)
        constants = {}
        pending = re.findall(r"constexpr u(?:8|16|32) (\w+)\s*=\s*([^;]+);", source)

        def expression(text):
            text = re.sub(r"\b(0x[0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", text)
            text = text.replace("sizeof(u32)", "4")
            node = ast.parse(text.strip(), mode="eval").body
            def visit(value):
                if isinstance(value, ast.Constant): return value.value
                if isinstance(value, ast.Name): return constants[value.id]
                if isinstance(value, ast.BinOp):
                    left, right = visit(value.left), visit(value.right)
                    if isinstance(value.op, ast.Add): return left + right
                    if isinstance(value.op, ast.Sub): return left - right
                    if isinstance(value.op, ast.Mult): return left * right
                raise ValueError("not a simple captured-range expression")
            return visit(node)

        for _ in range(12):
            for name, value in pending:
                try: constants[name] = expression(value)
                except (SyntaxError, ValueError, KeyError): pass
        manifest = source.split("constexpr StaticRange kStateStaticRanges[] = {", 1)[1].split("};", 1)[0]
        actual = [(expression(start), expression(size)) for start, size in re.findall(r"\{([^,{}]+),([^{}]+)\}", manifest)]
        ranges, camera_offset, _ = tool.schema(constants["kSnapshotVersion"])
        self.assertEqual(actual, [(start, size) for _, start, size in ranges])
        self.assertIn(constants["kSnapshotVersion"], tool.SUPPORTED_FORMATS)
        self.assertEqual(sum(size for _, _, size in ranges) + tool.STATICS_OFFSET, camera_offset)

    def test_new_lookup_formats_preserve_old_schema_readability(self):
        for version in (25, 26, 27, 28):
            for envelope, profile in ((1, False), (1, True), (2, True)):
                image = fixture25(envelope, profile, version)
                result = tool.inspect_bytes(image)
                self.assertEqual(result.header["version"], version)
                self.assertEqual(result.facts["profile.present"], profile)
                self.assertEqual("static.roomInfo.crc" in result.facts, version >= 26)
                for name in ("furnitureInfo", "roomMapLookup", "roomMapUi"):
                    self.assertEqual("static." + name + ".crc" in result.facts, version >= 27)
                self.assertEqual(result.header["heapDataOffset"], tool.schema(version)[2])

    def test_relabelled_legacy_core_cannot_masquerade_as_format26(self):
        image = fixture25()
        put(image, 20, 26); put(image, 64 + 4, 26); reseal(image)
        with self.assertRaisesRegex(tool.ArchiveError, "section offsets"):
            tool.inspect_bytes(image)
        image = fixture25(version=26)
        put(image, 20, 25); put(image, 64 + 4, 25); reseal(image)
        with self.assertRaisesRegex(tool.ArchiveError, "section offsets"):
            tool.inspect_bytes(image)

    def test_format27_and26_cannot_be_relabelled_as_each_other(self):
        for source, target in ((26, 27), (27, 26)):
            image = fixture25(version=source)
            put(image, 20, target); put(image, 64 + 4, target); reseal(image)
            with self.subTest(source=source), self.assertRaisesRegex(tool.ArchiveError, "section offsets"):
                tool.inspect_bytes(image)

    def test_format28_companion_kind_binds_codec_upgrade(self):
        for source, target in ((27, 28), (28, 27)):
            image = fixture25(version=source)
            put(image, 20, target); put(image, 64 + 4, target); reseal(image)
            with self.assertRaisesRegex(tool.ArchiveError, "companion/core binding"):
                tool.inspect_bytes(image)

    def test_format28_lml4_exact_decode_checksum_and_trailing_bytes(self):
        image = fixture25(version=28)
        core = tool.words(image, 64 + 16, 1)[0]
        old_raw = tool.words(image, 32, 1)[0]
        trailer = image[64 + old_raw:]
        parent = bytes(tool.SHARED_SIZE)
        packed = bytearray(b"LML4" + struct.pack(">I", 0x20000))
        for offset in range(0, len(parent), 0x20000):
            block = parent[offset:offset + 0x20000]
            packed.extend(struct.pack(">II", len(block), 0x80000000 | len(block)))
            packed.extend(block)
        packed.extend(struct.pack(">I", zlib.adler32(parent)))
        def rebuild(stream):
            result = image[:64 + core + 64] + stream + bytes((-len(stream)) % 32) + trailer
            put(result, 64 + core + 16, len(stream))
            put(result, 12, len(result) - 64)
            put(result, 32, len(result) - 64 - len(trailer))
            reseal(result)
            return result
        result = tool.inspect_bytes(rebuild(packed))
        self.assertEqual(result.facts["shared.codec"], "LML4")
        self.assertEqual(result.facts["shared.payloadCrc"], zlib.crc32(parent))
        for malformed in (packed[:-1], packed + b"X", packed[:-1] + bytes([packed[-1] ^ 1])):
            with self.assertRaisesRegex(tool.ArchiveError, "LML4"):
                tool.inspect_bytes(rebuild(malformed))
    def test_optional_controlled_wii_pair_has_verified_game_camera_extents(self):
        directory = ROOT.parent / "sd-captures/lm-0.3.40-reboot-pair-20260907/lm_states"
        paths = [directory / f"archive_{number:08}.lms" for number in (3, 4)]
        if not all(path.exists() for path in paths):
            self.skipTest("Private controlled Wii reboot pair unavailable")
        left, right = map(tool.inspect_archive, paths)
        self.assertTrue(tool.compare(left, right)["controlledBuildComparison"])
        self.assertEqual(left.shared, right.shared)
        self.assertEqual(left.facts["shared.payloadCrc"], right.facts["shared.payloadCrc"])
        for archive in (left, right):
            for index in range(3):
                prefix = f"camera.{index}."
                self.assertEqual(archive.facts[prefix + "capturedBytes"], 0)
                self.assertEqual(archive.facts[prefix + "allocationSize"], 0xEC)
                self.assertEqual(archive.facts[prefix + "allocationOffset"], 0)

    def test_format25_v1_zero_or_valid_profile_and_v2_profile_round_trip(self):
        for envelope, present in ((1, False), (1, True), (2, True)):
            with self.subTest(envelope=envelope, profile=present):
                data = fixture25(envelope, present)
                archive = tool.inspect_bytes(data)
                self.assertEqual(archive.header["version"], 25)
                self.assertEqual(archive.envelope["trailerSize"], 0x1EA0)
                self.assertEqual(archive.facts["profile.present"], present)
                self.assertIn("NOT verified", archive.warnings[0])
                self.assertEqual(tool.compare(archive, archive)["differences"], [])
                if present:
                    self.assertEqual(archive.profile["count"], 1)
                    self.assertEqual(len(archive.profile["anchors"]), 49)
                    self.assertEqual(archive.facts["profile.rootAllocations"], 1)
                    self.assertEqual(archive.facts["profile.systemAllocations"], 0)
                if envelope == 2:
                    self.assertEqual(archive.facts["archive.keyId"], 0xBEEFF00D)
                    self.assertNotIn("archive.session", archive.facts)
                else:
                    self.assertIn("archive.session", archive.facts)

    def test_v2_rejects_zero_profile_or_legacy24_core(self):
        with self.assertRaisesRegex(tool.ArchiveError, "requires a reboot profile"):
            tool.inspect_bytes(fixture25(2, False))
        data = fixture()
        put(data, 4, 2); put(data, 56, 0x4C4D5031, 1)
        with self.assertRaisesRegex(tool.ArchiveError, "v2"):
            tool.inspect_bytes(data)

    def test_format25_profile_size_cannot_be_short_long_or_legacy_trailer(self):
        data = fixture25()
        for candidate in (data[:-4], data + b"\0" * 4):
            put(candidate, 12, len(candidate) - 64)
            put(candidate, 36, len(candidate) - 64 - tool.words(candidate, 32, 1)[0])
            put(candidate, 40, zlib.crc32(memoryview(candidate)[64:]))
            with self.assertRaisesRegex(tool.ArchiveError, "extents"):
                tool.inspect_bytes(candidate)
        put(data, 36, tool.TRAILER_SIZE)
        with self.assertRaisesRegex(tool.ArchiveError, "extents"):
            tool.inspect_bytes(data)

    def test_v2_key_and_config_envelope_fields_must_be_present(self):
        for offset, value in ((28, 0), (56, 0), (56, 0xFFFFFFFF), (60, 0)):
            data = fixture25(); put(data, offset, value)
            with self.subTest(offset=offset), self.assertRaisesRegex(tool.ArchiveError, "key/config"):
                tool.inspect_bytes(data)
        data = fixture25(1); put(data, 60, 1)
        with self.assertRaisesRegex(tool.ArchiveError, "reserved"):
            tool.inspect_bytes(data)

    def test_profile_crc_checks_wire_bytes_with_checksum_zeroed(self):
        data = fixture25()
        pos = len(data) - tool.PROFILE_SIZE
        for offset in (12, 32, 288 + 12, tool.PROFILE_SIZE - 1):
            damaged = bytearray(data); damaged[pos + offset] ^= 1
            reseal(damaged)
            with self.subTest(offset=offset), self.assertRaisesRegex(tool.ArchiveError, "profile CRC"):
                tool.inspect_bytes(damaged)

    def test_recomputed_profile_crc_cannot_hide_invalid_shape(self):
        cases = ((0, 0), (4, 2), (8, 2), (16, 0), (16, 0x9999),
                 (20, 0x804FF004), (24, 0x80500004), (28, 0), (28, 49),
                 (32 + 49 * 4, 1), (288 + 24, 1), (288, 0),
                 (288 + 4, 0x817FFFFC), (288 + 8, 0xFFFF0000),
                 (288 + 12, 0xFFFFFFFF), (288 + 16, 0x80501000), (288 + 20, 0x80501000))
        for offset, value in cases:
            data = fixture25(); put(data, len(data) - tool.PROFILE_SIZE + offset, value)
            reseal_profile(data)
            with self.subTest(offset=offset, value=value), self.assertRaises(tool.ArchiveError):
                tool.inspect_bytes(data)

    def test_v1_nonzero_profile_is_validated_too(self):
        data = fixture25(1)
        pos = len(data) - tool.PROFILE_SIZE
        put(data, pos, 0); reseal_profile(data)
        with self.assertRaisesRegex(tool.ArchiveError, "magic/version"):
            tool.inspect_bytes(data)

    def test_typed_profile_anchor_differences_and_retained_counts_are_visible(self):
        original = fixture25()
        changed = bytearray(original)
        offset = tool.PROFILE_ANCHORS.index("audio.pool1.count") * 4 + 32
        put(changed, len(changed) - tool.PROFILE_SIZE + offset, 3)
        reseal_profile(changed)
        left, right = tool.inspect_bytes(original), tool.inspect_bytes(changed)
        differences = {d["field"]: d for d in tool.compare(left, right)["differences"]}
        field = "profile.anchor.audio.pool1.count"
        self.assertEqual(differences[field], {"field": field, "left": 0, "right": 3})
        self.assertEqual(right.facts["profile.rootAllocations"], 1)
        self.assertEqual(right.facts["profile.systemAllocations"], 0)

    def test_profile_heap_lists_are_bounded_linked_and_non_overlapping(self):
        data = fixture25()
        pos = len(data) - tool.PROFILE_SIZE
        put(data, pos + 28, 2)
        put(data, pos + 288 + 20, 0x80501020)
        put(data, pos + 312, 0x804FF000, 0x80501020, 0x484D0001, 32, 0x80501000, 0)
        put(data, 64 + tool.HEADER_FIELDS.index("rootUsedTail") * 4, 0x80501020)
        reseal_profile(data)
        with self.assertRaisesRegex(tool.ArchiveError, "overlap"):
            tool.inspect_bytes(data)

    def test_v2_inspection_never_reads_key_files_or_claims_authentication(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.lms"
            path.write_bytes(fixture25())
            key = Path(directory) / "archive_key0.bin"
            key.write_bytes(b"not a key; deliberately ignored")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(tool.main([str(path), "--json"]), 0)
            self.assertIn("SipHash authentication NOT verified", output.getvalue())
            self.assertIn('"archive.keyId"', output.getvalue())
            self.assertNotIn('"archive.session"', output.getvalue())
            self.assertEqual(key.read_bytes(), b"not a key; deliberately ignored")
            put_data = fixture25(); put(put_data, 48, 0); put(put_data, 52, 0)
            self.assertIn("NOT verified", tool.inspect_bytes(put_data).warnings[0])


if __name__ == "__main__":
    unittest.main()
