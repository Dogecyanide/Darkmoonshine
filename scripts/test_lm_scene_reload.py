"""Compiled ownership rules for same-map native Mission rebuilds."""
import ctypes
from pathlib import Path
import struct
import unittest

import test_lm_state_resource as resource_tests
import test_lm_hud_state as retail_tests

ROOT = Path(__file__).resolve().parents[1]


class SceneReloadNativeTests(unittest.TestCase):
    def test_relocated_resource_layout_has_two_bounded_owner_proofs(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        layout = source.split("bool resourceLayoutCompatible(", 1)[1].split("bool guardedCrossRoomRestoreAllowed", 1)[0]
        self.assertEqual(layout.count("LmStateResourceRootsValidate("), 2)
        guard = source.split("bool guardedCrossRoomRestoreAllowed", 1)[1].split("void repairSavedVolumeList", 1)[0]
        self.assertIn("!resourceLayoutCompatible(header, live)", guard)
        self.assertIn("!resourceReplacementMatches(header, live)", guard)
        self.assertIn("sResourceDiff.mapChanged != 0u", guard)

    @classmethod
    def setUpClass(cls):
        resource_tests.LmStateResourceNativeTests.setUpClass.__func__(cls)
        cls.lib.settled.argtypes = [ctypes.c_ulong, ctypes.c_uint] + [ctypes.c_ulong] * 3 + [ctypes.c_void_p]
        cls.lib.model_endpoint.argtypes = [ctypes.c_ulong] * 7
        cls.lib.model_changes.argtypes = [ctypes.c_void_p] * 4
        cls.lib.reload_changes.argtypes = [ctypes.c_uint] * 2 + [ctypes.c_void_p] * 4 + [ctypes.c_ulong] * 5

    @classmethod
    def tearDownClass(cls):
        resource_tests.LmStateResourceNativeTests.tearDownClass.__func__(cls)

    @staticmethod
    def record(**overrides):
        values = [0x200, 0x80011B28, 0, 0x8157C434, 0x8157CD04,
                  0x81349DD8, 0x80E99FA0, 0, 34, 0, 0, 0, 0, 0, 0, 0x1204002B]
        for key, value in overrides.items():
            values[int(key)] = value
        return struct.pack(">16I", *values)

    def settled(self, record):
        return self.lib.settled(34, 0, 0x80E99FA0, 0x80BE4560, 0x817FB140, record)

    def reload(self, saved, live, active=0, records=1):
        ids = (ctypes.c_ulong * 7)(34, *([0xFFFFFFFF] * 6))
        return self.lib.reload_changes(active, records, ids, ids,
            saved + bytes(384), live + bytes(384), 0x80E99FA0, 0x80E99FA0,
            0x70800, 0x80BE4560, 0x817FB140)

    def test_same_room_id_recreated_arrays_are_rewindable(self):
        before = self.record()
        after = self.record(**{"3": 0x815C0040, "4": 0x815C1200, "5": 0x815B1000})
        self.assertEqual(self.settled(before), 1)
        self.assertEqual(self.settled(after), 1)
        self.assertEqual(self.reload(before, after), 1)
        self.assertEqual(self.reload(after, before), 1)

    def test_pending_cleanup_callback_and_wrong_room_remain_refused(self):
        for word, value in ((0, 0x100), (0, 0), (0, 0x300), (1, 0x80011B2C),
                            (2, 0x815C0010), (6, 0x80E99FC0), (7, 1), (8, 35)):
            with self.subTest(word=word, value=value):
                bad = self.record(**{str(word): value})
                self.assertEqual(self.settled(bad), 0)
                self.assertEqual(self.reload(self.record(), bad), 0)

    def test_owned_resource_arrays_cannot_escape_game_or_be_unaligned(self):
        for index in (3, 4, 5):
            for pointer in (0x80000100, 0x80538550, 0x817FB140, 0xFFFFFFFF, 0x815C0041):
                self.assertEqual(self.settled(self.record(**{str(index): pointer})), 0)
            self.assertEqual(self.settled(self.record(**{str(index): 0})), 0)
        self.assertEqual(self.settled(self.record(**{"3": 0, "4": 0, "5": 0, "15": 0})), 1)

    def test_resource_array_extents_and_vector_headers_must_fit(self):
        for index in (3, 4, 5):
            self.assertEqual(self.settled(self.record(**{str(index): 0x817FB138})), 0)
        self.assertEqual(self.settled(self.record(**{"5": 0x817FB130})), 1)
        for index in (3, 4):
            self.assertEqual(self.settled(self.record(**{str(index): 0x80BE4560})), 0)

    def test_unknown_resource_words_and_bad_masks_remain_refused(self):
        for index in range(9, 15):
            self.assertEqual(self.settled(self.record(**{str(index): 1})), 0)
        for active, records in ((0x80, 0x80), (1, 0), (0, 0x80)):
            self.assertEqual(self.reload(self.record(), self.record(), active, records), 0)

    def endpoint(self, **overrides):
        values = dict(state=3, handle=0x8157C100, registry=0x8157C100,
                      root=0x815B8280, registryRoot=0x815C41C0,
                      archive=0x815A0440, size=147648)
        values.update(overrides)
        return self.lib.model_endpoint(*values.values())

    def test_loaded_to_loaded_replacement_validates_each_side(self):
        self.assertEqual(self.endpoint(), 1)
        self.assertEqual(self.endpoint(handle=0x81358000, registry=0x81358000,
            root=0x81361000, registryRoot=0, archive=0x81360000, size=0x20000), 1)
        self.assertEqual(self.endpoint(state=0, handle=0, registry=0,
            root=0, registryRoot=0, archive=0, size=0), 1)

    def test_model_request_states_and_dangling_roots_are_rejected(self):
        for state in (0, 1, 2, 4, 0xFFFFFFFF):
            self.assertEqual(self.endpoint(state=state), 0)
        for root in (0x80538550, 0x815A043C, 0x815C4500, 0xFFFFFFFF, 0x815B8281):
            self.assertEqual(self.endpoint(root=root), 0)
            self.assertEqual(self.endpoint(registryRoot=root), 0)
        self.assertEqual(self.endpoint(registry=0), 0)
        self.assertEqual(self.endpoint(registry=0x8157C104), 0)

    def test_only_retail_mutable_model_fields_may_change(self):
        saved = (ctypes.c_ulong * 13)(*range(13))
        live = (ctypes.c_ulong * 13)(*range(13))
        reg = (ctypes.c_ulong * 16)(*range(16))
        new_reg = (ctypes.c_ulong * 16)(*range(16))
        for i in range(13):
            live[i] ^= 0x100
            self.assertEqual(self.lib.model_changes(saved, live, reg, new_reg),
                             int(i in (1, 2, 3, 5, 11, 12)))
            live[i] ^= 0x100
        for i in range(16):
            new_reg[i] ^= 0x100
            self.assertEqual(self.lib.model_changes(saved, live, reg, new_reg),
                             int(i in (1, 3)))
            new_reg[i] ^= 0x100

    def test_scene_rebuild_requires_every_changed_archive_owner(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        function = source.split("bool modelReplacementMatches(")[1].split("bool resourceReplacementMatches")[0]
        self.assertNotIn("changedCount != removed + added", function)
        for name in ("matchModelEndpoint(sSavedModelCensus", "matchModelEndpoint(sLiveModelCensus",
                     "changedModelWordsAreKnown(change.index)", "matchVrArchive(header, true",
                     "matchVrArchive(header, false", "everyChangedVolumeMatched(matchedRemoved",
                     "everyChangedVolumeMatched(matchedAdded"):
            self.assertIn(name, function)
        self.assertIn("kVrSceneOwnerStateStart = 0x803C24E8u", source)
        self.assertIn("kVrSceneOwnerStateEnd = 0x803C26C8u", source)
        self.assertIn("kVrSceneArchiveGlobal = 0x804A0F10u", source)
        self.assertIn("kSnapshotVersion = 28u", source)

    def test_each_request_clears_old_extended_refusal(self):
        source = (ROOT / "lm_diag/src/lm_state.cpp").read_text()
        for function in ("saveState", "loadState"):
            entry = source.split("void " + function + "() {")[1].split("clearEpochMismatch();")[0]
            self.assertIn("sCrossRoomFault = 0u;", entry)
            self.assertIn("sCrossRoomFaultValue = 0u;", entry)


class SceneReloadRetailTests(unittest.TestCase):
    setUpClass = classmethod(retail_tests.AuthenticatedRetailTests.setUpClass.__func__)
    data = retail_tests.AuthenticatedRetailTests.data
    word = retail_tests.AuthenticatedRetailTests.word
    words = retail_tests.AuthenticatedRetailTests.words
    call = retail_tests.AuthenticatedRetailTests.call

    def test_vr_archive_and_private_heap_share_scene_lifetime(self):
        self.words({0x8002F0D4: 0x3BE324E8, 0x8002F108: 0x906D042C,
                    0x8002F570: 0x906D0430, 0x8002F9C0: 0x806D0430,
                    0x8002F9D8: 0x818C000C, 0x8002F9E8: 0x900D0430})
        self.call(0x8002F100, 0x801CADDC)
        self.call(0x8002F10C, 0x801C8E94)
        self.call(0x8002F56C, 0x800066A8)
        self.call(0x8002F9BC, 0x801CAE50)

    def test_vr_output_arrays_and_scalar_animation_partners(self):
        self.words({0x8002F1A4: 0x3B5F0140, 0x8002F1CC: 0x907A0000,
                    0x8002F1F4: 0x3B5F0000, 0x8002F220: 0x907A0000,
                    0x8002F24C: 0x3B5F00A0, 0x8002F274: 0x907A0000,
                    0x8002F574: 0x387F0050, 0x8002F5A8: 0x387F00F0,
                    0x8002F5D8: 0x387F0190, 0x8002F62C: 0x3B8324E8})
        for site in (0x8002F1C8, 0x8002F21C, 0x8002F270):
            self.call(site, 0x801C0178)

    def test_room_record_is_completed_only_after_callback_unmount(self):
        self.words({0x8001F100: 0x38000002, 0x8001F104: 0x981F0002,
                    0x80011B44: 0x83E30008, 0x80011BB0: 0x818C000C,
                    0x80011BBC: 0x38000000, 0x80011BC0: 0x901D0008,
                    0x80011E34: 0x7F63012E, 0x80011E54: 0x7D07312E})
        self.call(0x80011E64, 0x8001F2B4)
        self.words({0x8001F464: 0x1C7F007C, 0x8001F480: 0x38C0007C,
                    0x8001F488: 0x907E000C, 0x8001F650: 0x1C7F0024,
                    0x8001F670: 0x38C00024, 0x8001F678: 0x907E0010,
                    0x8001F684: 0x907E0014, 0x8001F4F0: 0x881E003C,
                    0x8001F500: 0x1C00007C, 0x8001F6FC: 0x881D003D,
                    0x8001F70C: 0x1C000024})

    def test_model_primary_and_registry_owners_are_rebuilt_not_os_queues(self):
        self.words({0x8006106C: 0x38000003, 0x80061070: 0x901F0030,
                    0x80061430: 0x935F0008, 0x800615AC: 0x907F0004,
                    0x80061620: 0x91830030, 0x80061628: 0x91830004,
                    0x8006162C: 0x91830008, 0x80185CF4: 0x93A30004,
                    0x80185DB0: 0x93DF0004, 0x80185DB4: 0x93DF000C})
        self.call(0x800613A8, 0x80185CB4)


if __name__ == "__main__":
    unittest.main()
