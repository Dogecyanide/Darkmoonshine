#!/usr/bin/env python3
"""Host contracts for the authenticated GLMJ01 hardware diagnostic."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
PATCHES_PATH = ROOT / "lm_diag" / "patches.py"
STATE_SOURCE = (ROOT / "lm_diag" / "src" / "lm_state.cpp").read_text(
    encoding="utf-8"
)
DIAG_SOURCE = (ROOT / "lm_diag" / "src" / "lm_diag.cpp").read_text(
    encoding="utf-8"
)
CRASH_SOURCE = (ROOT / "lm_diag" / "src" / "lm_crash.cpp").read_text(
    encoding="utf-8"
)
KERNEL_CRASH_SOURCE = (ROOT / "launcher" / "kernel" / "SusamuneCrash.c").read_text(
    encoding="utf-8"
)
CRASH_HEADER = (ROOT / "include" / "susamune" / "crash_report.h").read_text(
    encoding="utf-8"
)

spec = importlib.util.spec_from_file_location("lm_diag_patches_test", PATCHES_PATH)
assert spec and spec.loader
lm_diag = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lm_diag)


class LuigiMansionDiagnosticContracts(unittest.TestCase):
    def test_authenticated_hook_contract(self) -> None:
        hooks = [
            (entry["lmj"], entry["sym"], entry["type"].name, entry["expected"])
            for entry in lm_diag.patches
        ]
        self.assertEqual(
            hooks,
            [
                (0x801D5B5C, "getArenaLo", "B", 0x806DFF38),
                (0x8000776C, "diagnosticCopyDisp", "BL", 0x481E8CF1),
                (0x80007828, "diagnosticCopyDisp", "BL", 0x481E8C35),
                (0x8000B534, "diagnosticFrameBegin", "BL", 0x4BFFC1A5),
                (0x8000B544, "diagnosticMainSceneStep", "BL", 0x4BFFFD05),
                (0x8000B268, "diagnosticFirstPosMatrix", "BL", 0x481E99C9),
                (0x8000B34C, "diagnosticLastNrmMatrix", "BL", 0x481E9921),
                (0x8000B35C, "diagnosticSceneDraw", "BL", 0x4E800021),
                (
                    0x800111A8,
                    "diagnosticAnimatedModelPoolUpdate",
                    "BL",
                    0x480155A9,
                ),
                (
                    0x8002684C,
                    "diagnosticAnimatedModelControllerUpdate",
                    "BL",
                    0x4BFF8239,
                ),
                (0x8000B360, "diagnosticOrthoReset", "BL", 0x4BFFC59D),
                (0x8000B5EC, "diagnosticPreMainUpdate", "BL", 0x4BFFF6B9),
                (0x8000B608, "diagnosticPostMainUpdate", "BL", 0x4BFFC9FD),
                (0x8000B618, "diagnosticAudioTailB618", "BL", 0x4817B19D),
                (0x8000B62C, "diagnosticChangeFrameBuffer", "BL", 0x4BFFC1BD),
                (0x8000B640, "diagnosticConditionalTail", "BL", 0x4BFFC975),
                (0x8000B65C, "diagnosticLoopTailSync", "BL", 0x4BFFFD1D),
                (0x8000B660, "diagnosticLoopTailClock", "BL", 0x4BFFA7A5),
                (0x8000B714, "diagnosticGameLoop", "BL", 0x4BFFFDD5),
                (0x8000B728, "diagnosticOuterCleanup", "BL", 0x4BFFF551),
                (0x8000B744, "diagnosticOuterRestart", "BL", 0x4BFFA92D),
            ],
        )

    def test_revision_checks_cover_presenter_input_and_crash_setter(self) -> None:
        self.assertEqual(
            lm_diag.checks,
            [
                {"addr": 0x801D5B60, "expected": 0x4E800020},
                {"addr": 0x80007870, "expected": 0x481CCFC1},
                {"addr": 0x8000B350, "expected": 0x806D8038},
                {"addr": 0x8000B354, "expected": 0x8183001C},
                {"addr": 0x8000B358, "expected": 0x7D8803A6},
                {"addr": 0x801D20B0, "expected": 0x387D0018},
                {"addr": 0x801D20B4, "expected": 0x48012849},
                {"addr": 0x801D4124, "expected": 0x800D1594},
                {"addr": 0x801D4128, "expected": 0x906D1594},
                {"addr": 0x801D412C, "expected": 0x7C030378},
                {"addr": 0x801D4130, "expected": 0x4E800020},
                {"addr": 0x801DAE98, "expected": 0x7C0802A6},
                {"addr": 0x801DAE9C, "expected": 0x90010004},
                {"addr": 0x801DAEA0, "expected": 0x9421FFF0},
                {"addr": 0x801DAED8, "expected": 0x7C0802A6},
                {"addr": 0x801DAEDC, "expected": 0x90010004},
                {"addr": 0x801DAEE0, "expected": 0x9421FFF0},
                {"addr": 0x8018B810, "expected": 0x93ED12F0},
                {"addr": 0x8018D4E4, "expected": 0x7C0802A6},
                {"addr": 0x8018D510, "expected": 0x806301E8},
                {"addr": 0x8018D54C, "expected": 0x800DF94C},
                {"addr": 0x8018D5F0, "expected": 0x901E0064},
                {"addr": 0x8018D5F8, "expected": 0x389D0800},
                {"addr": 0x8018D61C, "expected": 0x38BE0064},
                {"addr": 0x8018D62C, "expected": 0x4BFFF1F1},
                {"addr": 0x8018D630, "expected": 0x801E0050},
                {"addr": 0x804A03A8, "expected": 0x803E3CF8},
            ],
        )
        addresses = [entry["lmj"] for entry in lm_diag.patches]
        addresses.extend(entry["addr"] for entry in lm_diag.checks)
        self.assertEqual(len(addresses), len(set(addresses)))

    def test_mem1_reservation_contract(self) -> None:
        self.assertEqual(lm_diag.game_id["lmj"], 0x474C4D4A)
        self.assertEqual(lm_diag.base_addr["lmj"], 0x804B8400)
        self.assertEqual(lm_diag.dol_size["lmj"], 0x00394924)
        self.assertEqual(lm_diag.dol_min["lmj"], 0x00003100)
        self.assertEqual(lm_diag.dol_max["lmj"], 0x004A6400)
        self.assertEqual(lm_diag.mod_region_size, 0x80000)
        self.assertEqual(lm_diag.arena_reserve, 0x82000)
        self.assertLessEqual(
            lm_diag.mod_attachment_heap_offset + lm_diag.mod_attachment_heap_size,
            lm_diag.mod_region_size - lm_diag.mod_scratch_size,
        )

    def test_state_slot_and_split_heap_contract(self) -> None:
        self.assertIn(
            "kSnapshotBase = SUSAMUNE_MEM2_SNAPSHOT_PPC_BASE", STATE_SOURCE
        )
        self.assertIn(
            "kSnapshotStorageSize = SUSAMUNE_MEM2_SNAPSHOT_SIZE", STATE_SOURCE
        )
        self.assertIn(
            "kSnapshotStorageSize - kModelCensusScratchSize", STATE_SOURCE
        )
        self.assertIn("kHeapMetadataStart = 0x3Cu", STATE_SOURCE)
        self.assertIn("kHeapMetadataEnd = 0x84u", STATE_SOURCE)
        self.assertIn("kExpHeapAlignment = 16u", STATE_SOURCE)
        self.assertIn("kSnapshotVersion = 15u", STATE_SOURCE)
        self.assertIn("kTransitionHeaderStateStart = 0x803985D4u", STATE_SOURCE)
        self.assertIn("kTransitionHeaderStateEnd = 0x803985E8u", STATE_SOURCE)
        self.assertIn("kTransitionTailStateStart = 0x80398764u", STATE_SOURCE)
        self.assertIn("kTransitionTailStateEnd = 0x80398770u", STATE_SOURCE)
        self.assertIn(
            "{kTransitionHeaderStateStart,\n"
            "     kTransitionHeaderStateEnd - kTransitionHeaderStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "{kTransitionTailStateStart,\n"
            "     kTransitionTailStateEnd - kTransitionTailStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kRendererStateStart = 0x80398770u", STATE_SOURCE)
        self.assertIn("kRendererStateEnd = 0x803989E0u", STATE_SOURCE)
        self.assertIn(
            "{kRendererStateStart, kRendererStateEnd - kRendererStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kCameraDescriptorStateStart = 0x80398BF8u", STATE_SOURCE)
        self.assertIn("kCameraDescriptorStateEnd = 0x80398C50u", STATE_SOURCE)
        self.assertIn("kCameraManagerStateStart = 0x80399B60u", STATE_SOURCE)
        self.assertIn("kCameraManagerStateEnd = 0x80399C60u", STATE_SOURCE)
        self.assertIn(
            "{kCameraDescriptorStateStart,\n"
            "     kCameraDescriptorStateEnd - kCameraDescriptorStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "{kCameraManagerStateStart,\n"
            "     kCameraManagerStateEnd - kCameraManagerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kRoomEventStateStart = 0x803C7CA0u", STATE_SOURCE)
        self.assertIn("kRoomEventStateEnd = 0x803C8428u", STATE_SOURCE)
        self.assertIn(
            "{kRoomEventStateStart, kRoomEventStateEnd - kRoomEventStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kRoomActorTableStart = 0x803C8490u", STATE_SOURCE)
        self.assertIn("kRoomActorTableEnd = 0x803C8690u", STATE_SOURCE)
        self.assertIn("kRoomActorCountGlobal = 0x804A12B8u", STATE_SOURCE)
        self.assertIn("kRoomActorCapacity = 0x80u", STATE_SOURCE)
        self.assertIn(
            "{kRoomActorTableStart, kRoomActorTableEnd - kRoomActorTableStart}",
            STATE_SOURCE,
        )
        self.assertNotIn("kInGameFlagsOffset", STATE_SOURCE)
        self.assertIn("kDoorVisibilityStateStart = 0x80399510u", STATE_SOURCE)
        self.assertIn("kDoorVisibilityStateEnd = 0x80399B30u", STATE_SOURCE)
        self.assertIn(
            "{kDoorVisibilityStateStart,\n"
            "     kDoorVisibilityStateEnd - kDoorVisibilityStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "kRoomVisibilityMaskStateStart = 0x803C2E10u", STATE_SOURCE
        )
        self.assertIn(
            "kRoomVisibilityMaskStateEnd = 0x803C3030u", STATE_SOURCE
        )
        self.assertIn(
            "{kRoomVisibilityMaskStateStart,\n"
            "     kRoomVisibilityMaskStateEnd - kRoomVisibilityMaskStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kEventActiveStateStart = 0x803C20C8u", STATE_SOURCE)
        self.assertIn("kEventActiveStateEnd = 0x803C2138u", STATE_SOURCE)
        self.assertIn(
            "{kEventActiveStateStart,\n"
            "     kEventActiveStateEnd - kEventActiveStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kGrainManagerStateStart = 0x803CBAF0u", STATE_SOURCE)
        self.assertIn("kGrainManagerStateEnd = 0x803CC460u", STATE_SOURCE)
        self.assertIn(
            "{kGrainManagerStateStart,\n     kGrainManagerStateEnd - kGrainManagerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("kParticleManagerStateStart = 0x803CD4FCu", STATE_SOURCE)
        self.assertIn("kParticleManagerStateEnd = 0x803CE0F0u", STATE_SOURCE)
        self.assertIn(
            "{kParticleManagerStateStart,\n"
            "     kParticleManagerStateEnd - kParticleManagerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "kEffectControllerStateStart = 0x803CE0F0u", STATE_SOURCE
        )
        self.assertIn(
            "kEffectControllerStateEnd = 0x803CEB00u", STATE_SOURCE
        )
        self.assertIn(
            "{kEffectControllerStateStart,\n"
            "     kEffectControllerStateEnd - kEffectControllerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "kSceneEffectManagerStateStart = 0x803CD1F4u", STATE_SOURCE
        )
        self.assertIn(
            "kSceneEffectManagerStateEnd = 0x803CD4C8u", STATE_SOURCE
        )
        self.assertIn(
            "{kSceneEffectManagerStateStart,\n"
            "     kSceneEffectManagerStateEnd - "
            "kSceneEffectManagerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn(
            "{kAnimatedModelOwnerStateStart,\n"
            "     kAnimatedModelOwnerStateEnd - kAnimatedModelOwnerStateStart}",
            STATE_SOURCE,
        )
        self.assertIn("bool particleManagerValid", STATE_SOURCE)
        self.assertIn("Gate::Particle", STATE_SOURCE)
        self.assertIn('return "PTCL";', STATE_SOURCE)
        self.assertIn(
            "particleManagerValid(*identity, &particleFault)", STATE_SOURCE
        )
        self.assertIn("kMainLoopStateBase = 0x80398A40u", STATE_SOURCE)
        self.assertIn("kMainLoopStateSize = 0x08u", STATE_SOURCE)
        self.assertIn("kMainLoopSceneGlobal = 0x804A0C20u", STATE_SOURCE)
        self.assertIn("kMainLoopExitGlobal = 0x804A0C28u", STATE_SOURCE)
        self.assertIn("kMainDrawStateGlobal = 0x804A0C44u", STATE_SOURCE)
        self.assertIn("kMatrixArrayGlobal = 0x804A17B8u", STATE_SOURCE)
        self.assertIn("kBooleanArrayGlobal = 0x804A17BCu", STATE_SOURCE)
        self.assertIn("kSimpleModelerGlobal = 0x804A17D0u", STATE_SOURCE)
        self.assertIn("kMapColGlobal = 0x804A17D8u", STATE_SOURCE)
        self.assertIn("kEnTypesManagerGlobal = 0x804A17E8u", STATE_SOURCE)
        self.assertIn("kGameSdata0Start = 0x80498AF8u", STATE_SOURCE)
        self.assertIn("kGameSdata0End = 0x80498B18u", STATE_SOURCE)
        self.assertIn("kGameSdata1Start = 0x80498B20u", STATE_SOURCE)
        self.assertIn("kGameSdata1End = 0x804A03A8u", STATE_SOURCE)
        self.assertIn("kGameSbss0Start = 0x804A0C00u", STATE_SOURCE)
        self.assertIn("kGameSbss0End = 0x804A0C90u", STATE_SOURCE)
        self.assertIn("kGameSbss1Start = 0x804A0CB0u", STATE_SOURCE)
        self.assertIn("kGameSbss1End = 0x804A1D10u", STATE_SOURCE)
        self.assertIn("kStateStaticsSize == 0x160ACu", STATE_SOURCE)
        self.assertIn("kCameraObjectStateOffset == 0x161F4u", STATE_SOURCE)
        self.assertIn("kCameraObjectStateSize == 0x300u", STATE_SOURCE)
        self.assertIn("kHeapDataOffset == 0x16500u", STATE_SOURCE)
        self.assertIn("roomActorCount > kRoomActorCapacity", STATE_SOURCE)
        self.assertIn(
            "readWord(kRoomActorTableStart + i * sizeof(u32))", STATE_SOURCE
        )
        self.assertIn("captureStaticRanges();", STATE_SOURCE)
        self.assertIn("restoreStaticRanges();", STATE_SOURCE)
        self.assertIn("storeStaticRanges();", STATE_SOURCE)
        self.assertIn("header->magic = 0u", STATE_SOURCE)
        self.assertIn("void initializeSlot()", STATE_SOURCE)
        self.assertIn("initializeSlot();", STATE_SOURCE)
        self.assertLess(
            STATE_SOURCE.index("header->magic = 0u"),
            STATE_SOURCE.index("header->magic = kSnapshotMagic"),
        )
        self.assertNotIn("copyWords(reinterpret_cast<void *>(kMem1Start)", STATE_SOURCE)

    def test_dvd_workers_are_quiescent_and_transport_is_repaired(self) -> None:
        self.assertIn("kDvdFileInfoArray = 0x8038FB98u", STATE_SOURCE)
        self.assertIn("kDvdFileInfoCount = 64u", STATE_SOURCE)
        self.assertIn("kDvdFileInfoSize = 0x88u", STATE_SOURCE)
        self.assertIn("kDvdPrimaryThread = 0x80393DC0u", STATE_SOURCE)
        self.assertIn("kDvdPrimaryQueue = 0x803940D0u", STATE_SOURCE)
        self.assertIn("kDvdPrimaryCursor = 0x803941F0u", STATE_SOURCE)
        self.assertIn("kDvdSecondaryThread = 0x80398200u", STATE_SOURCE)
        self.assertIn(
            "kDvdSecondaryRequestQueue = 0x80398510u", STATE_SOURCE
        )
        self.assertIn(
            "kDvdSecondaryCompletionQueue = 0x80398534u", STATE_SOURCE
        )
        self.assertIn("bool primaryDvdWorkerIdle", STATE_SOURCE)
        self.assertIn("bool secondaryDvdWorkerIdle", STATE_SOURCE)
        self.assertIn("Gate::DvdPrimary", STATE_SOURCE)
        self.assertIn("Gate::DvdSecondary", STATE_SOURCE)
        self.assertIn('return "DVD1";', STATE_SOURCE)
        self.assertIn('return "DVD2";', STATE_SOURCE)
        self.assertIn("void canonicalizeDvdTransport()", STATE_SOURCE)
        self.assertIn("writeWord(info + 0x7Cu, next);", STATE_SOURCE)
        self.assertIn("writeByte(info + 0x80u, 0u);", STATE_SOURCE)
        self.assertIn("canonicalizeDvdTransport();", STATE_SOURCE)
        self.assertNotIn("OSInitMessageQueue", STATE_SOURCE)

    def test_animated_model_owner_and_update_guard_contract(self) -> None:
        self.assertIn(
            "kAnimatedModelOwnerStateStart = 0x803C26C8u", STATE_SOURCE
        )
        self.assertIn(
            "kAnimatedModelOwnerStateEnd = 0x803C2D94u", STATE_SOURCE
        )
        self.assertIn(
            "kAnimatedModelOwnerStateEnd -\n"
            "                      kAnimatedModelOwnerStateStart ==\n"
            "                  0x6CCu",
            STATE_SOURCE,
        )
        self.assertIn(
            "const u32 kLMAnimatedModelPoolUpdateAddr = 0x80026750u",
            DIAG_SOURCE,
        )
        self.assertIn(
            "const u32 kLMAnimatedModelControllerUpdateAddr = 0x8001EA84u",
            DIAG_SOURCE,
        )
        pool_guard = DIAG_SOURCE.split(
            'extern "C" void diagnosticAnimatedModelPoolUpdate()', 1
        )[1].split("// This is the final call", 1)[0]
        self.assertIn("controller != expectedController", pool_guard)
        self.assertIn("writeWord(slot + 0x3Cu, flags & ~3u);", pool_guard)
        self.assertIn(
            "reinterpret_cast<VoidFn>(kLMAnimatedModelPoolUpdateAddr)();",
            pool_guard,
        )
        controller_guard = DIAG_SOURCE.split(
            'extern "C" void diagnosticAnimatedModelControllerUpdate', 1
        )[1].split("// MAIN GAME", 1)[0]
        self.assertIn("animatedModelControllerSafe", controller_guard)
        self.assertIn("LMState::postLoadDetail(0xE5u", controller_guard)
        self.assertIn(
            "reinterpret_cast<RetailCall4Fn>(kLMAnimatedModelControllerUpdateAddr)",
            controller_guard,
        )

    def test_state_controls_and_resource_gates(self) -> None:
        self.assertIn("kDPadLeft = 0x0001u", STATE_SOURCE)
        self.assertIn("kDPadRight = 0x0002u", STATE_SOURCE)
        self.assertIn("kDvdBusyPredicateAddr = 0x80006A5Cu", STATE_SOURCE)
        self.assertIn("kAramList0Global + 8u", STATE_SOURCE)
        self.assertIn("kAramList1Global + 8u", STATE_SOURCE)
        self.assertIn("kCardBlockGlobal = 0x80495960u", STATE_SOURCE)
        self.assertIn("kCardControlStride = 0x108u", STATE_SOURCE)
        self.assertIn("kCardResultBusy = 0xFFFFFFFFu", STATE_SOURCE)
        self.assertIn("kCurrentHeapGroupGlobal = 0x80498AE8u", STATE_SOURCE)
        self.assertIn("headerMatchesLive", STATE_SOURCE)
        self.assertIn("buildIdentity(&live, true)", STATE_SOURCE)
        self.assertIn("ioIdle(true)", STATE_SOURCE)
        self.assertIn('return "CARD0";', STATE_SOURCE)
        self.assertIn('return "MODE";', STATE_SOURCE)
        self.assertIn('return "AUDIO";', STATE_SOURCE)
        self.assertIn('return "LOOP";', STATE_SOURCE)
        self.assertIn('return "EXIT";', STATE_SOURCE)
        self.assertIn('return "PEND";', STATE_SOURCE)
        self.assertIn('return "DRAW";', STATE_SOURCE)
        self.assertIn('return "GROOT";', STATE_SOURCE)
        self.assertIn("identity->mainLoopMode != 2u", STATE_SOURCE)
        self.assertIn(
            "identity->mainLoopPendingScene != identity->mainLoopScene",
            STATE_SOURCE,
        )
        self.assertIn(
            "header->mainDrawState == live.mainDrawState", STATE_SOURCE
        )
        self.assertIn("identity->mainDrawState > 7u", STATE_SOURCE)
        self.assertIn("header->mainDrawState <= 7u", STATE_SOURCE)
        self.assertIn("kGameStaticRootGlobals[i]", STATE_SOURCE)
        self.assertIn("header->simpleModeler == live.simpleModeler", STATE_SOURCE)
        self.assertIn("header->mapCol == live.mapCol", STATE_SOURCE)
        self.assertIn("header->enTypesManager == live.enTypesManager", STATE_SOURCE)

    def test_cross_room_epoch_diagnostic_contract(self) -> None:
        self.assertIn(
            "#define SUSAMUNE_LM_EPOCH_PHASE_FLAG      0x80000000u",
            CRASH_HEADER,
        )
        self.assertIn(
            "#define SUSAMUNE_LM_EPOCH_MASK            0x003FFFFFu",
            CRASH_HEADER,
        )
        fields = (
            "MAP_VALUE", "SCENE_VALUE", "CURRENT_SCENE", "PENDING_SCENE",
            "LOOP_MODE", "AUDIO_SCENE", "MAP_ARCHIVE", "VOLUME_COUNT",
            "VOLUME_HEAD", "VOLUME_TAIL", "MISSION_MODE", "GAME_MODE",
            "SIMPLE_MODELER", "MAP_COL", "EN_TYPES", "GAME_HEAP",
            "GAME_HEAP_START", "GAME_HEAP_END", "ROOT_HEAP", "SYSTEM_HEAP",
            "AUDIO_BASIC", "DRAW_STATE",
        )
        for field in fields:
            self.assertIn(f"SUSAMUNE_LM_EPOCH_{field}", CRASH_HEADER)
        definitions = re.findall(
            r"#define\s+SUSAMUNE_LM_EPOCH_([A-Z_]+)\s+\(1u << (\d+)\)",
            CRASH_HEADER,
        )
        self.assertEqual(definitions, list(zip(fields, map(str, range(22)))))
        self.assertIn("struct EpochMismatch", STATE_SOURCE)
        self.assertIn("collectPreflightEpochMismatch", STATE_SOURCE)
        self.assertIn("mismatch->mask |= field;", STATE_SOURCE)
        self.assertIn("sEpochMismatch = mismatch;", STATE_SOURCE)
        self.assertIn(
            "SUSAMUNE_LM_EPOCH_PHASE_FLAG | mismatch.mask", STATE_SOURCE
        )
        self.assertEqual(STATE_SOURCE.count("clearEpochMismatch();"), 2)
        self.assertIn('return "VOLN";', STATE_SOURCE)
        self.assertIn('return "SCNP";', STATE_SOURCE)
        self.assertIn("u32 epochMask()", STATE_SOURCE)
        self.assertIn("u32 epochSaved()", STATE_SOURCE)
        self.assertIn("u32 epochLive()", STATE_SOURCE)
        self.assertIn("Susamune: epoch mask=%08X first=%s", KERNEL_CRASH_SOURCE)
        self.assertIn("LMEpochFieldName(mask)", KERNEL_CRASH_SOURCE)
        self.assertIn(
            '"LM STATE X0.3.26 F:%s C:%s H:%s X%02lX"', DIAG_SOURCE
        )
        self.assertIn("LMState::crossRoomGuardCode()", DIAG_SOURCE)
        self.assertIn(
            '"E:%s M%08lX %08lX>%08lX"', DIAG_SOURCE
        )

    def test_volume_census_is_bounded_and_generation_keyed(self) -> None:
        self.assertIn("kVolumeListGlobal = 0x80494754u", STATE_SOURCE)
        self.assertIn("kCurrentVolumeGlobal = 0x804A2038u", STATE_SOURCE)
        self.assertIn("kCurrentDirIdGlobal = 0x804A2040u", STATE_SOURCE)
        self.assertIn("kMemArchiveVtable = 0x80388D5Cu", STATE_SOURCE)
        self.assertIn("kMaxVolumes = 32u", STATE_SOURCE)
        self.assertIn("sizeof(VolumeDescriptor) == 0x60u", STATE_SOURCE)
        self.assertIn("bool captureVolumeCensus", STATE_SOURCE)
        self.assertIn("readWord(node + 4u) != kVolumeListGlobal", STATE_SOURCE)
        self.assertIn("entry.object + 0x18u != node", STATE_SOURCE)
        self.assertIn("entry.previous != previous", STATE_SOURCE)
        self.assertIn("previous != census->tail", STATE_SOURCE)
        self.assertIn("kVolumeFaultChanged", STATE_SOURCE)
        self.assertIn("captureVolumeName(&entry);", STATE_SOURCE)
        self.assertIn("entry.contentSignature = content;", STATE_SOURCE)
        self.assertIn("sSavedVolumeCensus.generation != header->generation", STATE_SOURCE)
        self.assertIn("commitSavedVolumeCensus(header->generation);", STATE_SOURCE)
        self.assertIn("diffVolumeCensus(sSavedVolumeCensus", STATE_SOURCE)
        self.assertIn('return "HEAD1";', STATE_SOURCE)
        self.assertIn('return "HEAD2";', STATE_SOURCE)
        self.assertIn('"V:%s S%lu>L%lu -%lu +%lu F%lu/%lu"', DIAG_SOURCE)
        self.assertIn("kVolumeRemovedSlots = 8u", STATE_SOURCE)
        self.assertIn("kVolumeAddedSlots = 8u", STATE_SOURCE)
        self.assertIn("kVolumeDisplayedPerKind = 3u", STATE_SOURCE)
        self.assertIn("2u * kVolumeDisplayedPerKind", STATE_SOURCE)
        self.assertIn("kModelChangeSlots = 16u", STATE_SOURCE)
        self.assertIn(
            "kModelChangeSlots >=\n"
            "                      kVolumeRemovedSlots + kVolumeAddedSlots",
            STATE_SOURCE,
        )
        self.assertIn('"V%s%s %s/%s O%08lX R%08lX %luB"', DIAG_SOURCE)
        self.assertIn('"VC %08lX>%08lX D%08lX>%08lX"', DIAG_SOURCE)
        self.assertIn("guardedCrossRoomRestoreAllowed", STATE_SOURCE)
        self.assertIn("repairSavedVolumeList", STATE_SOURCE)
        self.assertIn("kSnapshotVersion = 15u", STATE_SOURCE)
        display = STATE_SOURCE.split(
            "const VolumeDescriptor *volumeChangeEntry", 1
        )[1].split("u32 volumeChangeObject", 1)[0]
        self.assertIn("displayIndex < kVolumeDisplayedPerKind", display)
        self.assertIn(
            "displayIndex - kVolumeDisplayedPerKind", display
        )

    def test_resource_manager_epoch_census_is_bounded(self) -> None:
        self.assertIn("kResourceMapBase = 0x80398C50u", STATE_SOURCE)
        self.assertIn("kResourceMapSize = 0x200u", STATE_SOURCE)
        self.assertIn("kResourceActiveBase = 0x80398E90u", STATE_SOURCE)
        self.assertIn("kResourceBackingBase = 0x80398ECCu", STATE_SOURCE)
        self.assertIn("kResourceMarkBase = 0x80398F08u", STATE_SOURCE)
        self.assertIn("kResourceWantedBase = 0x80398F68u", STATE_SOURCE)
        self.assertIn("kResourceSlotCount = 7u", STATE_SOURCE)
        self.assertIn("kResourceWantedCapacity = 24u", STATE_SOURCE)
        self.assertIn("kResourceRecordSize = 0x40u", STATE_SOURCE)
        self.assertIn("kResourceSlotSize = 0x70800u", STATE_SOURCE)
        self.assertIn("bool captureResourceCensus", STATE_SOURCE)
        self.assertIn("diffResourceCensus(sSavedResourceCensus", STATE_SOURCE)
        self.assertIn("commitSavedResourceCensus(header->generation);", STATE_SOURCE)
        self.assertIn("saved.wantedIds[i] != live.wantedIds[i]", STATE_SOURCE)
        self.assertIn('"RM F%lu/%lu A%02lX R%02lX', DIAG_SOURCE)
        self.assertIn('"RA %lu:%08lX>%08lX', DIAG_SOURCE)
        self.assertIn('"RW %lu>%lu -%lu +%lu Q%lu', DIAG_SOURCE)
        self.assertIn("{kResourceMapBase, kResourceStateEnd - kResourceMapBase}",
                      STATE_SOURCE)
        self.assertIn("kResourceStateEnd = 0x80398FC8u", STATE_SOURCE)

    def test_model_manager_epoch_census_covers_both_fixed_tables(self) -> None:
        self.assertIn("kModelTableBase = 0x803435ACu", STATE_SOURCE)
        self.assertIn("kModelEntryCount = 262u", STATE_SOURCE)
        self.assertIn("kModelEntrySize = 0x34u", STATE_SOURCE)
        self.assertIn("kModelRegistryBase = 0x8037EC70u", STATE_SOURCE)
        self.assertIn("kModelRegistryEntrySize = 0x40u", STATE_SOURCE)
        self.assertIn("kModelTableSize == 0x3538u", STATE_SOURCE)
        self.assertIn("kModelRegistrySize == 0x4180u", STATE_SOURCE)
        self.assertIn("kModelCensusRecordSize == 0x76CCu", STATE_SOURCE)
        self.assertIn("kModelCensusScratchSize == 0x76E0u", STATE_SOURCE)
        self.assertIn(
            "kModelCensusScratchSize < kSnapshotStorageSize", STATE_SOURCE
        )
        self.assertIn("(kModelCensusScratchSize & 31u) == 0u", STATE_SOURCE)
        self.assertIn("kHeapDataOffset < kSnapshotCapacity", STATE_SOURCE)
        self.assertIn("kModelTableSnapshotOffset == 0xB50u", STATE_SOURCE)
        self.assertIn("kModelRegistrySnapshotOffset == 0x4088u", STATE_SOURCE)
        self.assertIn(
            "kSavedModelCensusMetadataAddress +\n"
            "                          kModelCensusMetadataSize ==\n"
            "                      SUSAMUNE_MEM2_CFG_PPC_BASE",
            STATE_SOURCE,
        )
        self.assertNotIn("ModelCensus sSavedModelCensus", STATE_SOURCE)
        self.assertNotIn("ModelCensus sLiveModelCensus", STATE_SOURCE)
        self.assertIn("const ModelCensusView sSavedModelCensus", STATE_SOURCE)
        self.assertIn("const ModelCensusView sLiveModelCensus", STATE_SOURCE)
        self.assertIn(
            "clearWords(sSavedModelCensus.metadata, kModelCensusMetadataSize)",
            STATE_SOURCE,
        )
        self.assertIn(
            "clearWords(sLiveModelCensus.metadata, kModelCensusRecordSize)",
            STATE_SOURCE,
        )
        self.assertEqual(0x803435AC + 262 * 0x34, 0x80346AE4)
        self.assertEqual(0x8037EC70 + 262 * 0x40, 0x80382DF0)
        self.assertIn("bool captureModelCensus", STATE_SOURCE)
        self.assertIn("sameModelEntry(saved, live, i)", STATE_SOURCE)
        self.assertIn("sameModelRegistryEntry(saved, live, i)", STATE_SOURCE)
        self.assertIn("captureModelName(&change", STATE_SOURCE)
        self.assertIn("modelEntryWord(saved, i, 0x08u)", STATE_SOURCE)
        self.assertIn("diffModelCensus(sSavedModelCensus", STATE_SOURCE)
        self.assertIn("commitSavedModelCensus(header->generation);", STATE_SOURCE)
        self.assertIn(
            "tableCopy != sLiveModelCensus.metadata->signature", STATE_SOURCE
        )
        self.assertIn(
            "registryCopy != sLiveModelCensus.metadata->registrySignature",
            STATE_SOURCE,
        )
        self.assertIn('return "P";', STATE_SOURCE)
        self.assertIn('return "R";', STATE_SOURCE)
        self.assertIn('return "B";', STATE_SOURCE)
        self.assertIn(
            '"MM F%lu/%lu N%lu P%08lX>%08lX R%08lX>%08lX"',
            DIAG_SOURCE,
        )
        self.assertIn(
            '"M%03lu%s %s S%lX>%lX H%07lX>%07lX R%07lX>%07lX"',
            DIAG_SOURCE,
        )
        self.assertIn('"M%03luR %s %08lX>%08lX %08lX>%08lX"', DIAG_SOURCE)
        self.assertIn("const u16 panelHeight = showModel ? 174u : 138u", DIAG_SOURCE)
        self.assertIn("const u16 rootTop = showModel ? 142u : 107u", DIAG_SOURCE)
        self.assertIn("directPrint, 0, kPanelTop, 320, panelHeight", DIAG_SOURCE)
        self.assertIn("directPrint, 2, kPanelTop + rootTop", DIAG_SOURCE)
        primary_row = (
            f"M{261:03d}B {'x' * 8} SF>F H{0x1FFFFFF:07X}>{0x1FFFFFF:07X} "
            f"R{0x1FFFFFF:07X}>{0x1FFFFFF:07X}"
        )
        registry_row = (
            f"M{261:03d}R {'x' * 8} {0xFFFFFFFF:08X}>{0xFFFFFFFF:08X} "
            f"{0xFFFFFFFF:08X}>{0xFFFFFFFF:08X}"
        )
        self.assertLessEqual(len(primary_row), 53)
        self.assertLessEqual(len(registry_row), 53)
        self.assertIn("{kModelTableBase, kModelTableSize}", STATE_SOURCE)
        self.assertIn("{kModelRegistryBase, kModelRegistrySize}", STATE_SOURCE)
        self.assertIn("kModelOutputStateStart = 0x803C86A0u", STATE_SOURCE)
        self.assertIn("kModelOutputStateEnd = 0x803C97C4u", STATE_SOURCE)
        self.assertIn("kModelRegistryOutputStateStart = 0x803E3088u",
                      STATE_SOURCE)
        self.assertIn("kModelRegistryOutputStateEnd = 0x803E3CF8u",
                      STATE_SOURCE)
        self.assertIn("kSnapshotVersion = 15u", STATE_SOURCE)

    def test_camera_state_tracks_persistent_views_safely(self) -> None:
        self.assertIn("kCameraObjectPointerTable = 0x80399BE0u", STATE_SOURCE)
        self.assertIn("kCameraObjectCount = 3u", STATE_SOURCE)
        self.assertIn("kCameraObjectSize = 0xECu", STATE_SOURCE)
        self.assertIn("bool cameraObjectsValid", STATE_SOURCE)
        self.assertIn("readWord(record) != target", STATE_SOURCE)
        self.assertIn("captureCameraObjects(live);", STATE_SOURCE)
        self.assertIn("restoreCameraObjects();", STATE_SOURCE)
        self.assertIn("storeCameraObjects();", STATE_SOURCE)
        capture = STATE_SOURCE.split("void captureCameraObjects", 1)[1].split(
            "void restoreCameraObjects", 1
        )[0]
        self.assertIn("inGameHeap ? 0u : kCameraObjectSize", capture)
        self.assertIn("if (!inGameHeap)", capture)

    def test_guarded_cross_room_raw_rewind_contract(self) -> None:
        guard_codes = (
            ("None", "0u"),
            ("Mask", "1u"),
            ("Generation", "2u"),
            ("Volume", "3u"),
            ("Topology", "4u"),
            ("Archive", "5u"),
            ("Resource", "6u"),
            ("Model", "7u"),
            ("ModelShape", "8u"),
            ("Accepted", "0xA0u"),
        )
        for name, value in guard_codes:
            self.assertIn(f"kCrossRoomGuard{name} = {value}", STATE_SOURCE)
        self.assertIn("u32 crossRoomGuardCode()", STATE_SOURCE)

        guard = STATE_SOURCE.split(
            "bool guardedCrossRoomRestoreAllowed", 1
        )[1].split("void repairSavedVolumeList", 1)[0]
        self.assertRegex(
            guard,
            r"SUSAMUNE_LM_EPOCH_VOLUME_COUNT\s*\|\s*"
            r"SUSAMUNE_LM_EPOCH_VOLUME_HEAD",
        )
        self.assertIn("if (mismatch.mask != allowedMask) return false;", guard)
        self.assertEqual((1 << 7) | (1 << 8), 0x180)
        self.assertIn("orderedVolumeReplacementMatches()", guard)
        self.assertIn("sVolumeDiff.removedIndices[i]", guard)
        self.assertIn("sVolumeDiff.addedIndices[i]", guard)
        self.assertIn("changedArchiveIsRewindable", guard)
        self.assertIn("sResourceDiff.mapChanged != 0u", guard)
        self.assertNotIn("sSavedResourceCensus.markMask != 0u", guard)
        self.assertNotIn("sLiveResourceCensus.markMask != 0u", guard)
        self.assertIn("modelReplacementMatches()", guard)

        topology = STATE_SOURCE.split(
            "bool orderedVolumeReplacementMatches", 1
        )[1].split("bool changedArchiveIsRewindable", 1)[0]
        self.assertNotIn("removedIndices[i] != i", topology)
        self.assertNotIn("addedIndices[i] != i", topology)
        self.assertIn("sSavedVolumeCensus.entries[savedIndex]", topology)
        self.assertIn("sLiveVolumeCensus.entries[liveIndex]", topology)

        model = STATE_SOURCE.split(
            "bool modelReplacementMatches", 1
        )[1].split("bool guardedCrossRoomRestoreAllowed", 1)[0]
        self.assertIn("sVolumeDiff.removedIndices, removed", model)
        self.assertIn("sVolumeDiff.addedIndices, added", model)

        self.assertIn("kResourceStateEnd = 0x80398FC8u", STATE_SOURCE)
        self.assertIn("kResourceStateEnd - kResourceMapBase == 0x378u",
                      STATE_SOURCE)
        self.assertIn("{kVolumeListGlobal, 3u * sizeof(u32)}", STATE_SOURCE)
        self.assertIn("{kCurrentVolumeGlobal, sizeof(u32)}", STATE_SOURCE)
        self.assertIn("{kCurrentDirIdGlobal, sizeof(u32)}", STATE_SOURCE)

        repair = STATE_SOURCE.split(
            "void repairSavedVolumeList", 1
        )[1].split("u32 crcByte", 1)[0]
        self.assertIn("writeWord(entry.node, entry.object);", repair)
        self.assertIn("writeWord(entry.node + 4u, kVolumeListGlobal);", repair)
        self.assertIn("writeWord(entry.node + 8u, entry.previous);", repair)
        self.assertIn("writeWord(entry.node + 0xCu, entry.next);", repair)
        self.assertIn("reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)",
                      repair)

        restore = STATE_SOURCE.split("void loadState()", 1)[1]
        self.assertIn("repairSavedVolumeList(header);", restore)
        self.assertLess(restore.index("restoreStaticRanges();"),
                        restore.index("repairSavedVolumeList(header);"))
        self.assertLess(restore.index("repairSavedVolumeList(header);"),
                        restore.index("\n    storeStaticRanges();"))
        self.assertIn("kGXInvalidateVtxCacheAddr = 0x801EF208u", STATE_SOURCE)
        self.assertIn("reinterpret_cast<VoidFn>(kGXInvalidateVtxCacheAddr)();",
                      restore)
        self.assertIn("reinterpret_cast<VoidFn>(kGXInvalidateTexAllAddr)();",
                      restore)
        self.assertLess(restore.index("kGXInvalidateVtxCacheAddr"),
                        restore.index("kGXInvalidateTexAllAddr"))

        self.assertNotRegex(
            STATE_SOURCE,
            r"\b(?:unload|unmount|mountFixed|removeResource|detachResource|"
            r"reconcile)\w*\s*\(",
        )

    def test_state_quiesces_audio_and_scheduler(self) -> None:
        self.assertIn("kAudioBasicGlobal = 0x804A1DD0u", STATE_SOURCE)
        self.assertIn("kAudioStaticObject = 0x803E3CF8u", STATE_SOURCE)
        self.assertIn("kAudioVtable = 0x80383FB0u", STATE_SOURCE)
        self.assertIn("kAudioBootstrapSoundId = 0x80000800u", STATE_SOURCE)
        self.assertIn("kAudioChangeSoundSceneAddr = 0x8018D4E4u", STATE_SOURCE)
        self.assertIn("!quiesceAudio(preflight)", STATE_SOURCE)
        self.assertEqual(STATE_SOURCE.count("!quiesceAudio(preflight)"), 2)
        self.assertIn("validAudioBootstrap(identity.audioBasic)", STATE_SOURCE)
        self.assertNotIn("AudioStopSoundHandleFn", STATE_SOURCE)
        self.assertNotIn("kAudioStopSoundHandleAddr", STATE_SOURCE)
        self.assertIn("kOSDisableSchedulerAddr = 0x801DAE98u", STATE_SOURCE)
        self.assertIn("kOSEnableSchedulerAddr = 0x801DAED8u", STATE_SOURCE)

    def test_state_transactions_leave_phase_breadcrumbs(self) -> None:
        self.assertIn("kEventStateSavePhase = 0x110u", STATE_SOURCE)
        self.assertIn("kEventStateLoadPhase = 0x111u", STATE_SOURCE)
        self.assertGreaterEqual(
            STATE_SOURCE.count("traceSavePhase("), 8
        )
        self.assertGreaterEqual(
            STATE_SOURCE.count("traceLoadPhase("), 16
        )
        self.assertIn("LMCrash::note(kEventStateSavePhase", STATE_SOURCE)
        self.assertIn("LMCrash::note(kEventStateLoadPhase", STATE_SOURCE)
        self.assertIn("272=save-phase", KERNEL_CRASH_SOURCE)
        self.assertIn("273=load-phase", KERNEL_CRASH_SOURCE)

    def test_hard_hang_phase_journal_contract(self) -> None:
        self.assertIn("SUSAMUNE_PHASE_TRACE_OFFSET", CRASH_HEADER)
        self.assertIn("sizeof(struct SusamunePhaseTrace) ==", CRASH_HEADER)
        self.assertIn("SUSAMUNE_PHASE_TRACE_PPC_PTR", CRASH_SOURCE)
        self.assertIn("sequence - 1u", CRASH_SOURCE)
        self.assertIn("SUSAMUNE_PHASE_TRACE_PHYS_PTR", KERNEL_CRASH_SOURCE)
        self.assertIn("PollPhaseTrace();", KERNEL_CRASH_SOURCE)
        self.assertIn("f_sync(&dbgfile)", (ROOT / "launcher" / "kernel" /
                                           "vsprintf.c").read_text(encoding="utf-8"))
        self.assertIn("presenterEnter();", DIAG_SOURCE)
        self.assertIn("presenterAfterTick();", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x88u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x8Au);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x8Cu);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x8Eu);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x90u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x92u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x94u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x96u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x98u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0x9Au);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0xA0u);", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0xA2u);", DIAG_SOURCE)
        self.assertIn("postLoadDetail(0xA4u", DIAG_SOURCE)
        self.assertIn("postLoadMilestone(0xA6u);", DIAG_SOURCE)
        self.assertIn("postLoadDetail(0xE2u", DIAG_SOURCE)
        self.assertIn("postLoadDetail(0xE3u", DIAG_SOURCE)
        self.assertIn("postLoadDetail(0xF0u", DIAG_SOURCE)
        self.assertIn("postLoadDetail(0xF1u", DIAG_SOURCE)
        self.assertNotIn("diagnosticAudioTailB628", DIAG_SOURCE)

    def test_verbose_trace_wrappers_are_pruned(self) -> None:
        removed_prefixes = (
            "diagnosticMainUpdate",
            "diagnosticEffect",
            "diagnosticActorUpdate",
            "diagnosticMainDraw",
            "diagnosticNormalDraw",
            "diagnosticPerViewDraw",
        )
        self.assertEqual(lm_diag.mod_write_count, 21)
        self.assertFalse(
            any(
                entry["sym"].startswith(removed_prefixes)
                for entry in lm_diag.patches
            )
        )
        self.assertNotIn("DEFINE_UPDATE_CALL", DIAG_SOURCE)
        self.assertNotIn("DEFINE_ACTOR_UPDATE_CALL", DIAG_SOURCE)
        self.assertNotIn("DEFINE_DRAW_CALL", DIAG_SOURCE)
        self.assertNotIn("roomActorIndex(", DIAG_SOURCE)
        self.assertIn("animatedModelControllerSafe(", DIAG_SOURCE)
        self.assertIn("diagnosticAnimatedModelPoolUpdate", DIAG_SOURCE)
        self.assertIn("diagnosticAnimatedModelControllerUpdate", DIAG_SOURCE)

    def test_post_load_trace_spans_multiple_restored_frames(self) -> None:
        self.assertIn("kPostLoadTraceFrameLimit = 8u", STATE_SOURCE)
        self.assertIn(
            "kPostLoadTraceLingeringFrameLimit = 7200u", STATE_SOURCE
        )
        self.assertIn("kPostLoadTraceHeartbeatFrames = 300u", STATE_SOURCE)
        self.assertIn("kPostLoadDoorWindowFrames = 240u", STATE_SOURCE)
        self.assertIn(
            "kPostLoadTransitionBurstUpdates = 2u", STATE_SOURCE
        )
        self.assertIn("u32 sPostLoadTraceFrame", STATE_SOURCE)
        self.assertIn("bool sPostLoadTracePresentationBurst", STATE_SOURCE)
        self.assertIn("kPadMovementDeadzone = 24u", STATE_SOURCE)
        self.assertIn("kPostLoadInputBurstUpdates = 2u", STATE_SOURCE)
        self.assertIn("bool padMovementActive()", STATE_SOURCE)
        load_success = STATE_SOURCE.split(
            "sStatus = LMState::Status::Loaded", 1
        )[1].split("traceLoadPhase(0x7Fu", 1)[0]
        self.assertLess(load_success.index("sPostLoadTraceFrame = 0u"),
                        load_success.index("sPostLoadTraceState = 1u"))
        presenter_enter = STATE_SOURCE.split("void presenterEnter()", 1)[1]
        self.assertLess(presenter_enter.index("sPostLoadTraceFrame >="),
                        presenter_enter.index("++sPostLoadTraceFrame"))
        after_tick = STATE_SOURCE.split("void presenterAfterTick()", 1)[1]
        self.assertLess(after_tick.index("tracePostLoadPhase(0x86u"),
                        after_tick.index("sPostLoadTraceState = 1u"))

    def test_post_load_trace_has_low_overhead_door_tail(self) -> None:
        watch = STATE_SOURCE.split(
            "void samplePostLoadTransitionWatch", 1
        )[1].split("bool refreshPostLoadTransitionWatch", 1)[0]
        for signal in (
            "kMapValueGlobal",
            "kSceneValueGlobal",
            "kMainLoopPendingSceneGlobal",
            "kMainLoopExitGlobal",
            "kDvdOutstandingGlobal",
            "kAramList0Global + 8u",
            "kAramList1Global + 8u",
            "kResourceWantedCountGlobal",
            "kVolumeListGlobal + 4u",
            "kVolumeListGlobal + 8u",
        ):
            self.assertIn(signal, watch)
        milestone = STATE_SOURCE.split(
            "void postLoadMilestone(u32 phase)", 1
        )[1].split("void postLoadDetail", 1)[0]
        self.assertIn("sPostLoadTraceState != 3u", milestone)
        self.assertIn("phase >= 0x96u && phase <= 0x9Bu", milestone)
        self.assertIn("phase == 0x8Du", milestone)
        self.assertIn(
            "sPostLoadDoorWindow = kPostLoadDoorWindowFrames", milestone
        )
        self.assertIn(
            "sPostLoadTraceBurstUpdates = kPostLoadInputBurstUpdates",
            milestone,
        )
        self.assertIn("refreshPostLoadTransitionWatch();", milestone)
        self.assertIn(
            "sPostLoadTraceBurstUpdates =\n"
            "                kPostLoadTransitionBurstUpdates;",
            milestone,
        )
        self.assertIn("phase == 0x8Eu", milestone)
        self.assertIn("--sPostLoadTraceBurstUpdates", milestone)
        self.assertIn("sPostLoadTracePresentationBurst", milestone)
        self.assertIn("tracePostLoadTransitionChange();", milestone)
        self.assertIn("const bool movement = padMovementActive();", milestone)
        self.assertIn("movement || changedButtons != 0u", milestone)
        self.assertIn("SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xE6u", milestone)
        detail = STATE_SOURCE.split(
            "void postLoadDetail(u32 phase", 1
        )[1].split("void presenterEnter", 1)[0]
        self.assertIn("postLoadDetailEnabled()", detail)
        self.assertIn("sPostLoadTraceState == 3u", detail)
        self.assertIn("sPostLoadTracePresentationBurst", detail)
        presenter = STATE_SOURCE.split("void presenterEnter()", 1)[1]
        self.assertIn("sPostLoadTraceState = 3u", presenter)
        self.assertIn("tracePostLoadPhase(0x87u", presenter)
        self.assertIn("--sPostLoadDoorWindow", presenter)

        save_success = STATE_SOURCE.split(
            "sStatus = LMState::Status::Saved", 1
        )[1].split("traceSavePhase(0x7Fu", 1)[0]
        self.assertIn("sPostLoadTraceState == 3u", save_success)
        self.assertIn(
            "sPostLoadTraceFrame = kPostLoadTraceFrameLimit", save_success
        )
        self.assertIn(
            "samplePostLoadTransitionWatch(&sPostLoadTransitionWatch)",
            save_success,
        )

    def test_state_transaction_runs_after_complete_retail_presenter(self) -> None:
        wrapper = DIAG_SOURCE.split(
            'extern "C" void diagnosticChangeFrameBuffer', 1
        )[1].split('extern "C" void diagnosticFrameBegin', 1)[0]
        self.assertLess(wrapper.index("kLMChangeFrameBufferAddr"),
                        wrapper.index("LMState::tick"))
        copy_wrapper = DIAG_SOURCE.split(
            'extern "C" void diagnosticCopyDisp', 1
        )[1]
        self.assertNotIn("LMState::tick", copy_wrapper)

    def test_frozen_transactions_do_not_take_heap_mutexes(self) -> None:
        save_frozen = STATE_SOURCE.split(
            "traceSavePhase(0x60u", 1
        )[1].split("freezeEnd(freeze);", 1)[0]
        load_frozen = STATE_SOURCE.split(
            "traceLoadPhase(0x60u", 1
        )[1].split("freezeEnd(freeze, true);", 1)[0]
        self.assertNotIn("heapsHealthy", save_frozen)
        self.assertNotIn("heapsHealthy", load_frozen)
        self.assertNotIn("ioIdle", save_frozen)
        self.assertNotIn("ioIdle", load_frozen)

    def test_overlay_is_painted_before_state_transaction(self) -> None:
        copy_wrapper = DIAG_SOURCE.split(
            'extern "C" void diagnosticCopyDisp', 1
        )[1]
        self.assertIn("drawRawHeartbeat", copy_wrapper)
        presenter_wrapper = DIAG_SOURCE.split(
            'extern "C" void diagnosticChangeFrameBuffer', 1
        )[1].split('extern "C" void diagnosticFrameBegin', 1)[0]
        self.assertLess(presenter_wrapper.index("kLMChangeFrameBufferAddr"),
                        presenter_wrapper.index("LMState::tick"))

    def test_state_gates_uncaptured_allocator_epochs(self) -> None:
        self.assertIn("header->rootFreeHead == live.rootFreeHead", STATE_SOURCE)
        self.assertIn("header->systemFreeHead == live.systemFreeHead", STATE_SOURCE)
        self.assertIn("header->currentHeap == live.currentHeap", STATE_SOURCE)
        self.assertIn(
            "header->currentHeapGroup == live.currentHeapGroup", STATE_SOURCE
        )
        self.assertNotIn("writeByte(live.systemHeap", STATE_SOURCE)
        self.assertNotIn("writeWord(kCurrentHeapGlobal", STATE_SOURCE)

    def test_lm_crash_callback_contract(self) -> None:
        self.assertIn("kSetPreUserCallbackAddr = 0x801D4124u", CRASH_SOURCE)
        self.assertIn("kPreUserCallbackAddr = 0x804A2074u", CRASH_SOURCE)
        self.assertIn("SUSAMUNE_CRASH_PPC_PTR", CRASH_SOURCE)
        self.assertIn("sPreviousHandler(exception, context, dsisr, dar)", CRASH_SOURCE)


if __name__ == "__main__":
    unittest.main()
