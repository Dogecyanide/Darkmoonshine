#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_state.hxx"
#include "Dolphin/PAD.h"

#include "lm_crash.hxx"
#include "lm_rumble.hxx"
#include "lm_notice.hxx"
#include "lm_practice.hxx"
#include "susamune/crash_report.h"
#include "susamune/mem2_map.h"
#include "susamune/mod_bin.h"
#include "susamune/lm_state_storage.h"
#include "susamune/lm_state_deflate.h"
#include "susamune/lm_crc32.h"
#include "susamune/lm_state_auth.h"
#include "susamune/lm_state_hotkeys.h"
#include "susamune/lm_shared_archive.h"
#include "susamune/lm_archive_guard.h"
#include "susamune/lm_state_resource.h"
#include "susamune/lm_state_model.h"
#include "susamune/lm_grain_state.h"
#include "susamune/lm_state_roots.h"
#include "susamune/lm_map_archive.h"
#include "susamune/lm_exp_heap.h"
#include "susamune/lm_render_targets.h"
#include "susamune/lm_camera_state.h"
#include "susamune/lm_persistent_profile.h"
#include "susamune/lm_audio_idle.h"
#include "susamune/lm_door_state.h"

// The Kuribo build is deliberately freestanding. Clang may still lower a
// small aggregate operations to memcpy/memcmp at -Oz, so keep those runtime
// helpers inside the injected image rather than depending on a retail libc.
extern "C" void *memcpy(void *destination, const void *source, u32 size) {
    volatile u8 *out = static_cast<volatile u8 *>(destination);
    const volatile u8 *in = static_cast<const volatile u8 *>(source);
    for (u32 i = 0; i < size; ++i) {
        out[i] = in[i];
    }
    return destination;
}

extern "C" int memcmp(const void *left, const void *right, u32 size) {
    const volatile u8 *a = static_cast<const volatile u8 *>(left);
    const volatile u8 *b = static_cast<const volatile u8 *>(right);
    for (u32 i = 0; i < size; ++i) {
        if (a[i] != b[i]) {
            return static_cast<int>(a[i]) - static_cast<int>(b[i]);
        }
    }
    return 0;
}

extern "C" void *memset(void *destination, int value, u32 size) {
    volatile u8 *out = static_cast<volatile u8 *>(destination);
    for (u32 i = 0; i < size; ++i) out[i] = static_cast<u8>(value);
    return destination;
}

namespace {

// Clean GLMJ01 retail anchors recovered from the Japanese DOL.
constexpr u32 kRootHeapGlobal = 0x804A0B90u;
constexpr u32 kSystemHeapGlobal = 0x804A0B94u;
constexpr u32 kGameHeapGlobal = 0x804A0B98u;
constexpr u32 kCurrentHeapGlobal = 0x804A1FF4u;
constexpr u32 kCurrentHeapGroupGlobal = 0x80498AE8u;
constexpr u32 kRandomStateGlobal = 0x804A0B30u;
constexpr u32 kSceneValueGlobal = 0x804A0C20u;
constexpr u32 kMapValueGlobal = 0x804A0C48u;
constexpr u32 kCurrentSceneGlobal = 0x80498B18u;
constexpr u32 kMainLoopStateBase = 0x80398A40u;
constexpr u32 kMainLoopModeGlobal = kMainLoopStateBase;
constexpr u32 kMainLoopPendingSceneGlobal = kMainLoopStateBase + 4u;
constexpr u32 kMainLoopSceneGlobal = 0x804A0C20u;
constexpr u32 kMainDrawStateGlobal = 0x804A0C44u;
constexpr u32 kMainLoopExitGlobal = 0x804A0C28u;
constexpr u32 kGameModeGlobal = 0x804A17B0u;
constexpr u32 kGameModeCountGlobal = 0x804A17B4u;
constexpr u32 kMatrixArrayGlobal = 0x804A17B8u;
constexpr u32 kBooleanArrayGlobal = 0x804A17BCu;
constexpr u32 kMissionModeGlobal = 0x804A17C8u;
constexpr u32 kPlayerVtable = 0x8034EE50u;
constexpr u32 kSimpleModelerGlobal = 0x804A17D0u;
constexpr u32 kMapColGlobal = 0x804A17D8u;
constexpr u32 kEnTypesManagerGlobal = 0x804A17E8u;
constexpr u32 kGameStaticRootGlobals[] = {
    kMatrixArrayGlobal,
    kBooleanArrayGlobal,
    kMissionModeGlobal,
    kSimpleModelerGlobal,
    kMapColGlobal,
    kEnTypesManagerGlobal,
};
constexpr u32 kVolumeListGlobal = 0x80494754u;
constexpr u32 kCurrentVolumeGlobal = 0x804A2038u;
constexpr u32 kCurrentDirIdGlobal = 0x804A2040u;
constexpr u32 kResourceRecordBaseGlobal = 0x804A0D08u;
constexpr u32 kResourceBulkBaseGlobal = 0x804A0D0Cu;
constexpr u32 kResourceSlotCountGlobal = 0x804A0D10u;
constexpr u32 kResourceSlotSizeGlobal = 0x804A0D14u;
constexpr u32 kResourceWantedCountGlobal = 0x804A0D18u;
constexpr u32 kPadStatusGlobal = 0x80494778u;
// LM's two DVD workers live outside the gameplay heap.  The public outstanding
// counter reaches zero before the primary worker finishes its callbacks, so a
// save/load gate must prove both workers are actually asleep on empty queues.
constexpr u32 kDvdFileInfoArray = 0x8038FB98u;
constexpr u32 kDvdFileInfoCount = 64u;
constexpr u32 kDvdFileInfoSize = 0x88u;
constexpr u32 kDvdCurrentGlobal = 0x80391D98u;
constexpr u32 kDvdOutstandingGlobal = kDvdCurrentGlobal;
constexpr u32 kDvdPrimaryThread = 0x80393DC0u;
constexpr u32 kDvdPrimaryThreadState = 0x80394088u;
constexpr u32 kDvdPrimaryThreadSuspend = 0x8039408Cu;
constexpr u32 kDvdPrimaryThreadQueue = 0x8039409Cu;
constexpr u32 kDvdPrimaryQueue = 0x803940D0u;
constexpr u32 kDvdPrimaryMessages = 0x803940F0u;
constexpr u32 kDvdPrimaryCursor = 0x803941F0u;
constexpr u32 kDvdSecondaryThread = 0x80398200u;
constexpr u32 kDvdSecondaryThreadState = 0x803984C8u;
constexpr u32 kDvdSecondaryThreadSuspend = 0x803984CCu;
constexpr u32 kDvdSecondaryThreadQueue = 0x803984DCu;
constexpr u32 kDvdSecondaryRequestQueue = 0x80398510u;
constexpr u32 kDvdSecondaryRequestMessage = 0x80398530u;
constexpr u32 kDvdSecondaryCompletionQueue = 0x80398534u;
constexpr u32 kDvdSecondaryCompletionMessage = 0x80398554u;
constexpr u16 kOsThreadWaiting = 4u;
constexpr u32 kAramList0Global = 0x804946F4u;
constexpr u32 kAramList1Global = 0x80494724u;
constexpr u32 kCardBlockGlobal = 0x80495960u;
constexpr u32 kCardControlStride = 0x108u;
constexpr u32 kCardResultOffset = 0x04u;
constexpr u32 kCardResultBusy = 0xFFFFFFFFu;
constexpr u32 kAudioObjectGlobal = 0x804A03A8u;
constexpr u32 kAudioBasicGlobal = 0x804A1DD0u;
constexpr u32 kAudioStaticObject = 0x803E3CF8u;
constexpr u32 kAudioVtable = 0x80383FB0u;
constexpr u32 kAudioSceneOffset = 0x50u;
constexpr u32 kAudioBootstrapHandleOffset = 0x64u;
constexpr u32 kAudioBootstrapSoundId = 0x80000800u;

constexpr u32 kDvdBusyPredicateAddr = 0x80006A5Cu;
constexpr u32 kDCInvalidateRangeAddr = 0x801D5DF4u;
constexpr u32 kDCStoreRangeAddr = 0x801D5E58u;
constexpr u32 kOSDisableInterruptsAddr = 0x801D85B0u;
constexpr u32 kOSRestoreInterruptsAddr = 0x801D85D8u;
constexpr u32 kOSDisableSchedulerAddr = 0x801DAE98u;
constexpr u32 kOSEnableSchedulerAddr = 0x801DAED8u;
constexpr u32 kGXInvalidateVtxCacheAddr = 0x801EF208u;
constexpr u32 kGXInvalidateTexAllAddr = 0x801F1C10u;
constexpr u32 kAudioChangeSoundSceneAddr = 0x8018D4E4u;

constexpr u32 kExpHeapVtable = 0x8038886Cu;
constexpr u32 kMemArchiveVtable = 0x80388D5Cu;
constexpr u32 kRarcMagic = 0x52415243u;  // 'RARC'
constexpr u32 kMem1Start = 0x80000000u;
constexpr u32 kMem1End = 0x81800000u;
#if IS_EMULATOR
constexpr u32 kSnapshotBase = SUSAMUNE_DOLPHIN_SNAPSHOT_PPC_BASE;
#else
constexpr u32 kSnapshotBase = SUSAMUNE_MEM2_SNAPSHOT_PPC_BASE;
#endif
constexpr u32 kSnapshotStorageSize = SUSAMUNE_MEM2_SNAPSHOT_SIZE;
constexpr u32 kSnapshotMagic = 0x4C4D5354u;  // 'LMST'
constexpr u32 kSnapshotVersion = 28u;
constexpr u32 kHeaderSize = 0x100u;
constexpr u32 kHeapMetadataStart = 0x3Cu;
constexpr u32 kHeapMetadataEnd = 0x84u;
constexpr u32 kExpHeapAlignment = 16u;
constexpr u32 kHeapModeOffset = 0x68u;
constexpr u32 kHeapGroupOffset = 0x69u;
constexpr u32 kHeapMetadataSize = kHeapMetadataEnd - kHeapMetadataStart;
constexpr u32 kHeapMetadataOffset = kHeaderSize;
// LM's fixed fade/wipe controller is advanced by fn_80039338 immediately
// after MAIN GAME update and reversed by fn_800391F0 in the loop tail. Rewind
// only its scalar controller fields. The middle 0x17C bytes are an embedded
// J2DPicture with live texture and tree pointers and must remain live.
constexpr u32 kTransitionHeaderStateStart = 0x803985D4u;
constexpr u32 kTransitionHeaderStateEnd = 0x803985E8u;
constexpr u32 kTransitionTailStateStart = 0x80398764u;
constexpr u32 kTransitionTailStateEnd = 0x80398770u;
// LM's camera/viewport object and its four scalar draw-state words live in
// BSS below the game-static window. Stop before the following live display
// object, which owns boot-allocated double-buffer pointers.
constexpr u32 kRendererStateStart = 0x80398770u;
constexpr u32 kRendererStateEnd = 0x803989E0u;
// The view callbacks rebuild the renderer block from these fixed camera
// descriptors and manager tables every frame. They survive room streaming,
// so leaving them live produces a restored Luigi under the future camera.
constexpr u32 kCameraDescriptorStateStart = 0x80398BF8u;
constexpr u32 kCameraDescriptorStateEnd = 0x80398C50u;
constexpr u32 kCameraManagerStateStart = 0x80399B60u;
constexpr u32 kCameraManagerStateEnd = 0x80399C60u;
constexpr u32 kCameraObjectPointerTable = 0x80399BE0u;
constexpr u32 kCameraObjectCount = 3u;
constexpr u32 kCameraObjectSize = 0xECu;
constexpr u32 kCameraObjectRecordSize = 0x100u;
// The room event/text interpreter owns heap script pointers and its adjacent
// request record. Capturing only its flag bitmap leaves a destination-room
// program counter live after the gameplay heap rewinds.
constexpr u32 kRoomEventStateStart = 0x803C7CA0u;
constexpr u32 kRoomEventStateEnd = 0x803C8428u;
// LM keeps the actor count in captured SBSS, but the matching 128-entry pointer
// vector is fixed here. Rewind both halves so iteration cannot pair a saved
// count with destination-room actors.
constexpr u32 kRoomActorTableStart = 0x803C8490u;
constexpr u32 kRoomActorTableEnd = 0x803C8690u;
constexpr u32 kRoomActorCountGlobal = 0x804A12B8u;
constexpr u32 kRoomActorCapacity = 0x80u;
// Room-prop pictures own GAME objects; their view origin must rewind with them.
constexpr u32 kRoomPropPictureStateStart = 0x803C1C60u;
constexpr u32 kRoomPropPictureStateEnd = 0x803C1C98u;
// Door and room effects keep a fixed registry of pointers into the transient
// animated-model pool. Rewind the owners with their heap-resident slots so a
// future cleanup cannot retire a slot restored from the saved epoch.
constexpr u32 kAnimatedModelOwnerStateStart = 0x803C26C8u;
constexpr u32 kAnimatedModelOwnerStateEnd = 0x803C2D94u;
// The scene VR archive has a separate fixed model/pane pointer table.
constexpr u32 kVrSceneOwnerStateStart = 0x803C24E8u;
constexpr u32 kVrSceneOwnerStateEnd = 0x803C26C8u;
constexpr u32 kVrSceneArchiveGlobal = 0x804A0F10u;
// Door-side and room-activation masks are fixed scalar state updated alongside
// the heap door entries. A rewind must not leave their future-room bits set.
constexpr u32 kRoomVisibilityMaskStateStart = 0x803C2E10u;
constexpr u32 kRoomVisibilityMaskStateEnd = 0x803C3030u;
// Event objects and their archives live in the gameplay heap, but this
// pointer-free byte map records which numbered events are active.  Leaving it
// in the destination epoch re-arms foyer cutscenes before Luigi reaches a door.
constexpr u32 kEventActiveStateStart = 0x803C20C8u;
constexpr u32 kEventActiveStateEnd = 0x803C2138u;
// RoomInfo caches a borrowed Mission ToolData pointer, room lookup indices and
// field indices. Map reload rebuilds it; restore it with the GAME-owned table.
constexpr u32 kRoomInfoStateStart = 0x803C2138u;
constexpr u32 kRoomInfoStateEnd = 0x803C236Cu;
// FurnitureInfo borrows a Mission ToolData and caches its JMP field indices
// plus decoded row properties. Door setup consumes this owner after a rewind.
constexpr u32 kFurnitureInfoStateStart = 0x803C236Cu;
constexpr u32 kFurnitureInfoStateEnd = 0x803C2468u;
// Per-room lookup bytes derived from RoomInfo, and map UI room records/vectors.
// Both are plain scene scalars; neither range owns a service or hardware queue.
constexpr u32 kRoomMapLookupStateStart = 0x803C2468u;
constexpr u32 kRoomMapLookupStateEnd = 0x803C24E8u;
constexpr u32 kRoomMapUiStateStart = 0x803C1C98u;
constexpr u32 kRoomMapUiStateEnd = 0x803C20C8u;
// The room-name presenter keeps ten owned J2DPicture pointers in this fixed
// wrapper array while the pictures themselves live in the gameplay heap. The
// retail constructor/destructor treats both adjacent map symbols as one unit.
constexpr u32 kRoomNameOwnerStateStart = 0x803C4628u;
constexpr u32 kRoomNameOwnerStateEnd = 0x803C4718u;
constexpr u32 kRoomNameWrapperSize = 0x18u;
constexpr u32 kRoomNameWrapperCount = 10u;
constexpr u32 kRoomNamePictureOffset = 0x10u;
constexpr u32 kRoomNameSpareOffset = 0x14u;
constexpr u32 kRoomNamePictureVtable = 0x802F97DCu;
// Scene HUD pictures/screens and fade controllers own GAME allocations.
// The gap between ranges contains a destructor record and boot-owned font.
constexpr u32 kGbhHudOwnerStateStart = 0x803C3238u;
constexpr u32 kGbhHudOwnerStateEnd = 0x803C3388u;
constexpr u32 kHudPictureOwnerStateStart = 0x803C3400u;
constexpr u32 kHudPictureOwnerStateEnd = 0x803C3730u;
// Dialogue text, choices, picture owners and channels share captured SBSS cursors.
constexpr u32 kDialogueOwnerStateStart = 0x803C3730u;
constexpr u32 kDialogueOwnerStateEnd = 0x803C4448u;
constexpr u32 kTimerHudOwnerStateStart = 0x803C4448u;
constexpr u32 kTimerHudOwnerStateEnd = 0x803C4628u;
constexpr u32 kElementHudOwnerStateStart = 0x803C4718u;
constexpr u32 kElementHudOwnerStateEnd = 0x803C4868u;
constexpr u32 kBooRadarOwnerStateStart = 0x803C49C0u;
constexpr u32 kBooRadarOwnerStateEnd = 0x803C49D8u;
// Model-render scratch includes a borrowed GAME matrix pointer, not hardware state.
constexpr u32 kModelRenderContextStateStart = 0x803C4A10u;
constexpr u32 kModelRenderContextStateEnd = 0x803C4A50u;
// Scene-owned texture addresses AND embedded GX descriptors must rewind together.
constexpr u32 kDepthTextureOwnerStateStart = 0x803C4B6Cu;
constexpr u32 kDepthTextureOwnerStateEnd = 0x803C4C80u;
// The grain nodes are game-heap allocations, but both circular-list sentinels
// live in these adjacent BSS managers and must rewind with their node links.
constexpr u32 kGrainManagerStateStart = 0x803CBAF0u;
constexpr u32 kGrainManagerStateEnd = 0x803CC460u;
// Three scene-owned model managers contain roots into the rewound game heap.
// Skip each intervening 12-byte global-destructor registration record.
constexpr u32 kModelEffectManager0StateStart = 0x803CC9A4u;
constexpr u32 kModelEffectManager1StateStart = 0x803CCC58u;
constexpr u32 kModelEffectManager2StateStart = 0x803CCF0Cu;
constexpr u32 kModelEffectManagerStateSize = 0x2A8u;
// Lazy scene manager: include its initialized flag, not its destructor record.
constexpr u32 kLazyModelEffectStateStart = 0x803CC46Cu;
constexpr u32 kLazyModelEffectStateEnd = 0x803CC718u;
// Retained player/effect queries borrow GAME pointers; stop before registration.
constexpr u32 kPlayerQueryCacheStateStart = 0x803CC718u;
constexpr u32 kPlayerQueryCacheStateEnd = 0x803CC818u;
// This scene-effect manager walks game-heap nodes from fixed intrusive-list
// anchors. 0.3.25's terminal journal stopped inside fn_80155118 while reading
// this exact manager after a room rewind. Stop at CD4C8; separate globals fill
// the gap before the JPA manager at CD4FC.
constexpr u32 kSceneEffectManagerStateStart = 0x803CD1F4u;
constexpr u32 kSceneEffectManagerStateEnd = 0x803CD4C8u;
// The JPA emitter manager owns fixed intrusive-list anchors and counters while
// its emitter/particle pools live in the rewound gameplay heap. Rewind the
// complete fixed manager so those two halves cannot describe different eras.
constexpr u32 kParticleManagerStateStart = 0x803CD4FCu;
constexpr u32 kParticleManagerStateEnd = 0x803CE0F0u;
constexpr u32 kParticleEmitterPoolCount = 0x80u;
constexpr u32 kParticleEmitterSize = 0x2A0u;
constexpr u32 kParticlePoolCount = 0x400u;
constexpr u32 kParticleSize = 0x64u;
constexpr u32 kParticleChildPoolCount = 0x400u;
constexpr u32 kParticleChildSize = 0x2Cu;
// Scene effects keep their list sentinels, pointer vectors, and active counts
// here while the linked nodes live in the gameplay heap. Stop before CEB00's
// destructor records; CEBA0 begins an async object with an OSMessageQueue.
constexpr u32 kEffectControllerStateStart = 0x803CE0F0u;
constexpr u32 kEffectControllerStateEnd = 0x803CEB00u;
constexpr u32 kMainLoopStateSize = 0x08u;
// Leave the live heap-group byte, fixed render-mode pointers, and sCurScene
// outside the copy. They are exact epoch gates, not state to rewind.
constexpr u32 kGameSdata0Start = 0x80498AF8u;
constexpr u32 kGameSdata0End = 0x80498B18u;
constexpr u32 kGameSdata1Start = 0x80498B20u;
constexpr u32 kGameSdata1End = 0x804A03A8u;
// 0x804A0BF8 is a live JUTGamePad pointer; 0x804A1D10 begins an audio list.
constexpr u32 kGameSbss0Start = 0x804A0C00u;
constexpr u32 kGameSbss0End = 0x804A0C90u;
// BootScene owns 0x804A0C90-0x804A0CB0, including live picture/archive
// pointers. MissionMode snapshots never need any part of that block.
constexpr u32 kGameSbss1Start = 0x804A0CB0u;
constexpr u32 kGameSbss1End = 0x804A1D10u;
// Room changes replace a small prefix of LM's mounted model archives.  The
// archive objects and payloads live in the rewound game heap, while these
// owner tables, output arrays, and room-streamer slots live in fixed BSS.
constexpr u32 kModelTableBase = 0x803435ACu;
constexpr u32 kModelEntryCount = 262u;
constexpr u32 kModelEntrySize = 0x34u;
constexpr u32 kModelTableSize = kModelEntryCount * kModelEntrySize;
constexpr u32 kModelRegistryBase = 0x8037EC70u;
constexpr u32 kModelRegistryEntrySize = 0x40u;
constexpr u32 kModelRegistrySize =
    kModelEntryCount * kModelRegistryEntrySize;
constexpr u32 kModelCensusMetadataSize = 5u * sizeof(u32);
constexpr u32 kModelCensusRecordSize =
    kModelCensusMetadataSize + kModelTableSize + kModelRegistrySize;
// All potentially large guard workspaces live at the very end of the dedicated
// MEM2 snapshot reservation.  They are outside kSnapshotCapacity, so copying a
// snapshot can neither persist nor overwrite them.  This keeps the injected
// MEM1 image small even when the census limits are raised for whole-mansion
// testing.
constexpr u32 kVolumeCensusRecordSize = 0x1828u;
constexpr u32 kVolumeDiffRecordSize = 0x228u;
constexpr u32 kResourceCensusRecordSize = 0xC4u;
constexpr u32 kResourceDiffRecordSize = 0x38u;
constexpr u32 kModelDiffRecordSize = 0x19A8u;
constexpr u32 kCrossRoomCensusScratchSize =
    2u * kVolumeCensusRecordSize + kVolumeDiffRecordSize +
    2u * kResourceCensusRecordSize + kResourceDiffRecordSize +
    kModelDiffRecordSize;
constexpr u32 kModelCensusScratchSize =
    kModelCensusRecordSize + kModelCensusMetadataSize;
constexpr u32 kGuardScratchSize =
    kCrossRoomCensusScratchSize + kModelCensusScratchSize;
constexpr u32 kSnapshotCapacity =
    kSnapshotStorageSize - kGuardScratchSize;
constexpr u32 kGuardScratchAddress = kSnapshotBase + kSnapshotCapacity;
constexpr u32 kSavedVolumeCensusAddress = kGuardScratchAddress;
constexpr u32 kLiveVolumeCensusAddress =
    kSavedVolumeCensusAddress + kVolumeCensusRecordSize;
constexpr u32 kVolumeDiffAddress =
    kLiveVolumeCensusAddress + kVolumeCensusRecordSize;
constexpr u32 kSavedResourceCensusAddress =
    kVolumeDiffAddress + kVolumeDiffRecordSize;
constexpr u32 kLiveResourceCensusAddress =
    kSavedResourceCensusAddress + kResourceCensusRecordSize;
constexpr u32 kResourceDiffAddress =
    kLiveResourceCensusAddress + kResourceCensusRecordSize;
constexpr u32 kModelDiffAddress =
    kResourceDiffAddress + kResourceDiffRecordSize;
constexpr u32 kLiveModelCensusAddress =
    kModelDiffAddress + kModelDiffRecordSize;
constexpr u32 kSavedModelCensusMetadataAddress =
    kLiveModelCensusAddress + kModelCensusRecordSize;
constexpr u32 kResourceMapBase = 0x80398C50u;
constexpr u32 kResourceMapSize = 0x200u;
constexpr u32 kResourceActiveBase = 0x80398E90u;
constexpr u32 kResourceBackingBase = 0x80398ECCu;
constexpr u32 kResourceMarkBase = 0x80398F08u;
constexpr u32 kResourceWantedBase = 0x80398F68u;
constexpr u32 kResourceStateEnd = 0x80398FC8u;
// These fixed lookup maps, transition records, and bank pointers are consumed
// together when a door is touched. Their entries point into the gameplay heap,
// so they must belong to the same epoch as the restored door objects.
constexpr u32 kDoorVisibilityStateStart = 0x80399510u;
constexpr u32 kDoorVisibilityStateEnd = 0x80399B30u;
constexpr u32 kModelOutputStateStart = 0x803C86A0u;
constexpr u32 kModelOutputStateEnd = 0x803C97C4u;
constexpr u32 kModelRegistryOutputStateStart = 0x803E3088u;
constexpr u32 kModelRegistryOutputStateEnd = 0x803E3CF8u;

struct StaticRange {
    u32 address;
    u32 size;
};

// These audited GLMJ01 ranges exclude identified live OS, JSystem, BootScene,
// and audio state. Only the first two words of lbl_80398A40 are scalars;
// +0x08 begins an OSMessageQueue.
constexpr StaticRange kStateStaticRanges[] = {
    {kTransitionHeaderStateStart,
     kTransitionHeaderStateEnd - kTransitionHeaderStateStart},
    {kTransitionTailStateStart,
     kTransitionTailStateEnd - kTransitionTailStateStart},
    {kRendererStateStart, kRendererStateEnd - kRendererStateStart},
    {kCameraDescriptorStateStart,
     kCameraDescriptorStateEnd - kCameraDescriptorStateStart},
    {kDoorVisibilityStateStart,
     kDoorVisibilityStateEnd - kDoorVisibilityStateStart},
    {kCameraManagerStateStart,
     kCameraManagerStateEnd - kCameraManagerStateStart},
    {kModelTableBase, kModelTableSize},
    {kModelRegistryBase, kModelRegistrySize},
    {kResourceMapBase, kResourceStateEnd - kResourceMapBase},
    {kRoomEventStateStart, kRoomEventStateEnd - kRoomEventStateStart},
    {kRoomActorTableStart, kRoomActorTableEnd - kRoomActorTableStart},
    {kRoomPropPictureStateStart,
     kRoomPropPictureStateEnd - kRoomPropPictureStateStart},
    {kAnimatedModelOwnerStateStart,
     kAnimatedModelOwnerStateEnd - kAnimatedModelOwnerStateStart},
    {kRoomVisibilityMaskStateStart,
     kRoomVisibilityMaskStateEnd - kRoomVisibilityMaskStateStart},
    {kEventActiveStateStart,
     kEventActiveStateEnd - kEventActiveStateStart},
    {kRoomNameOwnerStateStart,
     kRoomNameOwnerStateEnd - kRoomNameOwnerStateStart},
    {kGbhHudOwnerStateStart, kGbhHudOwnerStateEnd - kGbhHudOwnerStateStart},
    {kHudPictureOwnerStateStart,
     kHudPictureOwnerStateEnd - kHudPictureOwnerStateStart},
    {kDialogueOwnerStateStart, kDialogueOwnerStateEnd - kDialogueOwnerStateStart},
    {kTimerHudOwnerStateStart, kTimerHudOwnerStateEnd - kTimerHudOwnerStateStart},
    {kElementHudOwnerStateStart,
     kElementHudOwnerStateEnd - kElementHudOwnerStateStart},
    {kBooRadarOwnerStateStart, kBooRadarOwnerStateEnd - kBooRadarOwnerStateStart},
    {kModelRenderContextStateStart,
     kModelRenderContextStateEnd - kModelRenderContextStateStart},
    {kDepthTextureOwnerStateStart, kDepthTextureOwnerStateEnd - kDepthTextureOwnerStateStart},
    {kVrSceneOwnerStateStart, kVrSceneOwnerStateEnd - kVrSceneOwnerStateStart},
    {kModelOutputStateStart,
     kModelOutputStateEnd - kModelOutputStateStart},
    {kGrainManagerStateStart,
     kGrainManagerStateEnd - kGrainManagerStateStart},
    {kModelEffectManager0StateStart, kModelEffectManagerStateSize},
    {kModelEffectManager1StateStart, kModelEffectManagerStateSize},
    {kModelEffectManager2StateStart, kModelEffectManagerStateSize},
    {kLazyModelEffectStateStart, kLazyModelEffectStateEnd - kLazyModelEffectStateStart},
    {kPlayerQueryCacheStateStart,
     kPlayerQueryCacheStateEnd - kPlayerQueryCacheStateStart},
    {kSceneEffectManagerStateStart,
     kSceneEffectManagerStateEnd - kSceneEffectManagerStateStart},
    {kParticleManagerStateStart,
     kParticleManagerStateEnd - kParticleManagerStateStart},
    {kEffectControllerStateStart,
     kEffectControllerStateEnd - kEffectControllerStateStart},
    {kModelRegistryOutputStateStart,
     kModelRegistryOutputStateEnd - kModelRegistryOutputStateStart},
    {kMainLoopStateBase, kMainLoopStateSize},
    {kGameSdata0Start, kGameSdata0End - kGameSdata0Start},
    {kGameSdata1Start, kGameSdata1End - kGameSdata1Start},
    {kGameSbss0Start, kGameSbss0End - kGameSbss0Start},
    {kGameSbss1Start, kGameSbss1End - kGameSbss1Start},
    {kRoomInfoStateStart, kRoomInfoStateEnd - kRoomInfoStateStart},
    {kFurnitureInfoStateStart, kFurnitureInfoStateEnd - kFurnitureInfoStateStart},
    {kRoomMapLookupStateStart, kRoomMapLookupStateEnd - kRoomMapLookupStateStart},
    {kRoomMapUiStateStart, kRoomMapUiStateEnd - kRoomMapUiStateStart},
    // Restore the JKR list anchors only after every archive owner table.
    {kVolumeListGlobal, 3u * sizeof(u32)},
    {kCurrentVolumeGlobal, sizeof(u32)},
    {kCurrentDirIdGlobal, sizeof(u32)},
};
constexpr u32 kStateStaticRangeCount =
    sizeof(kStateStaticRanges) / sizeof(kStateStaticRanges[0]);
constexpr u32 kStateStaticsOffset =
    kHeapMetadataOffset + kHeapMetadataSize;
constexpr u32 kModelTableSnapshotOffset =
    kStateStaticsOffset +
    (kTransitionHeaderStateEnd - kTransitionHeaderStateStart) +
    (kTransitionTailStateEnd - kTransitionTailStateStart) +
    (kRendererStateEnd - kRendererStateStart) +
    (kCameraDescriptorStateEnd - kCameraDescriptorStateStart) +
    (kDoorVisibilityStateEnd - kDoorVisibilityStateStart) +
    (kCameraManagerStateEnd - kCameraManagerStateStart);
constexpr u32 kModelRegistrySnapshotOffset =
    kModelTableSnapshotOffset + kModelTableSize;
constexpr u32 kStateStaticsSize =
    (kTransitionHeaderStateEnd - kTransitionHeaderStateStart) +
    (kTransitionTailStateEnd - kTransitionTailStateStart) +
    (kRendererStateEnd - kRendererStateStart) +
    (kCameraDescriptorStateEnd - kCameraDescriptorStateStart) +
    (kCameraManagerStateEnd - kCameraManagerStateStart) +
    (kDoorVisibilityStateEnd - kDoorVisibilityStateStart) + kModelTableSize +
    kModelRegistrySize + (kResourceStateEnd - kResourceMapBase) +
    (kRoomEventStateEnd - kRoomEventStateStart) +
    (kRoomActorTableEnd - kRoomActorTableStart) +
    (kRoomPropPictureStateEnd - kRoomPropPictureStateStart) +
    (kAnimatedModelOwnerStateEnd - kAnimatedModelOwnerStateStart) +
    (kRoomVisibilityMaskStateEnd - kRoomVisibilityMaskStateStart) +
    (kEventActiveStateEnd - kEventActiveStateStart) +
    (kRoomNameOwnerStateEnd - kRoomNameOwnerStateStart) +
    (kGbhHudOwnerStateEnd - kGbhHudOwnerStateStart) +
    (kHudPictureOwnerStateEnd - kHudPictureOwnerStateStart) +
    (kDialogueOwnerStateEnd - kDialogueOwnerStateStart) +
    (kTimerHudOwnerStateEnd - kTimerHudOwnerStateStart) +
    (kElementHudOwnerStateEnd - kElementHudOwnerStateStart) +
    (kBooRadarOwnerStateEnd - kBooRadarOwnerStateStart) +
    (kModelRenderContextStateEnd - kModelRenderContextStateStart) +
    (kDepthTextureOwnerStateEnd - kDepthTextureOwnerStateStart) +
    (kVrSceneOwnerStateEnd - kVrSceneOwnerStateStart) +
    (kModelOutputStateEnd - kModelOutputStateStart) +
    (kGrainManagerStateEnd - kGrainManagerStateStart) + kMainLoopStateSize +
    3u * kModelEffectManagerStateSize +
    (kLazyModelEffectStateEnd - kLazyModelEffectStateStart) +
    (kPlayerQueryCacheStateEnd - kPlayerQueryCacheStateStart) +
    (kSceneEffectManagerStateEnd - kSceneEffectManagerStateStart) +
    (kParticleManagerStateEnd - kParticleManagerStateStart) +
    (kEffectControllerStateEnd - kEffectControllerStateStart) +
    (kModelRegistryOutputStateEnd - kModelRegistryOutputStateStart) +
    (kGameSdata0End - kGameSdata0Start) +
    (kGameSdata1End - kGameSdata1Start) +
    (kGameSbss0End - kGameSbss0Start) +
    (kGameSbss1End - kGameSbss1Start) +
    (kRoomInfoStateEnd - kRoomInfoStateStart) +
    (kFurnitureInfoStateEnd - kFurnitureInfoStateStart) +
    (kRoomMapLookupStateEnd - kRoomMapLookupStateStart) +
    (kRoomMapUiStateEnd - kRoomMapUiStateStart) + 5u * sizeof(u32);
constexpr u32 kCameraObjectStateOffset =
    kStateStaticsOffset + kStateStaticsSize;
constexpr u32 kCameraObjectStateSize =
    kCameraObjectCount * kCameraObjectRecordSize;
constexpr u32 kHeapDataOffset =
    (kCameraObjectStateOffset + kCameraObjectStateSize + 31u) & ~31u;
constexpr u16 kDPadLeft = 0x0001u;
constexpr u16 kDPadRight = 0x0002u;
constexpr u16 kButtonA = 0x0100u;
constexpr u32 kPadStickXOffset = 2u;
constexpr u32 kPadStickYOffset = 3u;
constexpr u32 kPadMovementDeadzone = 24u;
constexpr u32 kRequiredStableFrames = 3u;
constexpr u32 kPostLoadTraceFrameLimit = 8u;
constexpr u32 kPostLoadTraceLingeringFrameLimit = 7200u;
constexpr u32 kPostLoadInputBurstUpdates = 2u;
constexpr u32 kPostLoadTraceHeartbeatFrames = 300u;
constexpr u32 kPostLoadDoorWindowFrames = 240u;
constexpr u32 kPostLoadTransitionBurstUpdates = 2u;
constexpr u32 kMaxVolumes = 64u;
// A valid replacement retains at least one common long-lived volume, so an
// endpoint can replace at most every other slot. Keep every changed index;
// distance must not become an artificial bookkeeping limit.
constexpr u32 kVolumeRemovedSlots = kMaxVolumes - 1u;
constexpr u32 kVolumeAddedSlots = kMaxVolumes - 1u;
constexpr u32 kVolumeDisplayedPerKind = 3u;
constexpr u32 kVolumeChangeRows =
    2u * kVolumeDisplayedPerKind;
constexpr u32 kVolumeNameBytes = 16u;
constexpr u32 kVolumeNameHashBytes = 32u;
constexpr u32 kVolumeNameValid = 1u << 0;
constexpr u32 kVolumeArchiveValid = 1u << 1;
constexpr u32 kVolumeRarcValid = 1u << 2;
constexpr u32 kVolumeMounted = 1u << 8;
constexpr u32 kVolumeModeShift = 16u;
constexpr u32 kVolumeDirectionShift = 20u;
constexpr u32 kVolumeOpen = 1u << 24;
constexpr u32 kVolumeObjectOwnerShift = 0u;
constexpr u32 kVolumeArchiveOwnerShift = 4u;
constexpr u32 kVolumeObjectLocationShift = 8u;
constexpr u32 kVolumeBackingLocationShift = 12u;
constexpr u32 kVolumeOwnerMask = 0xFu;
constexpr u32 kResourceSlotCount = 7u;
constexpr u32 kResourceWantedCapacity = 24u;
constexpr u32 kResourceRecordSize = 0x40u;
constexpr u32 kResourceSlotSize = 0x70800u;
constexpr u32 kModelChangeSlots =
    kVolumeRemovedSlots + kVolumeAddedSlots;
constexpr u32 kRejectTelemetryRecordCount = 6u;
constexpr u32 kRejectTelemetryHoldFrames = 8u;
constexpr u32 kRejectSummaryPhase = 0xD0u;
constexpr u32 kRejectSavedIdentityPhase = 0xD1u;
constexpr u32 kRejectLiveIdentityPhase = 0xD2u;
constexpr u32 kModelNameBytes = 9u;
constexpr u32 kModelPathLimit = 96u;
constexpr u32 kModelChangedPrimary = 1u << 0;
constexpr u32 kModelChangedRegistry = 1u << 1;

enum ResourceFault : u32 {
    kResourceFaultNone = 0u,
    kResourceFaultSlotCount,
    kResourceFaultWantedCount,
    kResourceFaultRecords,
    kResourceFaultBulk,
    kResourceFaultSlotSize,
};

enum ModelFault : u32 {
    kModelFaultNone = 0u,
    kModelFaultChanged,
};

enum CrossRoomGuard : u32 {
    kCrossRoomGuardNone = 0u,
    kCrossRoomGuardMask = 1u,
    kCrossRoomGuardGeneration = 2u,
    kCrossRoomGuardVolume = 3u,
    kCrossRoomGuardTopology = 4u,
    kCrossRoomGuardArchive = 5u,
    kCrossRoomGuardResource = 6u,
    kCrossRoomGuardModel = 7u,
    kCrossRoomGuardModelShape = 8u,
    kCrossRoomGuardAccepted = 0xA0u,
};

enum VolumeFault : u32 {
    kVolumeFaultNone = 0u,
    kVolumeFaultCapacity,
    kVolumeFaultEmpty,
    kVolumeFaultEndpoint,
    kVolumeFaultNode,
    kVolumeFaultList,
    kVolumeFaultObject,
    kVolumeFaultEmbeddedLink,
    kVolumeFaultPrevious,
    kVolumeFaultDuplicate,
    kVolumeFaultTail,
    kVolumeFaultEnd,
    kVolumeFaultChanged,
};

enum VolumeOwner : u32 {
    kVolumeOwnerOther = 0u,
    kVolumeOwnerGame = 1u,
    kVolumeOwnerSystem = 2u,
    kVolumeOwnerRoot = 3u,
};
constexpr u32 kEventStateSave = 0x100u;
constexpr u32 kEventStateLoad = 0x101u;
constexpr u32 kEventStateReject = 0x10Fu;
constexpr u32 kEventStateSavePhase = 0x110u;
constexpr u32 kEventStateLoadPhase = 0x111u;

typedef bool (*BoolFn)();
typedef bool (*DisableInterruptsFn)();
typedef void (*RestoreInterruptsFn)(bool);
typedef s32 (*SchedulerFn)();
typedef void (*VoidFn)();
typedef void (*CacheRangeFn)(void *, u32);
typedef void (*AudioChangeSoundSceneFn)(void *, u32);

struct LiveIdentity {
    u32 heap;
    u32 heapStart;
    u32 heapEnd;
    u32 heapSize;
    u32 heapMode;
    u32 heapGroup;
    u32 heapFreeHead;
    u32 heapFreeTail;
    u32 heapUsedHead;
    u32 heapUsedTail;
    u32 rootHeap;
    u32 rootHeapStart;
    u32 rootHeapEnd;
    u32 rootHeapSize;
    u32 rootHeapMode;
    u32 rootHeapGroup;
    u32 rootFreeHead;
    u32 rootFreeTail;
    u32 rootUsedHead;
    u32 rootUsedTail;
    u32 systemHeap;
    u32 systemHeapStart;
    u32 systemHeapEnd;
    u32 systemHeapSize;
    u32 systemHeapMode;
    u32 systemHeapGroup;
    u32 systemFreeHead;
    u32 systemFreeTail;
    u32 systemUsedHead;
    u32 systemUsedTail;
    u32 currentHeap;
    u32 missionMode;
    u32 mapArchive;
    u32 volume[3];
    u32 mapValue;
    u32 sceneValue;
    u32 currentScene;
    u32 gameMode;
    u32 gameModeCount;
    u32 simpleModeler;
    u32 mapCol;
    u32 enTypesManager;
    u32 currentHeapGroup;
    u32 audioBasic;
    u32 audioScene;
    u32 mainLoopMode;
    u32 mainLoopPendingScene;
    u32 mainLoopScene;
    u32 mainDrawState;
    u32 mainLoopExit;
};

struct PostLoadTransitionWatch {
    u32 mapValue;
    u32 sceneValue;
    u32 pendingScene;
    u32 loopExit;
    u32 dvdOutstanding;
    u32 aram0;
    u32 aram1;
    u32 resourceWantedCount;
    u32 volumeTail;
    u32 volumeCount;
};

struct SnapshotHeader {
    u32 magic;
    u32 version;
    u32 headerSize;
    u32 gameId;
    u32 totalSize;
    u32 checksum;
    u32 generation;
    u32 heap;
    u32 heapStart;
    u32 heapEnd;
    u32 heapSize;
    u32 heapMetadataOffset;
    u32 heapMetadataSize;
    u32 stateStaticsOffset;
    u32 stateStaticsSize;
    u32 heapDataOffset;
    u32 heapDataSize;
    u32 rootHeap;
    u32 rootHeapStart;
    u32 rootHeapEnd;
    u32 rootHeapSize;
    u32 rootHeapMode;
    u32 rootHeapGroup;
    u32 rootFreeHead;
    u32 rootFreeTail;
    u32 rootUsedHead;
    u32 rootUsedTail;
    u32 systemHeap;
    u32 systemHeapStart;
    u32 systemHeapEnd;
    u32 systemHeapSize;
    u32 currentHeap;
    u32 missionMode;
    u32 mapArchive;
    u32 volume[3];
    u32 mapValue;
    u32 sceneValue;
    u32 currentScene;
    u32 gameMode;
    u32 gameModeCount;
    u32 heapMode;
    u32 heapGroup;
    u32 systemHeapMode;
    u32 systemHeapGroup;
    u32 systemFreeHead;
    u32 systemFreeTail;
    u32 systemUsedHead;
    u32 systemUsedTail;
    u32 currentHeapGroup;
    u32 randomState;
    u32 freeHead;
    u32 freeTail;
    u32 usedHead;
    u32 usedTail;
    u32 mainLoopMode;
    u32 mainLoopPendingScene;
    u32 mainDrawState;
    u32 simpleModeler;
    u32 mapCol;
    u32 enTypesManager;
    u32 audioBasic;
    u32 audioScene;
};

static_assert(sizeof(SnapshotHeader) == kHeaderSize,
              "LM snapshot header must remain one cache-aligned page");
#if !IS_EMULATOR
static_assert(kSnapshotBase + kSnapshotStorageSize ==
                  SUSAMUNE_MEM2_CFG_PPC_BASE,
              "LM state must end before the config/crash mailboxes");
#endif
static_assert(kGuardScratchSize < kSnapshotStorageSize,
              "LM guard scratch must fit inside snapshot storage");
static_assert(kSnapshotBase + kSnapshotCapacity ==
                  kGuardScratchAddress &&
                  kModelDiffAddress + kModelDiffRecordSize ==
                      kLiveModelCensusAddress &&
                  kSavedModelCensusMetadataAddress +
                          kModelCensusMetadataSize ==
                      kSnapshotBase + kSnapshotStorageSize,
              "LM guard scratch must occupy the snapshot tail");
static_assert((kGuardScratchSize & 31u) == 0u &&
                  kHeapDataOffset < kSnapshotCapacity,
              "LM snapshot payload and census scratch must remain disjoint");
static_assert(kDvdFileInfoArray +
                      kDvdFileInfoCount * kDvdFileInfoSize ==
                  kDvdCurrentGlobal,
              "LM primary DVD file-info array boundary drifted");
static_assert(kDvdPrimaryQueue + 0x20u == kDvdPrimaryMessages &&
                  kDvdPrimaryMessages +
                          kDvdFileInfoCount * sizeof(u32) ==
                      kDvdPrimaryCursor,
              "LM primary DVD worker layout drifted");
static_assert(kDvdSecondaryRequestQueue + 0x20u ==
                      kDvdSecondaryRequestMessage &&
                  kDvdSecondaryCompletionQueue + 0x20u ==
                      kDvdSecondaryCompletionMessage,
              "LM secondary DVD worker layout drifted");
static_assert((kHeapDataOffset & 31u) == 0,
              "LM heap payload must be cache-line aligned");
static_assert(kStateStaticsSize == 0x18D6Cu,
              "LM static manifest size drifted");
static_assert(kCameraObjectStateOffset == 0x18EB4u,
              "LM camera-object sidecar offset drifted");
static_assert(kCameraObjectStateSize == 0x300u,
              "LM camera-object sidecar size drifted");
static_assert(kHeapDataOffset == 0x191C0u,
              "LM static manifest packing drifted");
static_assert(kEventActiveStateEnd == kRoomInfoStateStart &&
                  kRoomInfoStateEnd - kRoomInfoStateStart == 0x234u,
              "LM RoomInfo owner must stop before the adjacent descriptor table");
static_assert(kRoomInfoStateEnd == kFurnitureInfoStateStart &&
                  kFurnitureInfoStateEnd - kFurnitureInfoStateStart == 0xFCu &&
                  kFurnitureInfoStateEnd == kRoomMapLookupStateStart &&
                  kRoomMapLookupStateEnd - kRoomMapLookupStateStart == 0x80u &&
                  kRoomMapLookupStateEnd == kVrSceneOwnerStateStart &&
                  kRoomPropPictureStateEnd == kRoomMapUiStateStart &&
                  kRoomMapUiStateEnd - kRoomMapUiStateStart == 0x430u &&
                  kRoomMapUiStateEnd == kEventActiveStateStart,
              "LM scene lookup family boundaries drifted");
static_assert(kVrSceneOwnerStateEnd - kVrSceneOwnerStateStart == 0x1E0u &&
                  kVrSceneOwnerStateEnd == kAnimatedModelOwnerStateStart &&
                  kVrSceneArchiveGlobal >= kGameSbss1Start &&
                  kVrSceneArchiveGlobal + 4u <= kGameSbss1End,
              "LM VR scene owners must be captured together");
static_assert(kModelTableSnapshotOffset == 0xB50u &&
                  kModelRegistrySnapshotOffset == 0x4088u &&
                  kModelRegistrySnapshotOffset + kModelRegistrySize ==
                      0x8208u,
              "LM saved model census offsets drifted");
static_assert(kStateStaticRanges[6].address == kModelTableBase &&
                  kStateStaticRanges[6].size == kModelTableSize &&
                  kStateStaticRanges[7].address == kModelRegistryBase &&
                  kStateStaticRanges[7].size == kModelRegistrySize,
              "LM model census ranges left their packed snapshot offsets");
static_assert(kTransitionHeaderStateEnd - kTransitionHeaderStateStart == 0x14u,
              "LM transition header snapshot boundary drifted");
static_assert(kTransitionTailStateEnd - kTransitionTailStateStart == 0x0Cu,
              "LM transition tail snapshot boundary drifted");
static_assert(kTransitionHeaderStateEnd == 0x803985E8u &&
                  kTransitionTailStateStart == 0x80398764u,
              "LM embedded J2DPicture exclusion drifted");
static_assert(kTransitionTailStateEnd == kRendererStateStart,
              "LM transition tail must abut renderer state");
static_assert(kRendererStateEnd - kRendererStateStart == 0x270u,
              "LM renderer snapshot boundary drifted");
static_assert(kCameraDescriptorStateEnd - kCameraDescriptorStateStart ==
                  0x58u,
              "LM camera descriptor snapshot boundary drifted");
static_assert(kCameraManagerStateEnd - kCameraManagerStateStart == 0x100u,
              "LM camera manager snapshot boundary drifted");
static_assert(kDoorVisibilityStateEnd - kDoorVisibilityStateStart == 0x620u,
              "LM door visibility snapshot boundary drifted");
static_assert(kDoorVisibilityStateEnd + 0x30u == kCameraManagerStateStart,
              "LM door state must stop before the excluded float table");
static_assert(kCameraObjectPointerTable >= kCameraManagerStateStart &&
                  kCameraObjectPointerTable +
                          kCameraObjectCount * sizeof(u32) <=
                      kCameraManagerStateEnd,
              "LM camera-object roots left the captured manager range");
static_assert(kCameraDescriptorStateEnd == kResourceMapBase,
              "LM camera descriptors must stop before room resources");
static_assert(kGrainManagerStateEnd - kGrainManagerStateStart == 0x970u,
              "LM grain-manager snapshot boundary drifted");
static_assert(kTimerHudOwnerStateEnd - kTimerHudOwnerStateStart == 20u * 0x18u &&
                  kTimerHudOwnerStateEnd == kRoomNameOwnerStateStart &&
                  kElementHudOwnerStateStart == kRoomNameOwnerStateEnd &&
                  kElementHudOwnerStateEnd - kElementHudOwnerStateStart == 14u * 0x18u &&
                  kBooRadarOwnerStateEnd - kBooRadarOwnerStateStart == 0x18u,
              "LM scene HUD sibling ownership boundaries drifted");
static_assert(kDialogueOwnerStateStart == kHudPictureOwnerStateEnd &&
                  kDialogueOwnerStateEnd == kTimerHudOwnerStateStart &&
                  kDialogueOwnerStateEnd - kDialogueOwnerStateStart == 0xD18u,
              "LM dialogue buffers and picture owners must rewind together");
static_assert(kModelRenderContextStateEnd - kModelRenderContextStateStart == 0x40u,
              "LM model-render context boundary drifted");
static_assert(kLazyModelEffectStateStart == kGrainManagerStateEnd + 0xCu &&
                  kLazyModelEffectStateEnd - kLazyModelEffectStateStart == 0x2ACu,
              "LM lazy effect must include its flag but exclude registration");
static_assert(kPlayerQueryCacheStateStart == kLazyModelEffectStateEnd &&
                  kPlayerQueryCacheStateEnd - kPlayerQueryCacheStateStart == 0x100u,
              "LM player-query cache must stop before destructor registration");
static_assert(kGbhHudOwnerStateEnd - kGbhHudOwnerStateStart == 0x150u &&
                  kHudPictureOwnerStateEnd - kHudPictureOwnerStateStart ==
                      0x330u &&
                  kGbhHudOwnerStateEnd == 0x803C3388u &&
                  kHudPictureOwnerStateStart == 0x803C3400u,
              "LM HUD snapshot must exclude the destructor/font gap");
static_assert(kModelEffectManager0StateStart + kModelEffectManagerStateSize ==
                  kModelEffectManager1StateStart - 0xCu &&
                  kModelEffectManager1StateStart + kModelEffectManagerStateSize ==
                  kModelEffectManager2StateStart - 0xCu &&
                  kModelEffectManager2StateStart + kModelEffectManagerStateSize ==
                  0x803CD1B4u,
              "LM model-effect manager destructor exclusions drifted");
static_assert(kSceneEffectManagerStateEnd -
                      kSceneEffectManagerStateStart ==
                  0x2D4u,
              "LM scene-effect manager snapshot boundary drifted");
static_assert(kParticleManagerStateEnd - kParticleManagerStateStart == 0xBF4u,
              "LM particle-manager snapshot boundary drifted");
static_assert(kEffectControllerStateEnd - kEffectControllerStateStart ==
                  0xA10u,
              "LM effect-controller snapshot boundary drifted");
static_assert(kAnimatedModelOwnerStateEnd -
                      kAnimatedModelOwnerStateStart ==
                  0x6CCu,
              "LM animated-model owner registry boundary drifted");
static_assert(kRoomVisibilityMaskStateEnd -
                      kRoomVisibilityMaskStateStart ==
                  0x220u,
              "LM room-visibility mask boundary drifted");
static_assert(kEventActiveStateEnd - kEventActiveStateStart == 0x70u,
              "LM active-event bitmap boundary drifted");
static_assert(kRoomPropPictureStateEnd - kRoomPropPictureStateStart ==
                  10u * sizeof(u32) + 0x10u,
              "LM room-prop picture table and view origin must rewind together");
static_assert(kRoomNameOwnerStateEnd - kRoomNameOwnerStateStart == 0xF0u,
              "LM room-name owner boundary drifted");
static_assert(kRoomNameWrapperCount * kRoomNameWrapperSize ==
                  kRoomNameOwnerStateEnd - kRoomNameOwnerStateStart &&
                  kRoomNamePictureOffset + sizeof(u32) ==
                      kRoomNameSpareOffset &&
                  kRoomNameSpareOffset + sizeof(u32) ==
                      kRoomNameWrapperSize,
              "LM room-name wrapper layout drifted");
static_assert(kGameSdata0End == kCurrentSceneGlobal &&
                  kGameSdata1Start == kCurrentSceneGlobal + 8u,
              "LM sCurScene must remain an uncaptured epoch gate");
static_assert(kGameSdata1End == kAudioObjectGlobal,
              "LM game sdata must stop before live audio state");
static_assert(kGameSbss1End < kAudioBasicGlobal,
              "LM game sbss must stop before live audio state");
static_assert(kResourceStateEnd - kResourceMapBase == 0x378u,
              "LM room-streamer snapshot boundary drifted");
static_assert(kRoomEventStateEnd - kRoomEventStateStart == 0x788u,
              "LM room-event snapshot boundary drifted");
static_assert(kRoomEventStateEnd + 0x68u == kRoomActorTableStart,
              "LM boot heap helpers must remain outside the room snapshot");
static_assert(kRoomActorTableEnd - kRoomActorTableStart ==
                  kRoomActorCapacity * sizeof(u32),
              "LM room-actor pointer table boundary drifted");
static_assert(kRoomActorTableEnd + 0x10u == kModelOutputStateStart,
              "LM actor table must stop before primary model outputs");
static_assert(kModelOutputStateEnd - kModelOutputStateStart == 0x1124u,
              "LM primary model-output boundary drifted");
static_assert(kModelRegistryOutputStateEnd -
                      kModelRegistryOutputStateStart ==
                  0xC70u,
              "LM registry-output boundary drifted");
static_assert(kModelRegistryOutputStateEnd == kAudioStaticObject,
              "LM registry outputs must stop before live audio state");
static_assert(kMainLoopSceneGlobal == kSceneValueGlobal,
              "LM loop scene must match the captured scene identity");
static_assert(kMainDrawStateGlobal >= kGameSbss0Start &&
                  kMainDrawStateGlobal + sizeof(u32) <= kGameSbss0End,
              "LM draw state must remain inside the captured game sbss");
static_assert(kRoomActorCountGlobal >= kGameSbss1Start &&
                  kRoomActorCountGlobal + sizeof(u32) <= kGameSbss1End,
              "LM room-actor count must remain inside captured game sbss");

struct FreezeState {
    bool interruptsWereEnabled;
    bool dmaWasEnabled;
};

enum class Gate : u32 {
    Ready,
    Boot,
    GameHeap,
    RootHeap,
    SystemHeap,
    Size,
    Distinct,
    SystemNest,
    GameNest,
    Overlap,
    CurrentHeap,
    MissionNull,
    MissionRange,
    ModeMismatch,
    ModeCount,
    GameRoot,
    Particle,
    RoomName,
    Scene,
    LoopMode,
    LoopExit,
    LoopScene,
    DrawState,
    DvdPredicate,
    DvdCount,
    DvdPrimary,
    DvdSecondary,
    Aram0,
    Aram1,
    Card0,
    Card1,
    Audio,
    Stability,
    IdentityChanged,
    Cameras,
    HeapHealth,
    RestoreProof,
    Grain,
    DoorOwner,
    Door,
};

struct EpochMismatch {
    u32 mask;
    u32 saved;
    u32 live;
};

struct VolumeDescriptor {
    u32 node;
    u32 object;
    u32 previous;
    u32 next;
    u32 vtable;
    u32 objectOwnerHeap;
    u32 archiveHeap;
    u32 namePointer;
    u32 nameHash;
    u32 type;
    u32 stateFlags;
    u32 mountCount;
    u32 mountSource;
    u32 archiveInfo;
    u32 archiveHeader;
    u32 archiveData;
    u32 fileLength;
    u32 dataLength;
    u32 ownerFlags;
    char name[kVolumeNameBytes];
    u32 contentSignature;
};

struct VolumeCensus {
    u32 generation;
    u32 valid;
    u32 fault;
    u32 count;
    u32 head;
    u32 tail;
    u32 currentVolume;
    u32 currentDirId;
    u32 signature;
    u32 stableFrames;
    VolumeDescriptor entries[kMaxVolumes];
};

struct VolumeDiff {
    u32 ready;
    u32 savedValid;
    u32 liveValid;
    u32 savedCount;
    u32 liveCount;
    u32 removedCount;
    u32 addedCount;
    u32 commonOrder;
    u32 headOnly;
    u32 currentChanged;
    u32 removedIndices[kVolumeRemovedSlots];
    u32 addedIndices[kVolumeAddedSlots];
    u32 objectReuseMask;
    u32 archiveReuseMask;
};

struct ResourceCensus {
    u32 generation;
    u32 valid;
    u32 fault;
    u32 slotCount;
    u32 wantedCount;
    u32 recordBase;
    u32 bulkBase;
    u32 slotSize;
    u32 mapHash;
    u32 markMask;
    u32 backingBadMask;
    u32 activeIds[kResourceSlotCount];
    u32 recordHashes[kResourceSlotCount];
    u32 wantedIds[kResourceWantedCapacity];
};

struct ResourceDiff {
    u32 ready;
    u32 savedValid;
    u32 liveValid;
    u32 activeMismatchMask;
    u32 recordMismatchMask;
    u32 layoutChanged;
    u32 mapChanged;
    u32 wantedSequenceChanged;
    u32 wantedRemovedCount;
    u32 wantedAddedCount;
    u32 wantedRemovedIds[2];
    u32 wantedAddedIds[2];
};

struct ModelCensusMetadata {
    u32 generation;
    u32 valid;
    u32 fault;
    u32 signature;
    u32 registrySignature;
};

struct ModelCensus {
    ModelCensusMetadata metadata;
    u32 words[kModelTableSize / sizeof(u32)];
    u32 registryWords[kModelRegistrySize / sizeof(u32)];
};

struct ModelCensusView {
    ModelCensusMetadata *metadata;
    u32 *words;
    u32 *registryWords;
};

struct ModelChange {
    u32 index;
    u32 sourceMask;
    u32 savedHandle;
    u32 liveHandle;
    u32 savedState;
    u32 liveState;
    u32 savedRoot;
    u32 liveRoot;
    u32 savedRegistrySignature;
    u32 liveRegistrySignature;
    char name[kModelNameBytes];
};

struct ModelDiff {
    u32 ready;
    u32 savedValid;
    u32 liveValid;
    u32 changedCount;
    ModelChange changes[kModelChangeSlots];
};

struct RejectTelemetryRecord {
    u32 phase;
    u32 arg0;
    u32 arg1;
};

static_assert(sizeof(VolumeDescriptor) == 0x60u,
              "LM volume descriptor layout drifted");
static_assert(kModelTableSize == 0x3538u,
              "LM model descriptor table range drifted");
static_assert(kModelRegistrySize == 0x4180u,
              "LM model registry table range drifted");
static_assert(kVolumeRemovedSlots < kMaxVolumes &&
                  kVolumeAddedSlots < kMaxVolumes &&
                  kModelChangeSlots >=
                      kVolumeRemovedSlots + kVolumeAddedSlots,
              "LM cross-room change storage cannot represent every delta");
static_assert(sizeof(ModelCensusMetadata) == kModelCensusMetadataSize,
              "LM model census metadata layout drifted");
static_assert(sizeof(ModelCensus) == kModelCensusRecordSize,
              "LM model census scratch layout drifted");
static_assert(kModelCensusRecordSize == 0x76CCu &&
                  kModelCensusScratchSize == 0x76E0u,
              "LM model census scratch size drifted");
static_assert(sizeof(VolumeCensus) == kVolumeCensusRecordSize &&
                  sizeof(VolumeDiff) == kVolumeDiffRecordSize &&
                  sizeof(ResourceCensus) == kResourceCensusRecordSize &&
                  sizeof(ResourceDiff) == kResourceDiffRecordSize &&
                  sizeof(ModelDiff) == kModelDiffRecordSize &&
                  kCrossRoomCensusScratchSize == 0x4DE0u &&
                  kGuardScratchSize == 0xC4C0u,
              "LM cross-room guard scratch layout drifted");

LMState::Status sStatus = LMState::Status::Empty;
LiveIdentity sLastIdentity = {};
Gate sGate = Gate::Boot;
Gate sRejectedGate = Gate::Boot;
u32 sRejectedGateValue;
EpochMismatch sEpochMismatch = {};
VolumeCensus &sSavedVolumeCensus =
    *reinterpret_cast<VolumeCensus *>(kSavedVolumeCensusAddress);
VolumeCensus &sLiveVolumeCensus =
    *reinterpret_cast<VolumeCensus *>(kLiveVolumeCensusAddress);
VolumeDiff &sVolumeDiff =
    *reinterpret_cast<VolumeDiff *>(kVolumeDiffAddress);
ResourceCensus &sSavedResourceCensus =
    *reinterpret_cast<ResourceCensus *>(kSavedResourceCensusAddress);
ResourceCensus &sLiveResourceCensus =
    *reinterpret_cast<ResourceCensus *>(kLiveResourceCensusAddress);
ResourceDiff &sResourceDiff =
    *reinterpret_cast<ResourceDiff *>(kResourceDiffAddress);
const ModelCensusView sSavedModelCensus = {
    reinterpret_cast<ModelCensusMetadata *>(
        kSavedModelCensusMetadataAddress),
    reinterpret_cast<u32 *>(kSnapshotBase + kModelTableSnapshotOffset),
    reinterpret_cast<u32 *>(kSnapshotBase + kModelRegistrySnapshotOffset),
};
const ModelCensusView sLiveModelCensus = {
    reinterpret_cast<ModelCensusMetadata *>(kLiveModelCensusAddress),
    reinterpret_cast<u32 *>(kLiveModelCensusAddress +
                            kModelCensusMetadataSize),
    reinterpret_cast<u32 *>(kLiveModelCensusAddress +
                            kModelCensusMetadataSize + kModelTableSize),
};
ModelDiff &sModelDiff =
    *reinterpret_cast<ModelDiff *>(kModelDiffAddress);
RejectTelemetryRecord sRejectTelemetryRecords[kRejectTelemetryRecordCount] = {};
u32 sRejectTelemetryCount;
u32 sRejectTelemetryIndex;
u32 sRejectTelemetryHoldFrames;
u32 sRejectTelemetryAction = SUSAMUNE_PHASE_ACTION_LOAD;
u32 sRequestAction;
bool sHaveIdentity;
bool sSlotInitialized;
u32 sStableFrames;
u32 sSnapshotSize;
u32 sGeneration;
u32 sTimelineRevision;
u32 sLoadRevision;
u32 sMenuRequest;
void initializeStorage();
bool storageStartupReady();
void savedSlotCommitted();
bool sPersistentKeyReady;
bool sPersistentLoaded;
u32 sPersistentConfigId;
LmPersistentProfile sSavedPersistentProfile, sLivePersistentProfile;
void serviceStorage();
bool storageInFlight();
u32 snapshotStoredSize(const SnapshotHeader *header);
bool snapshotCompanionValid(const SnapshotHeader *header, u32 storedSize);
bool sharedArchiveMatchesSnapshot(const SnapshotHeader *header, LmSharedArchiveDescriptor *live);
LmStateHotkeyLatch sHotkeys = {};
u32 sGateValue;
u32 sPostLoadTraceState;
u32 sPostLoadTraceFrame;
u32 sPostLoadTraceHeartbeat;
u16 sPostLoadTraceButtons;
bool sPostLoadTraceBurst;
bool sPostLoadTracePresentationBurst;
bool sPostLoadTraceMovement;
u32 sPostLoadDoorWindow;
u32 sPostLoadTraceBurstUpdates;
PostLoadTransitionWatch sPostLoadTransitionWatch;
u32 sPostLoadTransitionChangedMask;
u32 sPostLoadTransitionChangedBefore;
u32 sPostLoadTransitionChangedAfter;
u32 sCrossRoomGuard;
u32 sCrossRoomFault;
u32 sCrossRoomFaultValue;
u32 sUnmatchedVolumeIndex = 0xFFFFFFFFu;
u32 sUnmatchedVolumeObject, sUnmatchedVolumeName, sUnmatchedVolumeBacking;

void traceSavePhase(u32 phase, u32 detail) {
    sRequestAction = SUSAMUNE_PHASE_ACTION_SAVE;
    LMCrash::note(kEventStateSavePhase, phase, detail);
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_SAVE, phase, detail, sStableFrames);
}

void traceLoadPhase(u32 phase, u32 detail) {
    sRequestAction = SUSAMUNE_PHASE_ACTION_LOAD;
    LMCrash::note(kEventStateLoadPhase, phase, detail);
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, phase, detail, sStableFrames);
}

void tracePostLoadPhase(u32 phase, u32 detail = 0u) {
    if (sRejectTelemetryCount != 0u) {
        return;
    }
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, phase, detail,
                   sStableFrames);
}

void clearRejectTelemetry() {
    sRejectTelemetryCount = 0u;
    sRejectTelemetryIndex = 0u;
    sRejectTelemetryHoldFrames = 0u;
}

void publishRejectTelemetry() {
    if (sRejectTelemetryIndex >= sRejectTelemetryCount) {
        return;
    }
    const RejectTelemetryRecord &record =
        sRejectTelemetryRecords[sRejectTelemetryIndex];
    LMCrash::phase(sRejectTelemetryAction, record.phase, record.arg0,
                   record.arg1);
}

void serviceRejectTelemetry() {
    if (sRejectTelemetryCount == 0u) {
        return;
    }
    if (sRejectTelemetryHoldFrames > 1u) {
        --sRejectTelemetryHoldFrames;
        return;
    }
    ++sRejectTelemetryIndex;
    if (sRejectTelemetryIndex >= sRejectTelemetryCount) {
        clearRejectTelemetry();
        return;
    }
    sRejectTelemetryHoldFrames = kRejectTelemetryHoldFrames;
    publishRejectTelemetry();
}

void tracePostLoadTransitionChange() {
    // E4 identifies every changed watch word; E5 carries the old/new values
    // for the lowest set bit. This makes a no-exception door hard lock useful
    // even when it occurs before the following update can begin.
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xE4u,
                   sPostLoadTransitionChangedMask, sPostLoadTraceFrame);
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xE5u,
                   sPostLoadTransitionChangedBefore,
                   sPostLoadTransitionChangedAfter);
}

bool gateFailure(Gate gate, u32 value, bool report) {
    if (report) {
        sGate = gate;
        sGateValue = value;
    }
    return false;
}

bool gateCheck(bool ok, Gate gate, u32 value) {
    return ok || gateFailure(gate, value, true);
}

void gateReady(bool report) {
    if (report) {
        sGate = Gate::Ready;
        sGateValue = 0u;
    }
}

inline u32 readWord(u32 address) {
    return *reinterpret_cast<volatile u32 *>(address);
}

inline u16 readHalf(u32 address) {
    return *reinterpret_cast<volatile u16 *>(address);
}

inline u8 readByte(u32 address) {
    return *reinterpret_cast<volatile u8 *>(address);
}

u32 stickMagnitude(u8 raw) {
    return raw < 0x80u ? raw : 0x100u - raw;
}

bool padMovementActive() {
    return stickMagnitude(readByte(kPadStatusGlobal + kPadStickXOffset)) >
               kPadMovementDeadzone ||
           stickMagnitude(readByte(kPadStatusGlobal + kPadStickYOffset)) >
               kPadMovementDeadzone;
}

inline void writeWord(u32 address, u32 value) {
    *reinterpret_cast<volatile u32 *>(address) = value;
}

inline void writeByte(u32 address, u8 value) {
    *reinterpret_cast<volatile u8 *>(address) = value;
}

inline bool isMem1Range(u32 address, u32 size) {
    return size <= kMem1End - kMem1Start && address >= kMem1Start &&
           address <= kMem1End - size && (address & 3u) == 0;
}

inline bool isMem1ByteRange(u32 address, u32 size) {
    return size <= kMem1End - kMem1Start && address >= kMem1Start &&
           address <= kMem1End - size;
}

bool isExpHeap(u32 heap) {
    if (!isMem1Range(heap, kHeapMetadataEnd) ||
        readWord(heap) != kExpHeapVtable) {
        return false;
    }
    const u32 start = readWord(heap + 0x30u);
    const u32 end = readWord(heap + 0x34u);
    const u32 size = readWord(heap + 0x38u);
    return start >= kMem1Start && start <= end && end <= kMem1End &&
           ((start | end) & (kExpHeapAlignment - 1u)) == 0u &&
           size == end - start;
}

bool rangeInside(u32 childStart, u32 childEnd, u32 parentStart,
                 u32 parentEnd) {
    return childStart >= parentStart && childStart < childEnd &&
           childEnd <= parentEnd;
}

bool roomNameOwnerValid(u32 *fault) {
    for (u32 i = 0u; i < kRoomNameWrapperCount; ++i) {
        const u32 wrapper =
            kRoomNameOwnerStateStart + i * kRoomNameWrapperSize;
        const u32 picture = readWord(wrapper + kRoomNamePictureOffset);
        if (readWord(wrapper + kRoomNameSpareOffset) != 0u) {
            *fault = wrapper + kRoomNameSpareOffset;
            return false;
        }
        if (picture != 0u &&
            (!isMem1Range(picture, sizeof(u32)) ||
             readWord(picture) != kRoomNamePictureVtable)) {
            *fault = picture;
            return false;
        }
    }
    return true;
}

bool particleManagerValid(const LiveIdentity &identity, u32 *fault) {
    const u32 base = kParticleManagerStateStart;
    const u32 emitterPool = readWord(base);
    const u32 particlePool = readWord(base + 0xAB8u);
    const u32 childPool = readWord(base + 0xB2Cu);

    // Retail constructs these three fixed-capacity pools while the gameplay
    // heap is current. Their intrusive links must therefore be covered by the
    // same heap snapshot as the manager roots below.
    if (readWord(base + 0xB68u) != kParticleEmitterPoolCount) {
        *fault = base + 0xB68u;
        return false;
    }
    if (readWord(base + 0xB6Cu) != kParticlePoolCount) {
        *fault = base + 0xB6Cu;
        return false;
    }
    if (readWord(base + 0xB70u) != kParticleChildPoolCount) {
        *fault = base + 0xB70u;
        return false;
    }
    if (!rangeInside(emitterPool,
                     emitterPool + kParticleEmitterPoolCount *
                                       kParticleEmitterSize,
                     identity.heapStart, identity.heapEnd)) {
        *fault = base;
        return false;
    }
    if (!rangeInside(particlePool,
                     particlePool + kParticlePoolCount * kParticleSize,
                     identity.heapStart, identity.heapEnd)) {
        *fault = base + 0xAB8u;
        return false;
    }
    if (!rangeInside(childPool,
                     childPool + kParticleChildPoolCount *
                                     kParticleChildSize,
                     identity.heapStart, identity.heapEnd)) {
        *fault = base + 0xB2Cu;
        return false;
    }

    // Four fixed emitter-group sentinels, plus the particle and child free
    // sentinels, are self-relative members of this BSS object. The active
    // group may select any one of the four, but must never point elsewhere.
    const u32 groups[4] = {
        base + 0x004u, base + 0x2B0u, base + 0x55Cu, base + 0x808u,
    };
    const u32 groupRoots[4] = {0x2A4u, 0x550u, 0x7FCu, 0xAA8u};
    u32 emitterCount = 0u;
    bool activeGroupValid = false;
    const u32 activeGroup = readWord(base + 0xAB4u);
    for (u32 i = 0u; i < 4u; ++i) {
        if (readWord(base + groupRoots[i]) != groups[i]) {
            *fault = base + groupRoots[i];
            return false;
        }
        const u32 count = readWord(base + groupRoots[i] + 8u);
        if (count > kParticleEmitterPoolCount ||
            emitterCount > kParticleEmitterPoolCount - count) {
            *fault = base + groupRoots[i] + 8u;
            return false;
        }
        emitterCount += count;
        activeGroupValid = activeGroupValid || activeGroup == groups[i];
    }
    if (!activeGroupValid) {
        *fault = base + 0xAB4u;
        return false;
    }
    if (readWord(base + 0xB20u) != base + 0xABCu ||
        readWord(base + 0xB28u) > kParticlePoolCount) {
        *fault = base + 0xB20u;
        return false;
    }
    if (readWord(base + 0xB5Cu) != base + 0xB30u ||
        readWord(base + 0xB64u) > kParticleChildPoolCount) {
        *fault = base + 0xB5Cu;
        return false;
    }
    return true;
}

bool validCurrentHeap(u32 currentHeap, const LiveIdentity &identity) {
    if (currentHeap == identity.rootHeap ||
        currentHeap == identity.systemHeap || currentHeap == identity.heap) {
        return true;
    }
    if (currentHeap < identity.heapStart ||
        currentHeap > identity.heapEnd - kHeapMetadataEnd ||
        (currentHeap & 3u) != 0u || !isExpHeap(currentHeap)) {
        return false;
    }
    return rangeInside(readWord(currentHeap + 0x30u),
                       readWord(currentHeap + 0x34u), identity.heapStart,
                       identity.heapEnd);
}

bool validAudioBootstrap(u32 basic) {
    if (!isMem1Range(basic, kAudioBootstrapHandleOffset + sizeof(u32))) {
        return false;
    }
    const u32 slot = basic + kAudioBootstrapHandleOffset;
    const u32 handle = readWord(slot);
    return isMem1Range(handle, 0x34u) &&
           readWord(handle + 8u) == kAudioBootstrapSoundId &&
           readWord(handle + 0x30u) == slot;
}

bool sameIdentity(const LiveIdentity &a, const LiveIdentity &b) {
    const u32 *left = reinterpret_cast<const u32 *>(&a);
    const u32 *right = reinterpret_cast<const u32 *>(&b);
    for (u32 i = 0; i < sizeof(LiveIdentity) / sizeof(u32); ++i) {
        if (left[i] != right[i]) {
            return false;
        }
    }
    return true;
}

bool doorTransitionReady(const LiveIdentity &identity, bool report) {
    auto inside = [&](u32 address, u32 size) {
        return LmDoorRangeInside(address, size,
                                identity.heapStart, identity.heapEnd) != 0;
    };
    // Bounded equivalent of the retail player-0 lookup, without RTTI calls.
    if (!inside(identity.missionMode, 0xCu))
        return gateFailure(Gate::DoorOwner, identity.missionMode, report);
    const u32 manager = readWord(identity.missionMode + 8u);
    if (!inside(manager, 0xE0Cu))
        return gateFailure(Gate::DoorOwner, manager, report);
    const u32 index = readWord(manager + 0xE08u);
    const u32 count = readWord(kRoomActorCountGlobal);
    if (count > kRoomActorCapacity || index >= count ||
        index >= kRoomActorCapacity)
        return gateFailure(Gate::DoorOwner, index, report);
    const u32 player = readWord(kRoomActorTableStart + index * sizeof(u32));
    if (!inside(player, LM_DOOR_CONTROLLER_OFFSET + sizeof(u32)) ||
        readWord(player) != kPlayerVtable)
        return gateFailure(Gate::DoorOwner, player, report);
    const u32 controller = readWord(player + LM_DOOR_CONTROLLER_OFFSET);
    if (!inside(controller, 0x320u))
        return gateFailure(Gate::DoorOwner, controller, report);
    if (LmDoorStateBusy(readWord(controller + LM_DOOR_MODE_OFFSET),
                        readWord(controller + LM_DOOR_STATE_OFFSET)))
        return gateFailure(Gate::Door, readWord(controller + 0x31Cu), report);
    return true;
}

bool buildIdentity(LiveIdentity *identity, bool report = false) {
    identity->heap = readWord(kGameHeapGlobal);
    identity->rootHeap = readWord(kRootHeapGlobal);
    identity->systemHeap = readWord(kSystemHeapGlobal);
    if (!isExpHeap(identity->heap)) {
        identity->heapStart = 0;
        identity->heapEnd = 0;
        identity->heapSize = 0;
        return gateFailure(Gate::GameHeap, identity->heap, report);
    }
    if (!isExpHeap(identity->rootHeap)) {
        return gateFailure(Gate::RootHeap, identity->rootHeap, report);
    }
    if (!isExpHeap(identity->systemHeap)) {
        return gateFailure(Gate::SystemHeap, identity->systemHeap, report);
    }
    identity->heapStart = readWord(identity->heap + 0x30u);
    identity->heapEnd = readWord(identity->heap + 0x34u);
    identity->heapSize = readWord(identity->heap + 0x38u);
    identity->heapMode = readByte(identity->heap + kHeapModeOffset);
    identity->heapGroup = readByte(identity->heap + kHeapGroupOffset);
    identity->heapFreeHead = readWord(identity->heap + 0x74u);
    identity->heapFreeTail = readWord(identity->heap + 0x78u);
    identity->heapUsedHead = readWord(identity->heap + 0x7Cu);
    identity->heapUsedTail = readWord(identity->heap + 0x80u);
    identity->rootHeapStart = readWord(identity->rootHeap + 0x30u);
    identity->rootHeapEnd = readWord(identity->rootHeap + 0x34u);
    identity->rootHeapSize = readWord(identity->rootHeap + 0x38u);
    identity->rootHeapMode = readByte(identity->rootHeap + kHeapModeOffset);
    identity->rootHeapGroup = readByte(identity->rootHeap + kHeapGroupOffset);
    identity->rootFreeHead = readWord(identity->rootHeap + 0x74u);
    identity->rootFreeTail = readWord(identity->rootHeap + 0x78u);
    identity->rootUsedHead = readWord(identity->rootHeap + 0x7Cu);
    identity->rootUsedTail = readWord(identity->rootHeap + 0x80u);
    identity->systemHeapStart = readWord(identity->systemHeap + 0x30u);
    identity->systemHeapEnd = readWord(identity->systemHeap + 0x34u);
    identity->systemHeapSize = readWord(identity->systemHeap + 0x38u);
    identity->systemHeapMode =
        readByte(identity->systemHeap + kHeapModeOffset);
    identity->systemHeapGroup =
        readByte(identity->systemHeap + kHeapGroupOffset);
    identity->systemFreeHead = readWord(identity->systemHeap + 0x74u);
    identity->systemFreeTail = readWord(identity->systemHeap + 0x78u);
    identity->systemUsedHead = readWord(identity->systemHeap + 0x7Cu);
    identity->systemUsedTail = readWord(identity->systemHeap + 0x80u);
    identity->currentHeap = readWord(kCurrentHeapGlobal);
    identity->missionMode = readWord(kMissionModeGlobal);
    identity->mapArchive =
        isMem1Range(identity->missionMode, 0x1Cu)
            ? readWord(identity->missionMode + 0x18u)
            : 0u;
    identity->volume[0] = readWord(kVolumeListGlobal);
    identity->volume[1] = readWord(kVolumeListGlobal + 4u);
    identity->volume[2] = readWord(kVolumeListGlobal + 8u);
    identity->mapValue = readWord(kMapValueGlobal);
    identity->sceneValue = readWord(kSceneValueGlobal);
    identity->currentScene = readWord(kCurrentSceneGlobal);
    identity->gameMode = readWord(kGameModeGlobal);
    identity->gameModeCount = readWord(kGameModeCountGlobal);
    identity->simpleModeler = readWord(kSimpleModelerGlobal);
    identity->mapCol = readWord(kMapColGlobal);
    identity->enTypesManager = readWord(kEnTypesManagerGlobal);
    identity->currentHeapGroup = readByte(kCurrentHeapGroupGlobal);
    identity->audioBasic = readWord(kAudioObjectGlobal);
    identity->audioScene = 0u;
    identity->mainLoopMode = readWord(kMainLoopModeGlobal);
    identity->mainLoopPendingScene =
        readWord(kMainLoopPendingSceneGlobal);
    identity->mainLoopScene = readWord(kMainLoopSceneGlobal);
    identity->mainDrawState = readWord(kMainDrawStateGlobal);
    identity->mainLoopExit = readWord(kMainLoopExitGlobal);

    const bool distinctHeaps = identity->rootHeap != identity->systemHeap &&
        identity->rootHeap != identity->heap &&
        identity->systemHeap != identity->heap;
    if (identity->heapSize > kSnapshotCapacity - kHeapDataOffset) {
        return gateFailure(Gate::Size, identity->heapSize, report);
    }
    if (!distinctHeaps) {
        return gateFailure(Gate::Distinct, identity->heap, report);
    }
    if (!rangeInside(identity->systemHeapStart, identity->systemHeapEnd,
                     identity->rootHeapStart, identity->rootHeapEnd)) {
        return gateFailure(Gate::SystemNest, identity->systemHeapStart,
                           report);
    }
    if (!rangeInside(identity->heapStart, identity->heapEnd,
                     identity->rootHeapStart, identity->rootHeapEnd)) {
        return gateFailure(Gate::GameNest, identity->heapStart, report);
    }
    u32 particleFault = 0u;
    if (!particleManagerValid(*identity, &particleFault)) {
        return gateFailure(Gate::Particle, particleFault, report);
    }
    u32 roomNameFault = 0u;
    if (!roomNameOwnerValid(&roomNameFault)) {
        return gateFailure(Gate::RoomName, roomNameFault, report);
    }
    if (identity->systemHeapEnd > identity->heapStart &&
        identity->heapEnd > identity->systemHeapStart) {
        return gateFailure(Gate::Overlap, identity->systemHeapEnd, report);
    }
    if (!validCurrentHeap(identity->currentHeap, *identity)) {
        return gateFailure(Gate::CurrentHeap, identity->currentHeap, report);
    }
    if (identity->missionMode == 0u) {
        return gateFailure(Gate::MissionNull, 0u, report);
    }
    if (identity->missionMode < identity->heapStart ||
        identity->missionMode > identity->heapEnd - 0x1Cu) {
        return gateFailure(Gate::MissionRange, identity->missionMode, report);
    }
    if (identity->gameMode != identity->missionMode) {
        return gateFailure(Gate::ModeMismatch, identity->gameMode, report);
    }
    if (identity->gameModeCount != 1u) {
        return gateFailure(Gate::ModeCount, identity->gameModeCount, report);
    }
    for (u32 i = 0;
         i < sizeof(kGameStaticRootGlobals) /
                 sizeof(kGameStaticRootGlobals[0]);
         ++i) {
        const u32 root = readWord(kGameStaticRootGlobals[i]);
        if (root != 0u &&
            (root < identity->heapStart || root >= identity->heapEnd ||
             (root & 3u) != 0u)) {
            return gateFailure(Gate::GameRoot, root, report);
        }
    }
    const u32 roomActorCount = readWord(kRoomActorCountGlobal);
    if (roomActorCount > kRoomActorCapacity) {
        return gateFailure(Gate::GameRoot, roomActorCount, report);
    }
    for (u32 i = 0u; i < roomActorCount; ++i) {
        const u32 actor = readWord(kRoomActorTableStart + i * sizeof(u32));
        if (actor < identity->heapStart || actor >= identity->heapEnd ||
            (actor & 3u) != 0u) {
            return gateFailure(Gate::GameRoot, actor, report);
        }
    }
    if (!isMem1Range(identity->currentScene, sizeof(u32))) {
        return gateFailure(Gate::Scene, identity->currentScene, report);
    }
    if (!doorTransitionReady(*identity, report)) return false;
    if (identity->mainLoopMode != 2u) {
        return gateFailure(Gate::LoopMode, identity->mainLoopMode, report);
    }
    if (identity->mainLoopExit != 0u) {
        return gateFailure(Gate::LoopExit, identity->mainLoopExit, report);
    }
    if (identity->mainLoopPendingScene != identity->mainLoopScene) {
        return gateFailure(Gate::LoopScene,
                           identity->mainLoopPendingScene, report);
    }
    if (identity->mainDrawState > 7u) {
        return gateFailure(Gate::DrawState, identity->mainDrawState, report);
    }
    if (identity->audioBasic != kAudioStaticObject ||
        readWord(kAudioBasicGlobal) != identity->audioBasic ||
        !isMem1Range(identity->audioBasic, kAudioSceneOffset + sizeof(u32)) ||
        readWord(identity->audioBasic + 8u) != kAudioVtable ||
        !validAudioBootstrap(identity->audioBasic)) {
        const u32 handle =
            isMem1Range(identity->audioBasic,
                        kAudioBootstrapHandleOffset + sizeof(u32))
                ? readWord(identity->audioBasic +
                           kAudioBootstrapHandleOffset)
                : identity->audioBasic;
        return gateFailure(Gate::Audio, handle, report);
    }
    identity->audioScene =
        readWord(identity->audioBasic + kAudioSceneOffset);
    gateReady(report);
    return true;
}

bool primaryDvdWorkerIdle(u32 *fault) {
    const u32 current = readWord(kDvdCurrentGlobal + 8u);
    const u32 arrayEnd =
        kDvdFileInfoArray + kDvdFileInfoCount * kDvdFileInfoSize;
    if (readWord(kDvdCurrentGlobal + 4u) != 1u) {
        *fault = kDvdCurrentGlobal + 4u;
        return false;
    }
    if (current < kDvdFileInfoArray || current >= arrayEnd ||
        (current - kDvdFileInfoArray) % kDvdFileInfoSize != 0u) {
        *fault = kDvdCurrentGlobal + 8u;
        return false;
    }
    if (readWord(kDvdPrimaryCursor) != current) {
        *fault = kDvdPrimaryCursor;
        return false;
    }
    if (readHalf(kDvdPrimaryThreadState) != kOsThreadWaiting) {
        *fault = kDvdPrimaryThreadState;
        return false;
    }
    if (readWord(kDvdPrimaryThreadSuspend) != 0u) {
        *fault = kDvdPrimaryThreadSuspend;
        return false;
    }
    if (readWord(kDvdPrimaryThreadQueue) != kDvdPrimaryQueue + 8u) {
        *fault = kDvdPrimaryThreadQueue;
        return false;
    }
    if (readWord(kDvdPrimaryQueue) != 0u ||
        readWord(kDvdPrimaryQueue + 4u) != 0u ||
        readWord(kDvdPrimaryQueue + 8u) != kDvdPrimaryThread ||
        readWord(kDvdPrimaryQueue + 0x0Cu) != kDvdPrimaryThread ||
        readWord(kDvdPrimaryQueue + 0x10u) != kDvdPrimaryMessages ||
        readWord(kDvdPrimaryQueue + 0x14u) != kDvdFileInfoCount ||
        readWord(kDvdPrimaryQueue + 0x18u) >= kDvdFileInfoCount ||
        readWord(kDvdPrimaryQueue + 0x1Cu) != 0u) {
        *fault = kDvdPrimaryQueue;
        return false;
    }
    return true;
}

bool secondaryDvdWorkerIdle(u32 *fault) {
    if (readHalf(kDvdSecondaryThreadState) != kOsThreadWaiting) {
        *fault = kDvdSecondaryThreadState;
        return false;
    }
    if (readWord(kDvdSecondaryThreadSuspend) != 0u) {
        *fault = kDvdSecondaryThreadSuspend;
        return false;
    }
    if (readWord(kDvdSecondaryThreadQueue) !=
        kDvdSecondaryRequestQueue + 8u) {
        *fault = kDvdSecondaryThreadQueue;
        return false;
    }
    if (readWord(kDvdSecondaryRequestQueue) != 0u ||
        readWord(kDvdSecondaryRequestQueue + 4u) != 0u ||
        readWord(kDvdSecondaryRequestQueue + 8u) != kDvdSecondaryThread ||
        readWord(kDvdSecondaryRequestQueue + 0x0Cu) !=
            kDvdSecondaryThread ||
        readWord(kDvdSecondaryRequestQueue + 0x10u) !=
            kDvdSecondaryRequestMessage ||
        readWord(kDvdSecondaryRequestQueue + 0x14u) != 1u ||
        readWord(kDvdSecondaryRequestQueue + 0x18u) != 0u ||
        readWord(kDvdSecondaryRequestQueue + 0x1Cu) != 0u) {
        *fault = kDvdSecondaryRequestQueue;
        return false;
    }
    if (readWord(kDvdSecondaryCompletionQueue) != 0u ||
        readWord(kDvdSecondaryCompletionQueue + 4u) != 0u ||
        readWord(kDvdSecondaryCompletionQueue + 8u) != 0u ||
        readWord(kDvdSecondaryCompletionQueue + 0x0Cu) != 0u ||
        readWord(kDvdSecondaryCompletionQueue + 0x10u) !=
            kDvdSecondaryCompletionMessage ||
        readWord(kDvdSecondaryCompletionQueue + 0x14u) != 1u ||
        readWord(kDvdSecondaryCompletionQueue + 0x18u) != 0u ||
        readWord(kDvdSecondaryCompletionQueue + 0x1Cu) != 0u) {
        *fault = kDvdSecondaryCompletionQueue;
        return false;
    }
    return true;
}

bool ioIdle(bool report = false) {
    const bool predicateBusy =
        reinterpret_cast<BoolFn>(kDvdBusyPredicateAddr)();
    const u32 dvdOutstanding = readWord(kDvdOutstandingGlobal);
    const u32 aram0 = readWord(kAramList0Global + 8u);
    const u32 aram1 = readWord(kAramList1Global + 8u);
    const u32 card0 = readWord(kCardBlockGlobal + kCardResultOffset);
    const u32 card1 = readWord(kCardBlockGlobal + kCardControlStride +
                               kCardResultOffset);
    if (predicateBusy) {
        return gateFailure(Gate::DvdPredicate, dvdOutstanding, report);
    }
    if (dvdOutstanding != 0u) {
        return gateFailure(Gate::DvdCount, dvdOutstanding, report);
    }
    u32 dvdFault = 0u;
    if (!primaryDvdWorkerIdle(&dvdFault)) {
        return gateFailure(Gate::DvdPrimary, dvdFault, report);
    }
    if (!secondaryDvdWorkerIdle(&dvdFault)) {
        return gateFailure(Gate::DvdSecondary, dvdFault, report);
    }
    if (aram0 != 0u) {
        return gateFailure(Gate::Aram0, aram0, report);
    }
    if (aram1 != 0u) {
        return gateFailure(Gate::Aram1, aram1, report);
    }
    if (card0 == kCardResultBusy) {
        return gateFailure(Gate::Card0, card0, report);
    }
    if (card1 == kCardResultBusy) {
        return gateFailure(Gate::Card1, card1, report);
    }
    gateReady(report);
    return true;
}

bool heapsHealthy(const LiveIdentity &identity) {
    return isExpHeap(identity.rootHeap) && isExpHeap(identity.systemHeap) &&
           isExpHeap(identity.heap) &&
           LMState::heapHealthy(identity.rootHeap) &&
           LMState::heapHealthy(identity.systemHeap) &&
           LMState::heapHealthy(identity.heap);
}

void copyWords(void *destination, const void *source, u32 size) {
    volatile u32 *out = reinterpret_cast<volatile u32 *>(destination);
    const volatile u32 *in = reinterpret_cast<const volatile u32 *>(source);
    for (u32 i = 0; i < size / sizeof(u32); ++i) {
        out[i] = in[i];
    }
}

void copyBytes(void *destination, const void *source, u32 size) {
    volatile u8 *out = reinterpret_cast<volatile u8 *>(destination);
    const volatile u8 *in = reinterpret_cast<const volatile u8 *>(source);
    for (u32 i = 0; i < size; ++i) {
        out[i] = in[i];
    }
}

void canonicalizeDvdTransport() {
    // Every callback has completed and both workers are sleeping before the
    // scheduler is frozen.  Keep their OS queues/threads live, but discard
    // completed-request payload pointers into the destination room's erased
    // heap and restart the neutral 64-entry ring coherently.
    for (u32 i = 0u; i < kDvdFileInfoCount; ++i) {
        const u32 info = kDvdFileInfoArray + i * kDvdFileInfoSize;
        const u32 next = kDvdFileInfoArray +
            ((i + 1u) % kDvdFileInfoCount) * kDvdFileInfoSize;
        writeWord(info + 0x60u, 0u);
        writeWord(info + 0x64u, 0u);
        writeWord(info + 0x68u, 0u);
        writeWord(info + 0x6Cu, 0u);
        writeWord(info + 0x70u, 0u);
        writeWord(info + 0x74u, 0u);
        writeWord(info + 0x78u, 0u);
        writeWord(info + 0x7Cu, next);
        writeByte(info + 0x80u, 0u);
        writeWord(info + 0x84u, 0u);
    }
    writeWord(kDvdCurrentGlobal, 0u);
    writeWord(kDvdCurrentGlobal + 4u, 1u);
    writeWord(kDvdCurrentGlobal + 8u, kDvdFileInfoArray);
    writeWord(kDvdPrimaryCursor, kDvdFileInfoArray);

    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(kDvdFileInfoArray),
        kDvdFileInfoCount * kDvdFileInfoSize);
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(kDvdCurrentGlobal), 3u * sizeof(u32));
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(kDvdPrimaryCursor), sizeof(u32));
}

void samplePostLoadTransitionWatch(PostLoadTransitionWatch *watch) {
    watch->mapValue = readWord(kMapValueGlobal);
    watch->sceneValue = readWord(kSceneValueGlobal);
    watch->pendingScene = readWord(kMainLoopPendingSceneGlobal);
    watch->loopExit = readWord(kMainLoopExitGlobal);
    watch->dvdOutstanding = readWord(kDvdOutstandingGlobal);
    watch->aram0 = readWord(kAramList0Global + 8u);
    watch->aram1 = readWord(kAramList1Global + 8u);
    watch->resourceWantedCount = readWord(kResourceWantedCountGlobal);
    watch->volumeTail = readWord(kVolumeListGlobal + 4u);
    watch->volumeCount = readWord(kVolumeListGlobal + 8u);
}

bool refreshPostLoadTransitionWatch() {
    PostLoadTransitionWatch live;
    samplePostLoadTransitionWatch(&live);
    const u32 *const before =
        reinterpret_cast<const u32 *>(&sPostLoadTransitionWatch);
    const u32 *const after = reinterpret_cast<const u32 *>(&live);
    u32 changedMask = 0u;
    u32 firstBefore = 0u;
    u32 firstAfter = 0u;
    for (u32 i = 0u; i < sizeof(live) / sizeof(u32); ++i) {
        if (before[i] != after[i]) {
            if (changedMask == 0u) {
                firstBefore = before[i];
                firstAfter = after[i];
            }
            changedMask |= 1u << i;
        }
    }
    sPostLoadTransitionChangedMask = changedMask;
    sPostLoadTransitionChangedBefore = firstBefore;
    sPostLoadTransitionChangedAfter = firstAfter;
    copyWords(&sPostLoadTransitionWatch, &live, sizeof(live));
    return changedMask != 0u;
}

u32 cameraObjectRecordAddress(u32 index) {
    return kSnapshotBase + kCameraObjectStateOffset +
           index * kCameraObjectRecordSize;
}

int grainReadWord(void *context, unsigned int address, unsigned int *value);

bool cameraObjectsValid(const LiveIdentity &identity,
                        bool matchSnapshot) {
    unsigned int liveTargets[kCameraObjectCount], savedTargets[kCameraObjectCount];
    bool replaced = false;
    for (u32 i = 0; i < kCameraObjectCount; ++i) {
        const u32 target =
            readWord(kCameraObjectPointerTable + i * sizeof(u32));
        liveTargets[i] = target;
        if (!isMem1ByteRange(target, kCameraObjectSize) ||
            !rangeInside(target, target + kCameraObjectSize,
                         identity.rootHeapStart, identity.rootHeapEnd)) {
            return false;
        }
        for (u32 j = 0; j < i; ++j) {
            if (target ==
                readWord(kCameraObjectPointerTable + j * sizeof(u32))) {
                return false;
            }
        }
        if (!matchSnapshot) {
            continue;
        }
        const u32 record = cameraObjectRecordAddress(i);
        const bool inGameHeap =
            rangeInside(target, target + kCameraObjectSize,
                        identity.heapStart, identity.heapEnd);
        const u32 capturedSize = readWord(record + sizeof(u32));
        savedTargets[i] = readWord(record);
        if (capturedSize != (inGameHeap ? 0u : kCameraObjectSize)) {
            return false;
        }
        replaced |= savedTargets[i] != target;
    }
    if (!matchSnapshot || !replaced) return true;
    const SnapshotHeader *header = reinterpret_cast<const SnapshotHeader *>(kSnapshotBase);
    // Only complete GAME-owned endpoints may change address. Their blocks and
    // manager roots rewind together; retained camera sidecars still stay exact.
    for (u32 i = 0u; i < kCameraObjectCount; ++i) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xF4u + i,
                       savedTargets[i], liveTargets[i]);
        if (readWord(cameraObjectRecordAddress(i) + 4u) != 0u) return false;
    }
    unsigned int fault = 0u, value = 0u;
    if (!LmCameraGameValidate(const_cast<SnapshotHeader *>(header), grainReadWord,
            savedTargets, header->heapStart, header->heapEnd,
            header->usedHead, header->usedTail, &fault, &value)) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xF7u, fault, value);
        return false;
    }
    if (!LmCameraGameValidate(nullptr, grainReadWord, liveTargets,
            identity.heapStart, identity.heapEnd, identity.heapUsedHead,
            identity.heapUsedTail, &fault, &value)) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xF8u, fault, value);
        return false;
    }
    return true;
}

void captureCameraObjects(const LiveIdentity &identity) {
    for (u32 i = 0; i < kCameraObjectCount; ++i) {
        const u32 target =
            readWord(kCameraObjectPointerTable + i * sizeof(u32));
        const u32 record = cameraObjectRecordAddress(i);
        const bool inGameHeap =
            rangeInside(target, target + kCameraObjectSize,
                        identity.heapStart, identity.heapEnd);
        writeWord(record, target);
        writeWord(record + sizeof(u32),
                  inGameHeap ? 0u : kCameraObjectSize);
        if (!inGameHeap) {
            copyBytes(reinterpret_cast<void *>(record + 2u * sizeof(u32)),
                      reinterpret_cast<void *>(target), kCameraObjectSize);
        }
    }
}

void restoreCameraObjects() {
    for (u32 i = 0; i < kCameraObjectCount; ++i) {
        const u32 record = cameraObjectRecordAddress(i);
        if (readWord(record + sizeof(u32)) == kCameraObjectSize) {
            copyBytes(reinterpret_cast<void *>(readWord(record)),
                      reinterpret_cast<void *>(record + 2u * sizeof(u32)),
                      kCameraObjectSize);
        }
    }
}

void storeCameraObjects() {
    for (u32 i = 0; i < kCameraObjectCount; ++i) {
        const u32 record = cameraObjectRecordAddress(i);
        if (readWord(record + sizeof(u32)) == kCameraObjectSize) {
            reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
                reinterpret_cast<void *>(readWord(record)),
                kCameraObjectSize);
        }
    }
}

void captureStaticRanges() {
    u32 offset = kStateStaticsOffset;
    for (u32 i = 0; i < kStateStaticRangeCount; ++i) {
        const StaticRange &range = kStateStaticRanges[i];
        copyBytes(reinterpret_cast<void *>(kSnapshotBase + offset),
                  reinterpret_cast<void *>(range.address), range.size);
        offset += range.size;
    }
}

int grainReadWord(void *context, unsigned int address, unsigned int *value) {
    if (!isMem1Range(address, sizeof(u32))) return 0;
    const SnapshotHeader *header = static_cast<const SnapshotHeader *>(context);
    if (!header) {
        *value = readWord(address);
        return 1;
    }
    u32 offset = 0u;
    if (rangeInside(address, address + sizeof(u32),
                    header->heapStart, header->heapEnd)) {
        offset = kHeapDataOffset + address - header->heapStart;
    } else {
        u32 packed = kStateStaticsOffset;
        bool found = false;
        for (u32 i = 0u; i < kStateStaticRangeCount; ++i) {
            const StaticRange &range = kStateStaticRanges[i];
            if (rangeInside(address, address + sizeof(u32),
                            range.address, range.address + range.size)) {
                offset = packed + address - range.address;
                found = true;
                break;
            }
            packed += range.size;
        }
        if (!found) return 0;
    }
    if (offset > header->totalSize ||
        sizeof(u32) > header->totalSize - offset) return 0;
    *value = readWord(kSnapshotBase + offset);
    return 1;
}

bool grainStateValid(const SnapshotHeader *header, const LiveIdentity &live,
                     u32 action, u32 phase) {
    unsigned int fault = 0u, value = 0u;
    const bool valid = LmGrainValidate(
        const_cast<SnapshotHeader *>(header), grainReadWord,
        header ? header->heapStart : live.heapStart,
        header ? header->heapEnd : live.heapEnd, &fault, &value) != 0;
    LMCrash::note(action == SUSAMUNE_PHASE_ACTION_SAVE ? kEventStateSavePhase :
                       kEventStateLoadPhase, phase, fault);
    LMCrash::phase(action, phase, fault, value);
    if (!valid) gateFailure(Gate::Grain, fault, true);
    return valid;
}

void restoreStaticRanges() {
    u32 offset = kStateStaticsOffset;
    for (u32 i = 0; i < kStateStaticRangeCount; ++i) {
        const StaticRange &range = kStateStaticRanges[i];
        copyBytes(reinterpret_cast<void *>(range.address),
                  reinterpret_cast<void *>(kSnapshotBase + offset),
                  range.size);
        offset += range.size;
    }
}

void storeStaticRanges() {
    for (u32 i = 0; i < kStateStaticRangeCount; ++i) {
        const StaticRange &range = kStateStaticRanges[i];
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            reinterpret_cast<void *>(range.address), range.size);
    }
}

void clearWords(void *destination, u32 size) {
    volatile u32 *out = reinterpret_cast<volatile u32 *>(destination);
    for (u32 i = 0; i < size / sizeof(u32); ++i) {
        out[i] = 0;
    }
}

u8 lowerAscii(u8 value) {
    return value >= 'A' && value <= 'Z'
               ? static_cast<u8>(value + ('a' - 'A'))
               : value;
}

u32 hashVolumeWord(u32 hash, u32 value) {
    hash ^= value;
    return hash * 16777619u;
}

u32 classifyVolumeHeap(u32 heap, const LiveIdentity &identity) {
    if (heap == identity.heap) return kVolumeOwnerGame;
    if (heap == identity.systemHeap) return kVolumeOwnerSystem;
    if (heap == identity.rootHeap) return kVolumeOwnerRoot;
    return kVolumeOwnerOther;
}

u32 classifyVolumeRange(u32 address, u32 size,
                        const LiveIdentity &identity) {
    if (size == 0u || !isMem1ByteRange(address, size)) {
        return kVolumeOwnerOther;
    }
    const u32 end = address + size;
    if (address >= identity.heapStart && end <= identity.heapEnd) {
        return kVolumeOwnerGame;
    }
    if (address >= identity.systemHeapStart && end <= identity.systemHeapEnd) {
        return kVolumeOwnerSystem;
    }
    if (address >= identity.rootHeapStart && end <= identity.rootHeapEnd) {
        return kVolumeOwnerRoot;
    }
    return kVolumeOwnerOther;
}

void captureVolumeName(VolumeDescriptor *entry) {
    entry->name[0] = '?';
    entry->name[1] = '\0';
    if (!isMem1ByteRange(entry->namePointer, 1u)) {
        return;
    }

    u32 hash = 2166136261u;
    for (u32 i = 0; i < kVolumeNameHashBytes; ++i) {
        if (!isMem1ByteRange(entry->namePointer + i, 1u)) {
            return;
        }
        const u8 value = readByte(entry->namePointer + i);
        if (value == 0u) {
            entry->nameHash = hash;
            entry->stateFlags |= kVolumeNameValid;
            if (i == 0u) {
                entry->name[0] = '/';
                entry->name[1] = '\0';
            }
            return;
        }
        hash ^= lowerAscii(value);
        hash *= 16777619u;
        if (i + 1u < kVolumeNameBytes) {
            entry->name[i] = value >= 0x20u && value <= 0x7Eu
                                 ? static_cast<char>(value)
                                 : '.';
            entry->name[i + 1u] = '\0';
        }
    }
}

void failVolumeCensus(VolumeCensus *census, u32 fault) {
    census->fault = fault;
    census->valid = 0u;
}

bool captureVolumeCensus(VolumeCensus *census,
                         const LiveIdentity &identity) {
    clearWords(census, sizeof(*census));
    census->count = identity.volume[2];
    census->head = identity.volume[0];
    census->tail = identity.volume[1];
    census->currentVolume = readWord(kCurrentVolumeGlobal);
    census->currentDirId = readWord(kCurrentDirIdGlobal);
    census->stableFrames = sStableFrames;

    if (census->count > kMaxVolumes) {
        failVolumeCensus(census, kVolumeFaultCapacity);
        return false;
    }
    if (census->count == 0u) {
        if (census->head != 0u || census->tail != 0u) {
            failVolumeCensus(census, kVolumeFaultEmpty);
            return false;
        }
        census->signature = 2166136261u;
        census->valid = 1u;
        return true;
    }
    if (!isMem1Range(census->head, 0x10u) ||
        !isMem1Range(census->tail, 0x10u)) {
        failVolumeCensus(census, kVolumeFaultEndpoint);
        return false;
    }

    u32 node = census->head;
    u32 previous = 0u;
    u32 signature = 2166136261u;
    for (u32 i = 0; i < census->count; ++i) {
        if (!isMem1Range(node, 0x10u)) {
            failVolumeCensus(census, kVolumeFaultNode);
            return false;
        }
        for (u32 prior = 0; prior < i; ++prior) {
            if (census->entries[prior].node == node) {
                failVolumeCensus(census, kVolumeFaultDuplicate);
                return false;
            }
        }

        VolumeDescriptor &entry = census->entries[i];
        entry.node = node;
        entry.object = readWord(node);
        entry.previous = readWord(node + 8u);
        entry.next = readWord(node + 0xCu);
        if (readWord(node + 4u) != kVolumeListGlobal) {
            failVolumeCensus(census, kVolumeFaultList);
            return false;
        }
        if (!isMem1Range(entry.object, 0x68u)) {
            failVolumeCensus(census, kVolumeFaultObject);
            return false;
        }
        if (entry.object + 0x18u != node ||
            readWord(entry.object + 0x18u) != entry.object) {
            failVolumeCensus(census, kVolumeFaultEmbeddedLink);
            return false;
        }
        if (entry.previous != previous) {
            failVolumeCensus(census, kVolumeFaultPrevious);
            return false;
        }
        for (u32 prior = 0; prior < i; ++prior) {
            if (census->entries[prior].object == entry.object) {
                failVolumeCensus(census, kVolumeFaultDuplicate);
                return false;
            }
        }

        entry.vtable = readWord(entry.object);
        entry.objectOwnerHeap = readWord(entry.object + 4u);
        entry.namePointer = readWord(entry.object + 0x28u);
        entry.type = readWord(entry.object + 0x2Cu);
        entry.mountCount = readWord(entry.object + 0x34u);
        if (readByte(entry.object + 0x30u) != 0u) {
            entry.stateFlags |= kVolumeMounted;
        }
        captureVolumeName(&entry);

        if (entry.vtable == kMemArchiveVtable) {
            entry.archiveHeap = readWord(entry.object + 0x38u);
            entry.mountSource = readWord(entry.object + 0x40u);
            entry.archiveInfo = readWord(entry.object + 0x44u);
            entry.archiveHeader = readWord(entry.object + 0x5Cu);
            entry.archiveData = readWord(entry.object + 0x60u);
            entry.stateFlags |= kVolumeArchiveValid;
            entry.stateFlags |=
                (static_cast<u32>(readByte(entry.object + 0x3Cu)) & 0xFu)
                << kVolumeModeShift;
            entry.stateFlags |=
                (readWord(entry.object + 0x58u) & 0xFu)
                << kVolumeDirectionShift;
            if (readByte(entry.object + 0x64u) != 0u) {
                entry.stateFlags |= kVolumeOpen;
            }
            if (isMem1Range(entry.archiveHeader, 0x20u) &&
                readWord(entry.archiveHeader) == kRarcMagic) {
                const u32 fileLength = readWord(entry.archiveHeader + 4u);
                const u32 dataLength = readWord(entry.archiveHeader + 0x10u);
                if (fileLength >= 0x20u && dataLength <= fileLength &&
                    isMem1ByteRange(entry.archiveHeader, fileLength)) {
                    entry.fileLength = fileLength;
                    entry.dataLength = dataLength;
                    entry.stateFlags |= kVolumeRarcValid;
                    u32 content = 2166136261u;
                    for (u32 offset = 0u; offset < 0x20u; offset += 4u) {
                        content = hashVolumeWord(
                            content, readWord(entry.archiveHeader + offset));
                    }
                    const u32 backingEnd = entry.archiveHeader + fileLength;
                    if (entry.archiveInfo >= entry.archiveHeader &&
                        entry.archiveInfo <= backingEnd - 0x20u &&
                        isMem1Range(entry.archiveInfo, 0x20u)) {
                        for (u32 offset = 0u; offset < 0x20u; offset += 4u) {
                            content = hashVolumeWord(
                                content,
                                readWord(entry.archiveInfo + offset));
                        }
                    }
                    entry.contentSignature = content;
                }
            }
        }

        const u32 backingSize =
            (entry.stateFlags & kVolumeRarcValid) != 0u
                ? entry.fileLength
                : (entry.archiveHeader != 0u ? sizeof(u32) : 0u);
        entry.ownerFlags =
            classifyVolumeHeap(entry.objectOwnerHeap, identity)
                << kVolumeObjectOwnerShift;
        entry.ownerFlags |=
            classifyVolumeHeap(entry.archiveHeap, identity)
            << kVolumeArchiveOwnerShift;
        entry.ownerFlags |= classifyVolumeRange(entry.object, 0x68u, identity)
                            << kVolumeObjectLocationShift;
        entry.ownerFlags |=
            classifyVolumeRange(entry.archiveHeader, backingSize, identity)
            << kVolumeBackingLocationShift;

        signature = hashVolumeWord(signature, entry.object);
        signature = hashVolumeWord(signature, entry.nameHash);
        signature = hashVolumeWord(signature, entry.type);
        previous = node;
        node = entry.next;
    }

    if (previous != census->tail) {
        failVolumeCensus(census, kVolumeFaultTail);
        return false;
    }
    if (node != 0u) {
        failVolumeCensus(census, kVolumeFaultEnd);
        return false;
    }
    if (readWord(kVolumeListGlobal) != census->head ||
        readWord(kVolumeListGlobal + 4u) != census->tail ||
        readWord(kVolumeListGlobal + 8u) != census->count) {
        failVolumeCensus(census, kVolumeFaultChanged);
        return false;
    }
    census->signature = signature;
    census->valid = 1u;
    return true;
}

bool sameVolumeDescriptor(const VolumeDescriptor &saved,
                          const VolumeDescriptor &live) {
    return saved.node == live.node && saved.object == live.object &&
           saved.vtable == live.vtable && saved.nameHash == live.nameHash &&
           saved.type == live.type &&
           saved.archiveHeader == live.archiveHeader &&
           saved.fileLength == live.fileLength &&
           saved.dataLength == live.dataLength &&
           saved.contentSignature == live.contentSignature;
}

s32 findVolume(const VolumeCensus &census,
               const VolumeDescriptor &entry) {
    for (u32 i = 0; i < census.count; ++i) {
        if (sameVolumeDescriptor(entry, census.entries[i])) {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

void clearVolumeDiff() {
    clearWords(&sVolumeDiff, sizeof(sVolumeDiff));
    for (u32 i = 0; i < kVolumeRemovedSlots; ++i) {
        sVolumeDiff.removedIndices[i] = 0xFFFFFFFFu;
    }
    for (u32 i = 0; i < kVolumeAddedSlots; ++i) {
        sVolumeDiff.addedIndices[i] = 0xFFFFFFFFu;
    }
}

void diffVolumeCensus(const VolumeCensus &saved,
                      const VolumeCensus &live) {
    clearVolumeDiff();
    sVolumeDiff.ready = 1u;
    sVolumeDiff.savedValid = saved.valid;
    sVolumeDiff.liveValid = live.valid;
    sVolumeDiff.savedCount = saved.count;
    sVolumeDiff.liveCount = live.count;
    sVolumeDiff.currentChanged =
        saved.currentVolume != live.currentVolume ||
                saved.currentDirId != live.currentDirId
            ? 1u
            : 0u;
    if (!saved.valid || !live.valid) {
        return;
    }

    for (u32 i = 0; i < saved.count; ++i) {
        if (findVolume(live, saved.entries[i]) < 0) {
            if (sVolumeDiff.removedCount < kVolumeRemovedSlots) {
                sVolumeDiff.removedIndices[sVolumeDiff.removedCount] = i;
            }
            ++sVolumeDiff.removedCount;
        }
    }
    for (u32 i = 0; i < live.count; ++i) {
        if (findVolume(saved, live.entries[i]) < 0) {
            if (sVolumeDiff.addedCount < kVolumeAddedSlots) {
                sVolumeDiff.addedIndices[sVolumeDiff.addedCount] = i;
            }
            ++sVolumeDiff.addedCount;
        }
    }

    const u32 removedStored =
        sVolumeDiff.removedCount < kVolumeRemovedSlots
            ? sVolumeDiff.removedCount
            : kVolumeRemovedSlots;
    for (u32 removed = 0; removed < removedStored; ++removed) {
        const VolumeDescriptor &oldEntry =
            saved.entries[sVolumeDiff.removedIndices[removed]];
        for (u32 liveIndex = 0; liveIndex < live.count; ++liveIndex) {
            const VolumeDescriptor &newEntry = live.entries[liveIndex];
            if (findVolume(saved, newEntry) >= 0) continue;
            // These masks are HUD hints only. The guard's exact matching uses
            // the full boolean vectors below, so entries beyond bit 31 remain
            // fully validated without an undefined shift.
            if (removed < 32u && oldEntry.object == newEntry.object) {
                sVolumeDiff.objectReuseMask |= 1u << removed;
            }
            if (removed < 32u && oldEntry.archiveHeader != 0u &&
                oldEntry.archiveHeader == newEntry.archiveHeader) {
                sVolumeDiff.archiveReuseMask |= 1u << removed;
            }
        }
    }

    bool ordered = true;
    u32 nextLiveIndex = 0u;
    for (u32 i = 0; i < saved.count; ++i) {
        const s32 index = findVolume(live, saved.entries[i]);
        if (index >= 0) {
            if (static_cast<u32>(index) < nextLiveIndex) {
                ordered = false;
                break;
            }
            nextLiveIndex = static_cast<u32>(index) + 1u;
        }
    }
    sVolumeDiff.commonOrder = ordered ? 1u : 0u;

    if (sVolumeDiff.addedCount == 0u &&
        saved.count == live.count + sVolumeDiff.removedCount) {
        bool suffix = true;
        for (u32 i = 0; i < live.count; ++i) {
            if (!sameVolumeDescriptor(
                    saved.entries[sVolumeDiff.removedCount + i],
                    live.entries[i])) {
                suffix = false;
                break;
            }
        }
        sVolumeDiff.headOnly = suffix ? 1u : 0u;
    }
}

void commitSavedVolumeCensus(u32 generation) {
    sSavedVolumeCensus.generation = 0u;
    copyBytes(&sSavedVolumeCensus, &sLiveVolumeCensus,
              sizeof(sSavedVolumeCensus));
    sSavedVolumeCensus.generation = generation;
}

void failResourceCensus(ResourceCensus *census, u32 fault) {
    census->fault = fault;
    census->valid = 0u;
}

u32 hashResourceWords(u32 address, u32 size) {
    u32 hash = 2166136261u;
    for (u32 offset = 0u; offset < size; offset += sizeof(u32)) {
        hash = hashVolumeWord(hash, readWord(address + offset));
    }
    return hash;
}

bool captureResourceCensus(ResourceCensus *census,
                           const LiveIdentity &identity) {
    clearWords(census, sizeof(*census));
    census->slotCount = readByte(kResourceSlotCountGlobal);
    census->wantedCount = readWord(kResourceWantedCountGlobal);
    census->recordBase = readWord(kResourceRecordBaseGlobal);
    census->bulkBase = readWord(kResourceBulkBaseGlobal);
    census->slotSize = readWord(kResourceSlotSizeGlobal);

    if (census->slotCount != kResourceSlotCount) {
        failResourceCensus(census, kResourceFaultSlotCount);
        return false;
    }
    if (census->wantedCount > kResourceWantedCapacity) {
        failResourceCensus(census, kResourceFaultWantedCount);
        return false;
    }
    if (!isMem1Range(census->recordBase,
                     kResourceSlotCount * kResourceRecordSize) ||
        classifyVolumeRange(census->recordBase,
                            kResourceSlotCount * kResourceRecordSize,
                            identity) != kVolumeOwnerGame) {
        failResourceCensus(census, kResourceFaultRecords);
        return false;
    }
    if (census->slotSize != kResourceSlotSize) {
        failResourceCensus(census, kResourceFaultSlotSize);
        return false;
    }
    if (!isMem1ByteRange(census->bulkBase,
                         kResourceSlotCount * kResourceSlotSize) ||
        classifyVolumeRange(census->bulkBase,
                            kResourceSlotCount * kResourceSlotSize,
                            identity) != kVolumeOwnerGame) {
        failResourceCensus(census, kResourceFaultBulk);
        return false;
    }

    census->mapHash = hashResourceWords(kResourceMapBase, kResourceMapSize);
    for (u32 i = 0; i < kResourceSlotCount; ++i) {
        census->activeIds[i] =
            readWord(kResourceActiveBase + i * sizeof(u32));
        census->recordHashes[i] = hashResourceWords(
            census->recordBase + i * kResourceRecordSize,
            kResourceRecordSize);
        if (readWord(kResourceMarkBase + i * sizeof(u32)) != 0u) {
            census->markMask |= 1u << i;
        }
        const u32 expectedBacking =
            census->bulkBase + i * kResourceSlotSize;
        if (readWord(kResourceBackingBase + i * sizeof(u32)) !=
            expectedBacking) {
            census->backingBadMask |= 1u << i;
        }
    }
    for (u32 i = 0; i < census->wantedCount; ++i) {
        const u32 id = readWord(kResourceWantedBase + i * sizeof(u32));
        census->wantedIds[i] = id;
    }
    census->valid = 1u;
    return true;
}

s32 findWantedResource(const ResourceCensus &census, u32 id) {
    for (u32 i = 0; i < census.wantedCount; ++i) {
        if (census.wantedIds[i] == id) {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

void clearResourceDiff() {
    clearWords(&sResourceDiff, sizeof(sResourceDiff));
    for (u32 i = 0; i < 2u; ++i) {
        sResourceDiff.wantedRemovedIds[i] = 0xFFFFFFFFu;
        sResourceDiff.wantedAddedIds[i] = 0xFFFFFFFFu;
    }
}

void diffResourceCensus(const ResourceCensus &saved,
                        const ResourceCensus &live) {
    clearResourceDiff();
    sResourceDiff.ready = 1u;
    sResourceDiff.savedValid = saved.valid;
    sResourceDiff.liveValid = live.valid;
    if (!saved.valid || !live.valid) {
        return;
    }

    sResourceDiff.layoutChanged =
        saved.slotCount != live.slotCount ||
                saved.recordBase != live.recordBase ||
                saved.bulkBase != live.bulkBase ||
                saved.slotSize != live.slotSize
            ? 1u
            : 0u;
    sResourceDiff.mapChanged = saved.mapHash != live.mapHash ? 1u : 0u;
    if (saved.wantedCount != live.wantedCount) {
        sResourceDiff.wantedSequenceChanged = 1u;
    } else {
        for (u32 i = 0; i < saved.wantedCount; ++i) {
            if (saved.wantedIds[i] != live.wantedIds[i]) {
                sResourceDiff.wantedSequenceChanged = 1u;
                break;
            }
        }
    }
    for (u32 i = 0; i < kResourceSlotCount; ++i) {
        if (saved.activeIds[i] != live.activeIds[i]) {
            sResourceDiff.activeMismatchMask |= 1u << i;
        }
        if (saved.recordHashes[i] != live.recordHashes[i]) {
            sResourceDiff.recordMismatchMask |= 1u << i;
        }
    }
    for (u32 i = 0; i < saved.wantedCount; ++i) {
        const u32 id = saved.wantedIds[i];
        if (findWantedResource(live, id) < 0) {
            if (sResourceDiff.wantedRemovedCount < 2u) {
                sResourceDiff.wantedRemovedIds[
                    sResourceDiff.wantedRemovedCount] = id;
            }
            ++sResourceDiff.wantedRemovedCount;
        }
    }
    for (u32 i = 0; i < live.wantedCount; ++i) {
        const u32 id = live.wantedIds[i];
        if (findWantedResource(saved, id) < 0) {
            if (sResourceDiff.wantedAddedCount < 2u) {
                sResourceDiff.wantedAddedIds[
                    sResourceDiff.wantedAddedCount] = id;
            }
            ++sResourceDiff.wantedAddedCount;
        }
    }
}

void commitSavedResourceCensus(u32 generation) {
    sSavedResourceCensus.generation = 0u;
    copyBytes(&sSavedResourceCensus, &sLiveResourceCensus,
              sizeof(sSavedResourceCensus));
    sSavedResourceCensus.generation = generation;
}

void failModelCensus(const ModelCensusView &census, u32 fault) {
    census.metadata->fault = fault;
    census.metadata->valid = 0u;
}

u32 hashModelWords(const u32 *words, u32 size) {
    u32 hash = 2166136261u;
    for (u32 i = 0; i < size / sizeof(u32); ++i) {
        hash = hashVolumeWord(hash, words[i]);
    }
    return hash;
}

bool captureModelCensus(const ModelCensusView &census) {
    clearWords(census.metadata, kModelCensusRecordSize);
    const u32 tableBefore =
        hashResourceWords(kModelTableBase, kModelTableSize);
    const u32 registryBefore =
        hashResourceWords(kModelRegistryBase, kModelRegistrySize);
    copyWords(census.words, reinterpret_cast<const void *>(kModelTableBase),
              kModelTableSize);
    copyWords(census.registryWords,
              reinterpret_cast<const void *>(kModelRegistryBase),
              kModelRegistrySize);
    const u32 tableAfter = hashResourceWords(kModelTableBase, kModelTableSize);
    const u32 registryAfter =
        hashResourceWords(kModelRegistryBase, kModelRegistrySize);
    const u32 tableCopy = hashModelWords(census.words, kModelTableSize);
    const u32 registryCopy =
        hashModelWords(census.registryWords, kModelRegistrySize);
    if (tableBefore != tableAfter || tableAfter != tableCopy ||
        registryBefore != registryAfter || registryAfter != registryCopy) {
        failModelCensus(census, kModelFaultChanged);
        return false;
    }
    census.metadata->signature = tableCopy;
    census.metadata->registrySignature = registryCopy;
    census.metadata->valid = 1u;
    return true;
}

u32 modelEntryWord(const ModelCensusView &census, u32 index, u32 offset) {
    return census.words[index * (kModelEntrySize / sizeof(u32)) +
                        offset / sizeof(u32)];
}

bool sameModelEntry(const ModelCensusView &saved,
                    const ModelCensusView &live,
                    u32 index) {
    const u32 first = index * (kModelEntrySize / sizeof(u32));
    for (u32 i = 0; i < kModelEntrySize / sizeof(u32); ++i) {
        if (saved.words[first + i] != live.words[first + i]) {
            return false;
        }
    }
    return true;
}

u32 modelRegistryWord(const ModelCensusView &census, u32 index, u32 offset) {
    return census.registryWords[
        index * (kModelRegistryEntrySize / sizeof(u32)) +
        offset / sizeof(u32)];
}

u32 modelRegistryEntrySignature(const ModelCensusView &census, u32 index) {
    const u32 first =
        index * (kModelRegistryEntrySize / sizeof(u32));
    return hashModelWords(&census.registryWords[first],
                          kModelRegistryEntrySize);
}

bool sameModelRegistryEntry(const ModelCensusView &saved,
                            const ModelCensusView &live, u32 index) {
    const u32 first =
        index * (kModelRegistryEntrySize / sizeof(u32));
    for (u32 i = 0; i < kModelRegistryEntrySize / sizeof(u32); ++i) {
        if (saved.registryWords[first + i] != live.registryWords[first + i]) {
            return false;
        }
    }
    return true;
}

void captureModelName(ModelChange *change, u32 path) {
    change->name[0] = '?';
    change->name[1] = '\0';
    if (!isMem1ByteRange(path, 1u)) return;

    u32 component = path;
    bool terminated = false;
    for (u32 i = 0; i < kModelPathLimit; ++i) {
        if (!isMem1ByteRange(path + i, 1u)) return;
        const u8 value = readByte(path + i);
        if (value == 0u) {
            terminated = true;
            break;
        }
        if (value == '/' || value == '\\') {
            component = path + i + 1u;
        }
    }
    if (!terminated || !isMem1ByteRange(component, 1u)) return;

    u32 length = 0u;
    for (u32 i = 0; i < kModelPathLimit && length + 1u < kModelNameBytes;
         ++i) {
        if (!isMem1ByteRange(component + i, 1u)) return;
        const u8 value = readByte(component + i);
        if (value == 0u || value == '.') break;
        change->name[length++] =
            value >= 0x20u && value <= 0x7Eu
                ? static_cast<char>(lowerAscii(value))
                : '.';
    }
    if (length == 0u) {
        change->name[0] = '?';
        change->name[1] = '\0';
    } else {
        change->name[length] = '\0';
    }
}

void clearModelDiff() {
    clearWords(&sModelDiff, sizeof(sModelDiff));
    for (u32 i = 0; i < kModelChangeSlots; ++i) {
        sModelDiff.changes[i].index = 0xFFFFFFFFu;
    }
}

void diffModelCensus(const ModelCensusView &saved,
                     const ModelCensusView &live) {
    clearModelDiff();
    sModelDiff.ready = 1u;
    sModelDiff.savedValid = saved.metadata->valid;
    sModelDiff.liveValid = live.metadata->valid;
    if (!saved.metadata->valid || !live.metadata->valid) return;

    for (u32 i = 0; i < kModelEntryCount; ++i) {
        const bool primaryChanged = !sameModelEntry(saved, live, i);
        const bool registryChanged =
            !sameModelRegistryEntry(saved, live, i);
        if (!primaryChanged && !registryChanged) continue;
        if (sModelDiff.changedCount < kModelChangeSlots) {
            ModelChange &change =
                sModelDiff.changes[sModelDiff.changedCount];
            change.index = i;
            change.sourceMask =
                (primaryChanged ? kModelChangedPrimary : 0u) |
                (registryChanged ? kModelChangedRegistry : 0u);
            change.savedHandle =
                primaryChanged ? modelEntryWord(saved, i, 0x04u)
                               : modelRegistryWord(saved, i, 0x04u);
            change.liveHandle =
                primaryChanged ? modelEntryWord(live, i, 0x04u)
                               : modelRegistryWord(live, i, 0x04u);
            change.savedRoot = modelEntryWord(saved, i, 0x08u);
            change.liveRoot = modelEntryWord(live, i, 0x08u);
            change.savedState = modelEntryWord(saved, i, 0x30u);
            change.liveState = modelEntryWord(live, i, 0x30u);
            change.savedRegistrySignature =
                modelRegistryEntrySignature(saved, i);
            change.liveRegistrySignature =
                modelRegistryEntrySignature(live, i);
            const u32 savedPath = modelEntryWord(saved, i, 0x00u);
            const u32 livePath = modelEntryWord(live, i, 0x00u);
            captureModelName(&change, savedPath != 0u ? savedPath : livePath);
        }
        ++sModelDiff.changedCount;
    }
}

void commitSavedModelCensus(u32 generation) {
    sSavedModelCensus.metadata->generation = 0u;
    copyBytes(sSavedModelCensus.metadata, sLiveModelCensus.metadata,
              kModelCensusMetadataSize);
    const u32 tableCopy =
        hashModelWords(sSavedModelCensus.words, kModelTableSize);
    const u32 registryCopy =
        hashModelWords(sSavedModelCensus.registryWords, kModelRegistrySize);
    if (!sLiveModelCensus.metadata->valid ||
        tableCopy != sLiveModelCensus.metadata->signature ||
        registryCopy != sLiveModelCensus.metadata->registrySignature) {
        failModelCensus(sSavedModelCensus, kModelFaultChanged);
    }
    // Generation is the commit word for the scratch metadata.  Publish it
    // last so a torn/incoherent census can only reject a cross-room load.
    sSavedModelCensus.metadata->generation = generation;
}

void diagnoseVolumeEpoch(const SnapshotHeader *header,
                         const LiveIdentity &live, u32 mask) {
    clearVolumeDiff();
    clearResourceDiff();
    clearModelDiff();
    if (sSavedResourceCensus.generation == header->generation) {
        captureResourceCensus(&sLiveResourceCensus, live);
        diffResourceCensus(sSavedResourceCensus, sLiveResourceCensus);
    }
    if (sSavedModelCensus.metadata->generation == header->generation) {
        captureModelCensus(sLiveModelCensus);
        diffModelCensus(sSavedModelCensus, sLiveModelCensus);
    }
    const u32 volumeMask = SUSAMUNE_LM_EPOCH_VOLUME_COUNT |
                           SUSAMUNE_LM_EPOCH_VOLUME_HEAD |
                           SUSAMUNE_LM_EPOCH_VOLUME_TAIL;
    if ((mask & (volumeMask | LM_STATE_RELOCATABLE_GAME_ROOT_MASK)) == 0u ||
        sSavedVolumeCensus.generation != header->generation) {
        return;
    }
    captureVolumeCensus(&sLiveVolumeCensus, live);
    diffVolumeCensus(sSavedVolumeCensus, sLiveVolumeCensus);
}

bool orderedVolumeReplacementMatches(bool allowUnchanged = false) {
    const u32 removed = sVolumeDiff.removedCount;
    const u32 added = sVolumeDiff.addedCount;
    // Walking farther through the mansion can leave only live additions (or
    // only saved removals).  The common-subsequence, ownership, resource, and
    // model proofs below handle a zero side; only a no-change/reorder-only
    // result is not a bounded replacement.
    if (!sVolumeDiff.ready || !sVolumeDiff.savedValid ||
        !sVolumeDiff.liveValid || !sVolumeDiff.commonOrder ||
        (!allowUnchanged && removed == 0u && added == 0u) || removed > kVolumeRemovedSlots ||
        added > kVolumeAddedSlots || removed + added > kModelChangeSlots ||
        sSavedVolumeCensus.count < removed ||
        sLiveVolumeCensus.count < added) {
        return false;
    }

    const u32 savedCommon = sSavedVolumeCensus.count - removed;
    const u32 liveCommon = sLiveVolumeCensus.count - added;
    if (savedCommon == 0u || savedCommon != liveCommon) {
        return false;
    }

    // The room-owned archives are inserted around long-lived JKR volumes, not
    // necessarily as one leading block.  Validate the recorded change indices
    // and compare the exact ordered common subsequence after filtering them.
    for (u32 i = 0; i < removed; ++i) {
        const u32 index = sVolumeDiff.removedIndices[i];
        if (index >= sSavedVolumeCensus.count ||
            (i != 0u && index <= sVolumeDiff.removedIndices[i - 1u])) {
            return false;
        }
    }
    for (u32 i = 0; i < added; ++i) {
        const u32 index = sVolumeDiff.addedIndices[i];
        if (index >= sLiveVolumeCensus.count ||
            (i != 0u && index <= sVolumeDiff.addedIndices[i - 1u])) {
            return false;
        }
    }

    u32 savedIndex = 0u;
    u32 liveIndex = 0u;
    u32 removedIndex = 0u;
    u32 addedIndex = 0u;
    u32 commonCount = 0u;
    while (savedIndex < sSavedVolumeCensus.count ||
           liveIndex < sLiveVolumeCensus.count) {
        if (removedIndex < removed &&
            sVolumeDiff.removedIndices[removedIndex] == savedIndex) {
            ++removedIndex;
            ++savedIndex;
            continue;
        }
        if (addedIndex < added &&
            sVolumeDiff.addedIndices[addedIndex] == liveIndex) {
            ++addedIndex;
            ++liveIndex;
            continue;
        }
        if (savedIndex >= sSavedVolumeCensus.count ||
            liveIndex >= sLiveVolumeCensus.count ||
            !sameVolumeDescriptor(sSavedVolumeCensus.entries[savedIndex],
                                  sLiveVolumeCensus.entries[liveIndex])) {
            return false;
        }
        ++commonCount;
        ++savedIndex;
        ++liveIndex;
    }
    return removedIndex == removed && addedIndex == added &&
           commonCount == savedCommon;
}

bool changedArchiveIsRewindable(const VolumeDescriptor &entry,
                                const LiveIdentity &identity,
                                const LmSharedArchiveDescriptor &shared) {
    // The heap-pointer fields inside JKRMemArchive do not consistently name
    // the allocator that owns its object/backing buffer.  What matters for the
    // raw rewind is that both byte ranges are inside the captured game heap.
    const LmArchiveGuardEntry record = {entry.node, entry.object, entry.vtable,
        entry.stateFlags, entry.archiveHeader, entry.fileLength, entry.ownerFlags};
    u32 reasons = LmArchiveGuardReasons(&record, identity.heapStart, identity.heapEnd,
                                       kMemArchiveVtable);
    // Native scene reload rebuilds GAME wrappers borrowing nested files from
    // the retained SYS parent. Their bytes are now in the mandatory companion.
    // This admits no SYS wrapper, allocator header, or unrelated SYS resource.
    if (LmArchiveGuardSharedBackingAllowed(&record, entry.objectOwnerHeap,
            entry.archiveHeap, entry.type, entry.mountSource, identity.heap, &shared)) {
        reasons &= ~(LM_ARCHIVE_GUARD_BACKING_LOCATION | LM_ARCHIVE_GUARD_BACKING_RANGE);
    }
    if (!reasons) return true;
    sCrossRoomFault = 0x600u | reasons;
    sCrossRoomFaultValue = LmArchiveGuardFailureValue(&record, reasons);
    return false;
}

bool changedModelWordsAreKnown(u32 index) {
    const u32 primaryFirst = index * (kModelEntrySize / sizeof(u32));
    const u32 registryFirst =
        index * (kModelRegistryEntrySize / sizeof(u32));
    return LmStateModelChangesKnown(sSavedModelCensus.words + primaryFirst,
        sLiveModelCensus.words + primaryFirst,
        sSavedModelCensus.registryWords + registryFirst,
        sLiveModelCensus.registryWords + registryFirst) != 0;
}

bool crossRoomFault(u32 code, u32 value) {
    sCrossRoomFault = code;
    sCrossRoomFaultValue = value;
    return false;
}

bool renderTargetsValid(const SnapshotHeader *header, const LiveIdentity &live) {
    unsigned int fault = 0u, value = 0u;
    if (LmRenderTargetsValidate(const_cast<SnapshotHeader *>(header), grainReadWord,
            header ? header->heapStart : live.heapStart,
            header ? header->heapEnd : live.heapEnd,
            header ? header->usedHead : live.heapUsedHead,
            header ? header->usedTail : live.heapUsedTail, &fault, &value)) return true;
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xF3u, fault, value);
    gateFailure(Gate::RestoreProof, fault, true);
    return crossRoomFault(header ? 0x44u : 0x45u, fault);
}

bool everyChangedVolumeMatched(const bool *matched, u32 count,
                              const VolumeCensus &volumes, const u32 *changes,
                              bool saved) {
    for (u32 i = 0u; i < count; ++i) {
        if (!matched[i]) {
            if (changes[i] < volumes.count) {
                const VolumeDescriptor &entry = volumes.entries[changes[i]];
                sUnmatchedVolumeIndex = (saved ? 0x80000000u : 0u) | changes[i];
                sUnmatchedVolumeObject = entry.object;
                sUnmatchedVolumeName = entry.nameHash;
                sUnmatchedVolumeBacking = entry.archiveHeader;
            }
            return crossRoomFault(0x13u, i);
        }
    }
    return true;
}

u32 savedStaticWord(const SnapshotHeader *header, u32 address) {
    u32 offset = header->stateStaticsOffset;
    for (u32 i = 0u; i < kStateStaticRangeCount; ++i) {
        const StaticRange &range = kStateStaticRanges[i];
        if (address >= range.address && address - range.address <= range.size - 4u)
            return readWord(kSnapshotBase + offset + address - range.address);
        offset += range.size;
    }
    return 0u;
}

bool matchModelEndpoint(const ModelCensusView &models, u32 index,
                        const VolumeCensus &volumes, const u32 *changes,
                        u32 count, bool *matched) {
    const u32 state = modelEntryWord(models, index, 0x30u);
    const u32 handle = modelEntryWord(models, index, 0x04u);
    const u32 registryHandle = modelRegistryWord(models, index, 0x04u);
    const u32 root = modelEntryWord(models, index, 0x08u);
    const u32 registryRoot = modelRegistryWord(models, index, 0x0Cu);
    if (state == 0u)
        return LmStateModelEndpointValid(state, handle, registryHandle,
                                         root, registryRoot, 0u, 0u) != 0;
    for (u32 i = 0u; i < count; ++i) {
        if (changes[i] >= volumes.count) return false;
        const VolumeDescriptor &archive = volumes.entries[changes[i]];
        if (archive.object != handle) continue;
        if (matched[i] || !LmStateModelEndpointValid(state, handle,
                registryHandle, root, registryRoot,
                archive.archiveHeader, archive.fileLength)) return false;
        matched[i] = true;
        return true;
    }
    return false;
}

bool matchVrArchive(const SnapshotHeader *header, bool saved,
                    const VolumeCensus &volumes, const u32 *changes,
                    u32 count, bool *matched) {
    const u32 handle = saved ? savedStaticWord(header, kVrSceneArchiveGlobal)
                             : readWord(kVrSceneArchiveGlobal);
    // The VR archive is not a model-table row. Its sole retained owner and
    // fixed output table are captured with the GAME-resident private heap.
    for (u32 i = 0u; i < count; ++i) {
        if (changes[i] >= volumes.count) return false;
        if (volumes.entries[changes[i]].object != handle) continue;
        if (matched[i]) return false;
        matched[i] = true;
    }
    return true;
}

bool matchMapArchive(const SnapshotHeader *header, const LiveIdentity &live,
                     bool saved, const VolumeCensus &volumes,
                     const u32 *changes, u32 count, bool *matched) {
    const u32 handle = saved ? header->mapArchive : live.mapArchive;
    for (u32 i = 0u; i < count; ++i) {
        if (changes[i] >= volumes.count) return false;
        const VolumeDescriptor &archive = volumes.entries[changes[i]];
        if (archive.object != handle) continue;
        if (matched[i]) return false;
        const LmMapArchiveRoots roots = {
            saved ? header->missionMode : live.missionMode, handle,
            archive.archiveHeader, archive.fileLength,
            saved ? header->heap : live.heap,
            saved ? header->heapStart : live.heapStart,
            saved ? header->heapEnd : live.heapEnd,
            saved ? header->usedHead : live.heapUsedHead,
            saved ? header->usedTail : live.heapUsedTail};
        unsigned int fault = 0u, value = 0u;
        if (!LmMapArchiveValidate(saved ? const_cast<SnapshotHeader *>(header) : nullptr,
                grainReadWord, &roots, &fault, &value))
            return crossRoomFault(saved ? 0x46u : 0x47u, fault);
        matched[i] = true;
    }
    return true;
}

bool modelReplacementMatches(const SnapshotHeader *header, const LiveIdentity &live) {
    const u32 removed = sVolumeDiff.removedCount;
    const u32 added = sVolumeDiff.addedCount;
    if (!sModelDiff.ready || !sModelDiff.savedValid ||
        !sModelDiff.liveValid ||
        sModelDiff.changedCount > kModelChangeSlots) {
        return false;
    }

    // A state of 1 or 2 is an in-flight model request even when the global
    // DVD counter happens to be idle at this instant.
    for (u32 i = 0u; i < kModelEntryCount; ++i) {
        const u32 savedState = modelEntryWord(sSavedModelCensus, i, 0x30u);
        const u32 liveState = modelEntryWord(sLiveModelCensus, i, 0x30u);
        if ((savedState != 0u && savedState != 3u) ||
            (liveState != 0u && liveState != 3u)) {
            return crossRoomFault(0x14u, i);
        }
    }

    bool matchedRemoved[kVolumeRemovedSlots];
    bool matchedAdded[kVolumeAddedSlots];
    for (u32 i = 0u; i < kVolumeRemovedSlots; ++i) {
        matchedRemoved[i] = false;
    }
    for (u32 i = 0u; i < kVolumeAddedSlots; ++i) {
        matchedAdded[i] = false;
    }
    for (u32 i = 0u; i < sModelDiff.changedCount; ++i) {
        const ModelChange &change = sModelDiff.changes[i];
        if (change.index >= kModelEntryCount ||
            !changedModelWordsAreKnown(change.index)) {
            return crossRoomFault(0x10u, change.index);
        }
        // A full scene reload can recreate the same model ID (3 -> 3).
        // Prove each side independently instead of equating rows to volumes.
        if (!matchModelEndpoint(sSavedModelCensus, change.index,
                                sSavedVolumeCensus,
                                sVolumeDiff.removedIndices, removed,
                                matchedRemoved))
            return crossRoomFault(0x11u, change.index);
        if (!matchModelEndpoint(sLiveModelCensus, change.index,
                                sLiveVolumeCensus,
                                sVolumeDiff.addedIndices, added,
                                matchedAdded)) {
            return crossRoomFault(0x12u, change.index);
        }
    }
    return matchVrArchive(header, true, sSavedVolumeCensus,
                         sVolumeDiff.removedIndices, removed, matchedRemoved) &&
           matchVrArchive(header, false, sLiveVolumeCensus,
                         sVolumeDiff.addedIndices, added, matchedAdded) &&
           matchMapArchive(header, live, true, sSavedVolumeCensus,
                         sVolumeDiff.removedIndices, removed, matchedRemoved) &&
           matchMapArchive(header, live, false, sLiveVolumeCensus,
                         sVolumeDiff.addedIndices, added, matchedAdded) &&
           everyChangedVolumeMatched(matchedRemoved, removed, sSavedVolumeCensus,
                                     sVolumeDiff.removedIndices, true) &&
           everyChangedVolumeMatched(matchedAdded, added, sLiveVolumeCensus,
                                     sVolumeDiff.addedIndices, false);
}

bool resourceReplacementMatches(const SnapshotHeader *header,
                                 const LiveIdentity &live) {
    static_assert(kResourceSlotCount == LM_STATE_RESOURCE_SLOT_COUNT &&
                      kResourceRecordSize == LM_STATE_RESOURCE_RECORD_SIZE,
                  "retail room-resource record layout changed");
    const u32 size = kResourceSlotCount * kResourceRecordSize;
    const u32 savedBase = sSavedResourceCensus.recordBase;
    const u32 liveBase = sLiveResourceCensus.recordBase;
    if (!isMem1Range(savedBase, size) || !isMem1Range(liveBase, size) ||
        !rangeInside(savedBase, savedBase + size,
                     header->heapStart, header->heapEnd) ||
        !rangeInside(liveBase, liveBase + size,
                     live.heapStart, live.heapEnd)) return crossRoomFault(1u, savedBase);

    const u32 savedOffset = savedBase - header->heapStart;
    if (savedOffset > header->heapDataSize ||
        size > header->heapDataSize - savedOffset) return crossRoomFault(1u, savedOffset);
    const bool matches = LmStateResourceReloadChangesMatch(
        sResourceDiff.activeMismatchMask, sResourceDiff.recordMismatchMask,
        sSavedResourceCensus.activeIds, sLiveResourceCensus.activeIds,
        reinterpret_cast<const u8 *>(
            kSnapshotBase + header->heapDataOffset + savedOffset),
        reinterpret_cast<const u8 *>(liveBase),
        sSavedResourceCensus.bulkBase, sLiveResourceCensus.bulkBase,
        kResourceSlotSize, header->heapStart, header->heapEnd) != 0;
    return matches || crossRoomFault(2u, sResourceDiff.recordMismatchMask &
                                          ~sResourceDiff.activeMismatchMask);
}

bool resourceLayoutCompatible(const SnapshotHeader *header, const LiveIdentity &live) {
    if (!sResourceDiff.layoutChanged) return true;
    // Native scene reload may move the seven-slot allocations. Validate each
    // entire allocation and captured owner graph, never just an address range.
    const LmStateResourceRoots savedRoots = {sSavedResourceCensus.recordBase,
        sSavedResourceCensus.bulkBase, sSavedResourceCensus.slotCount, sSavedResourceCensus.slotSize};
    const LmStateResourceRoots liveRoots = {sLiveResourceCensus.recordBase,
        sLiveResourceCensus.bulkBase, sLiveResourceCensus.slotCount, sLiveResourceCensus.slotSize};
    unsigned int fault = 0u, value = 0u;
    if (!LmStateResourceRootsValidate(const_cast<SnapshotHeader *>(header), grainReadWord,
            &savedRoots, header->heapStart, header->heapEnd, &fault, &value))
        return crossRoomFault(0x42u, fault);
    if (!LmStateResourceRootsValidate(nullptr, grainReadWord, &liveRoots,
            live.heapStart, live.heapEnd, &fault, &value))
        return crossRoomFault(0x43u, fault);
    return true;
}

bool guardedCrossRoomRestoreAllowed(const SnapshotHeader *header,
                                    const LiveIdentity &live,
                                    const EpochMismatch &mismatch) {
    sCrossRoomFault = 0u;
    sCrossRoomFaultValue = 0u;
    sCrossRoomGuard = kCrossRoomGuardMask;
    sUnmatchedVolumeIndex = 0xFFFFFFFFu;
    // Same-map native reload may relocate captured GAME roots. Map/archive,
    // scene, audio, OS/SYS identities and volume tail still remain exact.
    if (!LmStateGameEpochMaskAllowed(mismatch.mask)) {
        return false;
    }
    if (mismatch.mask & LM_STATE_RELOCATABLE_GAME_ROOT_MASK) {
        const LmStateGameRoots savedRoots = {header->missionMode, header->mapArchive,
            header->gameMode, header->simpleModeler, header->mapCol, header->enTypesManager};
        const LmStateGameRoots liveRoots = {live.missionMode, live.mapArchive,
            live.gameMode, live.simpleModeler, live.mapCol, live.enTypesManager};
        unsigned int fault = 0u, value = 0u;
        if (!LmStateGameRootsValidate(const_cast<SnapshotHeader *>(header), grainReadWord,
                &savedRoots, header->heapStart, header->heapEnd, &fault, &value))
            return crossRoomFault(0x40u, fault);
        if (!LmStateGameRootsValidate(nullptr, grainReadWord, &liveRoots,
                live.heapStart, live.heapEnd, &fault, &value))
            return crossRoomFault(0x41u, fault);
    }

    sCrossRoomGuard = kCrossRoomGuardGeneration;
    if (sSavedVolumeCensus.generation != header->generation ||
        sSavedResourceCensus.generation != header->generation ||
        sSavedModelCensus.metadata->generation != header->generation) {
        return false;
    }

    diagnoseVolumeEpoch(header, live, mismatch.mask);
    sCrossRoomGuard = kCrossRoomGuardVolume;
    if (!sVolumeDiff.ready || !sSavedVolumeCensus.valid ||
        !sLiveVolumeCensus.valid ||
        sSavedVolumeCensus.count != header->volume[2] ||
        sSavedVolumeCensus.head != header->volume[0] ||
        sSavedVolumeCensus.tail != header->volume[1] ||
        sSavedVolumeCensus.tail != sLiveVolumeCensus.tail ||
        sVolumeDiff.currentChanged != 0u) {
        return false;
    }

    sCrossRoomGuard = kCrossRoomGuardTopology;
    if (!orderedVolumeReplacementMatches(LmStateGameRootOnlyEpoch(mismatch.mask) != 0)) return false;

    sCrossRoomGuard = kCrossRoomGuardArchive;
    LmSharedArchiveDescriptor shared;
    if (!sharedArchiveMatchesSnapshot(header, &shared)) return false;
    for (u32 i = 0u; i < sVolumeDiff.removedCount; ++i) {
        const u32 index = sVolumeDiff.removedIndices[i];
        if (index >= sSavedVolumeCensus.count ||
            !changedArchiveIsRewindable(sSavedVolumeCensus.entries[index],
                                        live, shared)) {
            return false;
        }
    }
    for (u32 i = 0u; i < sVolumeDiff.addedCount; ++i) {
        const u32 index = sVolumeDiff.addedIndices[i];
        if (index >= sLiveVolumeCensus.count ||
            !changedArchiveIsRewindable(sLiveVolumeCensus.entries[index],
                                        live, shared)) {
            return false;
        }
    }

    sCrossRoomGuard = kCrossRoomGuardResource;
    if (!sResourceDiff.ready || !sResourceDiff.savedValid ||
        !sResourceDiff.liveValid || !resourceLayoutCompatible(header, live) ||
        sResourceDiff.mapChanged != 0u ||
        sSavedResourceCensus.backingBadMask != 0u ||
        sLiveResourceCensus.backingBadMask != 0u ||
        sSavedResourceCensus.wantedCount != 0u ||
        sLiveResourceCensus.wantedCount != 0u ||
        sResourceDiff.wantedSequenceChanged != 0u ||
        !resourceReplacementMatches(header, live)) {
        return false;
    }

    sCrossRoomGuard = kCrossRoomGuardModel;
    if (!sModelDiff.ready || !sSavedModelCensus.metadata->valid ||
        !sLiveModelCensus.metadata->valid) {
        return false;
    }
    sCrossRoomGuard = kCrossRoomGuardModelShape;
    if (!modelReplacementMatches(header, live)) return false;

    sCrossRoomGuard = kCrossRoomGuardAccepted;
    return true;
}

void repairSavedVolumeList(const SnapshotHeader *header) {
    if (sSavedVolumeCensus.generation != header->generation ||
        !sSavedVolumeCensus.valid) {
        return;
    }
    writeWord(kVolumeListGlobal, sSavedVolumeCensus.head);
    writeWord(kVolumeListGlobal + 4u, sSavedVolumeCensus.tail);
    writeWord(kVolumeListGlobal + 8u, sSavedVolumeCensus.count);
    writeWord(kCurrentVolumeGlobal, sSavedVolumeCensus.currentVolume);
    writeWord(kCurrentDirIdGlobal, sSavedVolumeCensus.currentDirId);
    for (u32 i = 0u; i < sSavedVolumeCensus.count; ++i) {
        const VolumeDescriptor &entry = sSavedVolumeCensus.entries[i];
        writeWord(entry.node, entry.object);
        writeWord(entry.node + 4u, kVolumeListGlobal);
        writeWord(entry.node + 8u, entry.previous);
        writeWord(entry.node + 0xCu, entry.next);
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            reinterpret_cast<void *>(entry.node), 0x10u);
    }
}

u32 crcByte(u32 crc, u8 byte) {
    return LmCrc32Byte(crc, byte);
}

u32 snapshotChecksum(const SnapshotHeader *header) {
    const volatile u8 *bytes = reinterpret_cast<const volatile u8 *>(header);
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < header->totalSize; ++i) {
        const bool ignored = i < sizeof(header->magic) ||
            (i >= __builtin_offsetof(SnapshotHeader, checksum) &&
             i < __builtin_offsetof(SnapshotHeader, checksum) +
                     sizeof(header->checksum));
        crc = crcByte(crc, ignored ? 0u : bytes[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

FreezeState freezeBegin() {
    FreezeState state;
    state.interruptsWereEnabled =
        reinterpret_cast<DisableInterruptsFn>(kOSDisableInterruptsAddr)();
    reinterpret_cast<SchedulerFn>(kOSDisableSchedulerAddr)();
    volatile u16 *dmaControl = reinterpret_cast<volatile u16 *>(0xCC005036u);
    state.dmaWasEnabled = (*dmaControl & 0x8000u) != 0u;
    *dmaControl = static_cast<u16>(*dmaControl & ~0x8000u);
    asm volatile("sync" ::: "memory");
    return state;
}

void freezeEnd(const FreezeState &state, bool journalLoad = false) {
    asm volatile("sync" ::: "memory");
    volatile u16 *dmaControl = reinterpret_cast<volatile u16 *>(0xCC005036u);
    const u16 liveControl = *dmaControl;
    *dmaControl = state.dmaWasEnabled
                      ? static_cast<u16>(liveControl | 0x8000u)
                      : static_cast<u16>(liveControl & ~0x8000u);
    asm volatile("sync" ::: "memory");
    if (journalLoad) {
        traceLoadPhase(0x71u, liveControl);
    }
    reinterpret_cast<SchedulerFn>(kOSEnableSchedulerAddr)();
    if (journalLoad) {
        traceLoadPhase(0x72u, state.interruptsWereEnabled ? 1u : 0u);
    }
    reinterpret_cast<RestoreInterruptsFn>(kOSRestoreInterruptsAddr)(
        state.interruptsWereEnabled);
    if (journalLoad) {
        traceLoadPhase(0x73u, state.interruptsWereEnabled ? 1u : 0u);
    }
}

bool quiesceAudio(const LiveIdentity &identity) {
    // LM's scene switch drains prior SE/sequence/stream handles and rebuilds
    // its required bootstrap sequence. Passing the live scene avoids a
    // resource-bank change; the replacement handle must remain engine-owned.
    if (!validAudioBootstrap(identity.audioBasic)) {
        return gateFailure(
            Gate::Audio,
            readWord(identity.audioBasic + kAudioBootstrapHandleOffset),
            true);
    }
    reinterpret_cast<AudioChangeSoundSceneFn>(kAudioChangeSoundSceneAddr)(
        reinterpret_cast<void *>(identity.audioBasic), identity.audioScene);
    if (!validAudioBootstrap(identity.audioBasic)) {
        return gateFailure(
            Gate::Audio,
            readWord(identity.audioBasic + kAudioBootstrapHandleOffset),
            true);
    }
    return true;
}

void setReject(LMState::Status status, u32 detail) {
    sStatus = status;
    LMNotice::show(status == LMState::Status::Busy ? LM_POPUP_BUSY : LM_POPUP_REJECTED);
    sRejectedGate = sGate;
    sRejectedGateValue = sGateValue;
    LMCrash::note(kEventStateReject, static_cast<u32>(status), detail);
    if (status != LMState::Status::Epoch) {
        sRejectTelemetryRecords[0] = {
            0xD3u, (static_cast<u32>(status) << 24) |
                (static_cast<u32>(sGate) << 16) | (sStableFrames & 0xFFFFu), detail};
        sRejectTelemetryRecords[1] = {0xD4u, readWord(kMapValueGlobal), readWord(kSceneValueGlobal)};
        sRejectTelemetryRecords[2] = {0xD5u, sGateValue, readWord(kMainDrawStateGlobal)};
        sRejectTelemetryRecords[3] = {0xD6u, readWord(kMainLoopModeGlobal), readWord(kMainLoopExitGlobal)};
        sRejectTelemetryAction = sRequestAction;
        sRejectTelemetryCount = 4u;
        sRejectTelemetryIndex = 0u;
        sRejectTelemetryHoldFrames = kRejectTelemetryHoldFrames;
        publishRejectTelemetry();
    }
}

void clearEpochMismatch(EpochMismatch *mismatch) {
    mismatch->mask = 0u;
    mismatch->saved = 0u;
    mismatch->live = 0u;
}

void clearEpochMismatch() {
    clearEpochMismatch(&sEpochMismatch);
}

void addEpochMismatch(EpochMismatch *mismatch, u32 field, u32 saved,
                      u32 live) {
    if (saved == live) {
        return;
    }
    if (mismatch->mask == 0u) {
        mismatch->saved = saved;
        mismatch->live = live;
    }
    mismatch->mask |= field;
}

void collectPreflightEpochMismatch(EpochMismatch *mismatch,
                                   const SnapshotHeader *header,
                                   const LiveIdentity &live) {
    clearEpochMismatch(mismatch);
    // The order is intentional: the first saved/live pair should describe the
    // most useful high-level cause while the mask still reports every change.
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_MAP_VALUE,
                     header->mapValue, live.mapValue);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_SCENE_VALUE,
                     header->sceneValue, live.sceneValue);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_CURRENT_SCENE,
                     header->currentScene, live.currentScene);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_PENDING_SCENE,
                     header->mainLoopPendingScene,
                     live.mainLoopPendingScene);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_LOOP_MODE,
                     header->mainLoopMode, live.mainLoopMode);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_AUDIO_SCENE,
                     header->audioScene, live.audioScene);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_MAP_ARCHIVE,
                     header->mapArchive, live.mapArchive);
    // JKRFileLoader::sVolumeList is {head, tail, count}. Cardinality is the
    // clearest first clue, followed by endpoint replacement or reordering.
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_VOLUME_COUNT,
                     header->volume[2], live.volume[2]);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_VOLUME_HEAD,
                     header->volume[0], live.volume[0]);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_VOLUME_TAIL,
                     header->volume[1], live.volume[1]);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_MISSION_MODE,
                     header->missionMode, live.missionMode);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_GAME_MODE,
                     header->gameMode, live.gameMode);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_SIMPLE_MODELER,
                     header->simpleModeler, live.simpleModeler);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_MAP_COL,
                     header->mapCol, live.mapCol);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_EN_TYPES,
                     header->enTypesManager, live.enTypesManager);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_GAME_HEAP,
                     header->heap, live.heap);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_GAME_HEAP_START,
                     header->heapStart, live.heapStart);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_GAME_HEAP_END,
                     header->heapEnd, live.heapEnd);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_ROOT_HEAP,
                     header->rootHeap, live.rootHeap);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_SYSTEM_HEAP,
                     header->systemHeap, live.systemHeap);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_AUDIO_BASIC,
                     header->audioBasic, live.audioBasic);
    addEpochMismatch(mismatch, SUSAMUNE_LM_EPOCH_DRAW_STATE,
                     header->mainDrawState, live.mainDrawState);
}

u32 rejectTelemetryByte(u32 value) {
    return value < 0xFFu ? value : 0xFFu;
}

u32 rejectTelemetryHalf(u32 value) {
    return value < 0xFFFFu ? value : 0xFFFFu;
}

void queueEpochRejectTelemetry(const EpochMismatch &mismatch,
                               const SnapshotHeader *header,
                               const LiveIdentity &live) {
    const u32 removed =
        sVolumeDiff.ready ? sVolumeDiff.removedCount : 0xFFu;
    const u32 added = sVolumeDiff.ready ? sVolumeDiff.addedCount : 0xFFu;
    const u32 models =
        sModelDiff.ready ? sModelDiff.changedCount : 0xFFFFu;
    const u32 epochPhase =
        SUSAMUNE_LM_EPOCH_PHASE_FLAG |
        ((sCrossRoomGuard << SUSAMUNE_LM_EPOCH_GUARD_SHIFT) &
         SUSAMUNE_LM_EPOCH_GUARD_MASK) |
        (mismatch.mask & SUSAMUNE_LM_EPOCH_MASK);

    sRejectTelemetryRecords[0] = {
        epochPhase,
        mismatch.saved,
        mismatch.live,
    };
    sRejectTelemetryRecords[1] = {
        kRejectSummaryPhase,
        (static_cast<u32>(LMState::Status::Epoch) << 24) |
            ((sCrossRoomGuard & 0xFFu) << 16) |
            (rejectTelemetryByte(header->volume[2]) << 8) |
            rejectTelemetryByte(live.volume[2]),
        (rejectTelemetryByte(removed) << 24) |
            (rejectTelemetryByte(added) << 16) |
            rejectTelemetryHalf(models),
    };
    sRejectTelemetryRecords[2] = {
        kRejectSavedIdentityPhase,
        header->mapValue,
        header->sceneValue,
    };
    sRejectTelemetryRecords[3] = {
        kRejectLiveIdentityPhase,
        live.mapValue,
        live.sceneValue,
    };
    sRejectTelemetryRecords[4] = {0xD7u,
        (sResourceDiff.activeMismatchMask << 24) |
        (sResourceDiff.recordMismatchMask << 16) |
        (sSavedResourceCensus.wantedCount << 8) | sLiveResourceCensus.wantedCount,
        (sResourceDiff.layoutChanged << 31) | (sResourceDiff.mapChanged << 30) |
        (sSavedResourceCensus.backingBadMask << 16) |
        (sLiveResourceCensus.backingBadMask << 8)};
    sRejectTelemetryRecords[5] = {0xD8u, sCrossRoomFault, sCrossRoomFaultValue};
    if (sCrossRoomFault == 0x13u && sUnmatchedVolumeIndex != 0xFFFFFFFFu) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xD9u,
                      sUnmatchedVolumeIndex, sUnmatchedVolumeObject);
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xDAu,
                      sUnmatchedVolumeName, sUnmatchedVolumeBacking);
    }
    sRejectTelemetryCount = kRejectTelemetryRecordCount;
    sRejectTelemetryIndex = 0u;
    sRejectTelemetryHoldFrames = kRejectTelemetryHoldFrames;
    sRejectTelemetryAction = SUSAMUNE_PHASE_ACTION_LOAD;
    publishRejectTelemetry();
}

void rejectDirectEpoch(u32 detail, const SnapshotHeader *header,
                       const LiveIdentity &live) {
    // Some fail-closed compatibility checks do not correspond to one of the
    // header/live epoch-mask fields. Still publish the complete rejection
    // bundle: the flagged phase carries the check-specific detail, while D0,
    // D1, and D2 retain the guard summary and both trustworthy identities.
    EpochMismatch mismatch = {};
    mismatch.saved = detail;
    sEpochMismatch = mismatch;
    queueEpochRejectTelemetry(mismatch, header, live);
    setReject(LMState::Status::Epoch, detail);
}

void rejectEpoch(const EpochMismatch &mismatch, const SnapshotHeader *header,
                 const LiveIdentity &live) {
    diagnoseVolumeEpoch(header, live, mismatch.mask);
    sEpochMismatch = mismatch;
    queueEpochRejectTelemetry(mismatch, header, live);
    setReject(LMState::Status::Epoch, mismatch.mask);
}

bool basicHeaderValid(const SnapshotHeader *header) {
    if (header->magic != kSnapshotMagic ||
        header->version != kSnapshotVersion ||
        header->headerSize != kHeaderSize ||
        header->gameId != SUSAMUNE_MOD_GAME_ID_LMJ ||
        header->heapMetadataOffset != kHeapMetadataOffset ||
        header->heapMetadataSize != kHeapMetadataSize ||
        header->stateStaticsOffset != kStateStaticsOffset ||
        header->stateStaticsSize != kStateStaticsSize ||
        header->heapDataOffset != kHeapDataOffset ||
        header->heapDataSize != header->heapSize || header->heapSize == 0u ||
        header->heapSize > kSnapshotCapacity - kHeapDataOffset ||
        header->totalSize != kHeapDataOffset + header->heapSize ||
        header->totalSize > LM_STATE_STORAGE_PAYLOAD_MAX - 0x2000u - 64u) {
        return false;
    }
    const bool plausibleCurrentHeap =
        header->currentHeap == header->rootHeap ||
        header->currentHeap == header->systemHeap ||
        header->currentHeap == header->heap ||
        (header->currentHeap >= header->heapStart &&
         header->currentHeap <= header->heapEnd - kHeapMetadataEnd &&
         (header->currentHeap & 3u) == 0u);
    const bool childRangesValid =
        rangeInside(header->systemHeapStart, header->systemHeapEnd,
                    header->rootHeapStart, header->rootHeapEnd) &&
        rangeInside(header->heapStart, header->heapEnd,
                    header->rootHeapStart, header->rootHeapEnd) &&
        (header->systemHeapEnd <= header->heapStart ||
         header->heapEnd <= header->systemHeapStart);
    return header->heapStart < header->heapEnd &&
           header->heapSize == header->heapEnd - header->heapStart &&
           ((header->heapStart | header->heapEnd) &
            (kExpHeapAlignment - 1u)) == 0u &&
           isMem1Range(header->heapStart, header->heapSize) &&
           header->rootHeapStart < header->rootHeapEnd &&
           header->rootHeapSize ==
               header->rootHeapEnd - header->rootHeapStart &&
           ((header->rootHeapStart | header->rootHeapEnd) &
            (kExpHeapAlignment - 1u)) == 0u &&
           isMem1Range(header->rootHeapStart, header->rootHeapSize) &&
           header->systemHeapStart < header->systemHeapEnd &&
           header->systemHeapSize ==
               header->systemHeapEnd - header->systemHeapStart &&
           ((header->systemHeapStart | header->systemHeapEnd) &
            (kExpHeapAlignment - 1u)) == 0u &&
           isMem1Range(header->systemHeapStart, header->systemHeapSize) &&
           isMem1Range(header->heap, kHeapMetadataEnd) &&
           isMem1Range(header->rootHeap, kHeapMetadataEnd) &&
           isMem1Range(header->systemHeap, kHeapMetadataEnd) &&
           header->rootHeap != header->systemHeap &&
           header->rootHeap != header->heap &&
           header->systemHeap != header->heap && childRangesValid &&
           plausibleCurrentHeap && header->missionMode != 0u &&
           header->missionMode >= header->heapStart &&
           header->missionMode <= header->heapEnd - 0x1Cu &&
           header->gameMode == header->missionMode &&
           header->gameModeCount == 1u &&
           header->mainLoopMode == 2u &&
           header->mainLoopPendingScene == header->sceneValue &&
           header->mainDrawState <= 7u &&
           isMem1Range(header->currentScene, sizeof(u32)) &&
           header->audioBasic == kAudioStaticObject &&
           header->heapMode <= 0xFFu && header->heapGroup <= 0xFFu &&
           header->rootHeapMode <= 0xFFu &&
           header->rootHeapGroup <= 0xFFu &&
           header->systemHeapMode <= 0xFFu &&
           header->systemHeapGroup <= 0xFFu &&
           header->currentHeapGroup <= 0xFFu;
}

#include "lm_state_shared.inc"

void initializeSlot() {
    if (sSlotInitialized) {
        return;
    }
    initializeStorage();
    if (!storageStartupReady()) return;
    // MEM2 is not a persistent-state format. Invalidate the commit word once
    // per injected payload so a valid-looking slot left by an earlier game
    // session can never be loaded into a fresh process.
    SnapshotHeader *header =
        reinterpret_cast<SnapshotHeader *>(kSnapshotBase);
    header->magic = 0u;
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(header, 32u);
    asm volatile("sync" ::: "memory");
    clearWords(&sSavedVolumeCensus, sizeof(sSavedVolumeCensus));
    clearWords(&sLiveVolumeCensus, sizeof(sLiveVolumeCensus));
    clearVolumeDiff();
    clearWords(&sSavedResourceCensus, sizeof(sSavedResourceCensus));
    clearWords(&sLiveResourceCensus, sizeof(sLiveResourceCensus));
    clearResourceDiff();
    clearWords(sSavedModelCensus.metadata, kModelCensusMetadataSize);
    clearWords(sLiveModelCensus.metadata, kModelCensusRecordSize);
    clearModelDiff();
    sSlotInitialized = true;
}

bool headerMatchesLive(const SnapshotHeader *header,
                       const LiveIdentity &live,
                       bool allowGuardedVolumeDrift = false) {
    return header->heap == live.heap &&
           header->heapStart == live.heapStart &&
           header->heapEnd == live.heapEnd &&
           header->heapSize == live.heapSize &&
           header->rootHeap == live.rootHeap &&
           header->rootHeapStart == live.rootHeapStart &&
           header->rootHeapEnd == live.rootHeapEnd &&
           header->rootHeapSize == live.rootHeapSize &&
           header->rootHeapMode == live.rootHeapMode &&
           header->rootHeapGroup == live.rootHeapGroup &&
           header->rootFreeHead == live.rootFreeHead &&
           header->rootFreeTail == live.rootFreeTail &&
           header->rootUsedHead == live.rootUsedHead &&
           header->rootUsedTail == live.rootUsedTail &&
           header->systemHeap == live.systemHeap &&
           header->systemHeapStart == live.systemHeapStart &&
           header->systemHeapEnd == live.systemHeapEnd &&
           header->systemHeapSize == live.systemHeapSize &&
           header->currentHeap == live.currentHeap &&
           (allowGuardedVolumeDrift || header->missionMode == live.missionMode) &&
           header->mapArchive == live.mapArchive &&
           header->mapValue == live.mapValue &&
           header->sceneValue == live.sceneValue &&
           header->currentScene == live.currentScene &&
           (allowGuardedVolumeDrift || header->gameMode == live.gameMode) &&
           header->gameModeCount == live.gameModeCount &&
           header->heapMode == live.heapMode &&
           header->heapGroup == live.heapGroup &&
           header->systemHeapMode == live.systemHeapMode &&
           header->systemHeapGroup == live.systemHeapGroup &&
           header->systemFreeHead == live.systemFreeHead &&
           header->systemFreeTail == live.systemFreeTail &&
           header->systemUsedHead == live.systemUsedHead &&
           header->systemUsedTail == live.systemUsedTail &&
           header->currentHeapGroup == live.currentHeapGroup &&
           header->audioBasic == live.audioBasic &&
           header->audioScene == live.audioScene &&
           header->mainLoopMode == live.mainLoopMode &&
           header->mainLoopPendingScene == live.mainLoopPendingScene &&
           header->mainDrawState == live.mainDrawState &&
           (allowGuardedVolumeDrift || header->simpleModeler == live.simpleModeler) &&
           (allowGuardedVolumeDrift || header->mapCol == live.mapCol) &&
           (allowGuardedVolumeDrift || header->enTypesManager == live.enTypesManager) &&
           (allowGuardedVolumeDrift ||
            (header->volume[0] == live.volume[0] &&
             header->volume[1] == live.volume[1] &&
             header->volume[2] == live.volume[2]));
}

bool pointerInSavedHeap(u32 pointer, const SnapshotHeader *header) {
    return pointer >= header->heapStart && pointer < header->heapEnd &&
           (pointer & 3u) == 0u;
}

bool savedPointerCompatible(u32 saved, u32 current,
                            const SnapshotHeader *header) {
    // Heap-owned roots are rewound below. A root owned by the fixed system
    // heap is safe only when it is still exactly the same live object.
    return saved == 0u || pointerInSavedHeap(saved, header) ||
           saved == current;
}

bool actionIdentity(LiveIdentity *identity) {
    if (!buildIdentity(identity, true) || !ioIdle(true)) return false;
    if (sStableFrames < kRequiredStableFrames)
        return gateFailure(Gate::Stability, sStableFrames, true);
    if (!sameIdentity(*identity, sLastIdentity))
        return gateFailure(Gate::IdentityChanged, identity->heap, true);
    return true;
}

bool persistentProfileMatches(const SnapshotHeader *header, const LiveIdentity &live) {
    if (!sPersistentLoaded && !sSavedPersistentProfile.magic) return true;
    unsigned int fault = 0u, value = 0u;
    if (!sPersistentKeyReady || header->mapValue != 2u || header->sceneValue != 2u ||
        live.mapValue != 2u || live.sceneValue != 2u ||
        !LmAudioIdleValidate(nullptr, grainReadWord, &fault, &value) ||
        !LmPersistentCapture(nullptr, grainReadWord, live.rootHeap, live.systemHeap,
            sPersistentConfigId, header->generation, &sLivePersistentProfile, &fault, &value)) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xF9u, fault, value);
        return false;
    }
    if (!LmPersistentMatch(&sSavedPersistentProfile, &sLivePersistentProfile, &fault)) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_LOAD, 0xFAu, fault,
            fault < sizeof(sLivePersistentProfile) / 4u ?
                reinterpret_cast<const u32 *>(&sLivePersistentProfile)[fault] : 0u);
        return false;
    }
    return true;
}

void saveState() {
    clearRejectTelemetry();
    sCrossRoomGuard = kCrossRoomGuardNone;
    sCrossRoomFault = 0u;
    sCrossRoomFaultValue = 0u;
    clearEpochMismatch();
    clearVolumeDiff();
    clearResourceDiff();
    clearModelDiff();
    traceSavePhase(0x01u, sStableFrames);
    LiveIdentity preflight;
    if (!actionIdentity(&preflight)) {
        setReject(LMState::Status::Busy, sStableFrames);
        return;
    }
    if (!heapsHealthy(preflight)) {
        setReject(LMState::Status::BadHeap, preflight.heap);
        return;
    }
    if (!cameraObjectsValid(preflight, false)) {
        gateFailure(Gate::Cameras, kCameraObjectPointerTable, true);
        setReject(LMState::Status::Busy, kCameraObjectPointerTable);
        return;
    }

    // Drain prior live handles through LM's own scene-change path. Its new
    // bootstrap handle remains in uncaptured system audio state.
    traceSavePhase(0x20u, preflight.audioBasic);
    if (!quiesceAudio(preflight)) {
        setReject(LMState::Status::Busy, preflight.audioBasic);
        return;
    }
    traceSavePhase(0x21u, preflight.audioBasic);
    LiveIdentity before;
    if (!buildIdentity(&before, true) || !ioIdle(true) ||
        !gateCheck(heapsHealthy(before), Gate::HeapHealth, before.heap) ||
        !gateCheck(cameraObjectsValid(before, false), Gate::Cameras,
                   kCameraObjectPointerTable)) {
        setReject(LMState::Status::Busy, preflight.heap);
        return;
    }
    const u32 totalSize = kHeapDataOffset + before.heapSize;
    if (totalSize > LM_STATE_STORAGE_PAYLOAD_MAX - 0x2000u) {
        setReject(LMState::Status::TooLarge, totalSize);
        return;
    }

    traceSavePhase(0x40u, before.heap);
    const FreezeState freeze = freezeBegin();
    traceSavePhase(0x43u, before.heap);
    LiveIdentity live;
    if (!buildIdentity(&live, true) ||
        !gateCheck(sameIdentity(before, live), Gate::IdentityChanged, live.heap) ||
        !ioIdle(true) ||
        !gateCheck(cameraObjectsValid(live, false), Gate::Cameras,
                   kCameraObjectPointerTable)) {
        freezeEnd(freeze);
        setReject(LMState::Status::Busy, live.heap);
        return;
    }
    captureVolumeCensus(&sLiveVolumeCensus, live);
    captureResourceCensus(&sLiveResourceCensus, live);
    captureModelCensus(sLiveModelCensus);

    if (!grainStateValid(nullptr, live, SUSAMUNE_PHASE_ACTION_SAVE, 0x5Fu) ||
        !renderTargetsValid(nullptr, live)) {
        freezeEnd(freeze);
        setReject(LMState::Status::Busy, sGateValue);
        return;
    }
    // Compress the live parent while GAME and resource consumers are frozen.
    // Staging is disjoint from the previous state: capacity failure preserves it.
    LmSharedArchiveDescriptor shared;
    traceSavePhase(0x5Cu, 0x419B00u);
    const u32 sharedPacked = stageSharedArchive(totalSize, &shared);
    if (!sharedPacked) {
        freezeEnd(freeze);
        setReject(sCrossRoomFault ? LMState::Status::Busy : LMState::Status::TooLarge,
                  sCrossRoomFault ? sCrossRoomFaultValue : totalSize);
        return;
    }
    traceSavePhase(0x5Du, sharedPacked);
    unsigned int profileFault = 0u, profileValue = 0u;
    clearWords(&sLivePersistentProfile, sizeof(sLivePersistentProfile));
    if (sPersistentKeyReady && live.mapValue == 2u && live.sceneValue == 2u &&
        LmAudioIdleValidate(nullptr, grainReadWord, &profileFault, &profileValue)) {
        LmPersistentCapture(nullptr, grainReadWord, live.rootHeap, live.systemHeap,
            sPersistentConfigId, sGeneration + 1u, &sLivePersistentProfile,
            &profileFault, &profileValue);
    }
    traceSavePhase(0x60u, live.heap);
    SnapshotHeader *header =
        reinterpret_cast<SnapshotHeader *>(kSnapshotBase);
    header->magic = 0u;
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(header, 32u);
    asm volatile("sync" ::: "memory");

    clearWords(header, kHeapDataOffset);
    header->version = kSnapshotVersion;
    header->headerSize = kHeaderSize;
    header->gameId = SUSAMUNE_MOD_GAME_ID_LMJ;
    header->totalSize = totalSize;
    header->generation = ++sGeneration;
    header->heap = live.heap;
    header->heapStart = live.heapStart;
    header->heapEnd = live.heapEnd;
    header->heapSize = live.heapSize;
    header->heapMetadataOffset = kHeapMetadataOffset;
    header->heapMetadataSize = kHeapMetadataSize;
    header->stateStaticsOffset = kStateStaticsOffset;
    header->stateStaticsSize = kStateStaticsSize;
    header->heapDataOffset = kHeapDataOffset;
    header->heapDataSize = live.heapSize;
    header->rootHeap = live.rootHeap;
    header->rootHeapStart = live.rootHeapStart;
    header->rootHeapEnd = live.rootHeapEnd;
    header->rootHeapSize = live.rootHeapSize;
    header->rootHeapMode = live.rootHeapMode;
    header->rootHeapGroup = live.rootHeapGroup;
    header->rootFreeHead = live.rootFreeHead;
    header->rootFreeTail = live.rootFreeTail;
    header->rootUsedHead = live.rootUsedHead;
    header->rootUsedTail = live.rootUsedTail;
    header->systemHeap = live.systemHeap;
    header->systemHeapStart = live.systemHeapStart;
    header->systemHeapEnd = live.systemHeapEnd;
    header->systemHeapSize = live.systemHeapSize;
    header->currentHeap = live.currentHeap;
    header->missionMode = live.missionMode;
    header->mapArchive = live.mapArchive;
    header->volume[0] = live.volume[0];
    header->volume[1] = live.volume[1];
    header->volume[2] = live.volume[2];
    header->mapValue = live.mapValue;
    header->sceneValue = live.sceneValue;
    header->currentScene = live.currentScene;
    header->gameMode = live.gameMode;
    header->gameModeCount = live.gameModeCount;
    header->heapMode = live.heapMode;
    header->heapGroup = live.heapGroup;
    header->systemHeapMode = live.systemHeapMode;
    header->systemHeapGroup = live.systemHeapGroup;
    header->systemFreeHead = live.systemFreeHead;
    header->systemFreeTail = live.systemFreeTail;
    header->systemUsedHead = live.systemUsedHead;
    header->systemUsedTail = live.systemUsedTail;
    header->currentHeapGroup = live.currentHeapGroup;
    header->randomState = readWord(kRandomStateGlobal);
    header->freeHead = live.heapFreeHead;
    header->freeTail = live.heapFreeTail;
    header->usedHead = live.heapUsedHead;
    header->usedTail = live.heapUsedTail;
    header->mainLoopMode = live.mainLoopMode;
    header->mainLoopPendingScene = live.mainLoopPendingScene;
    header->mainDrawState = live.mainDrawState;
    header->simpleModeler = live.simpleModeler;
    header->mapCol = live.mapCol;
    header->enTypesManager = live.enTypesManager;
    header->audioBasic = live.audioBasic;
    header->audioScene = live.audioScene;

    copyWords(reinterpret_cast<void *>(kSnapshotBase + kHeapMetadataOffset),
              reinterpret_cast<void *>(live.heap + kHeapMetadataStart),
              kHeapMetadataSize);
    captureStaticRanges();
    captureCameraObjects(live);
    traceSavePhase(0x63u, kStateStaticsSize);
    copyWords(reinterpret_cast<void *>(kSnapshotBase + kHeapDataOffset),
              reinterpret_cast<void *>(live.heapStart), live.heapSize);
    traceSavePhase(0x64u, totalSize);
    header->checksum = snapshotChecksum(header);
    const u32 storedSize = commitSharedArchive(header, shared, sharedPacked);

    traceSavePhase(0x65u, storedSize);
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(header, storedSize);
    asm volatile("sync" ::: "memory");
    header->magic = kSnapshotMagic;
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(header, 32u);
    asm volatile("sync" ::: "memory");
    commitSavedVolumeCensus(header->generation);
    commitSavedResourceCensus(header->generation);
    commitSavedModelCensus(header->generation);
    copyWords(&sSavedPersistentProfile, &sLivePersistentProfile, sizeof(sSavedPersistentProfile));
    sPersistentLoaded = false;
    traceSavePhase(0x70u, live.heap);
    freezeEnd(freeze);

    sSnapshotSize = totalSize;
    savedSlotCommitted();
    ++sTimelineRevision;
    sStatus = LMState::Status::Saved;
    LMNotice::show(LM_POPUP_SAVED);
    if (sPostLoadTraceState == 3u) {
        // A successful save starts a fresh tail for the next door attempt.
        sPostLoadTraceFrame = kPostLoadTraceFrameLimit;
        sPostLoadTraceHeartbeat = 0u;
        sPostLoadTraceButtons = readHalf(kPadStatusGlobal);
        sPostLoadTraceBurst = false;
        sPostLoadTracePresentationBurst = false;
        sPostLoadTraceMovement = false;
        sPostLoadDoorWindow = 0u;
        sPostLoadTraceBurstUpdates = 0u;
        samplePostLoadTransitionWatch(&sPostLoadTransitionWatch);
    }
    traceSavePhase(0x7Fu, totalSize);
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_SAVE, 0xF9u, profileFault, profileValue);
    LMCrash::note(kEventStateSave, live.heap, live.heapSize);
}

void loadState() {
    clearRejectTelemetry();
    sCrossRoomGuard = kCrossRoomGuardNone;
    sCrossRoomFault = 0u;
    sCrossRoomFaultValue = 0u;
    clearEpochMismatch();
    clearVolumeDiff();
    clearResourceDiff();
    clearModelDiff();
    traceLoadPhase(0x01u, sStableFrames);
    SnapshotHeader *header =
        reinterpret_cast<SnapshotHeader *>(kSnapshotBase);
    traceLoadPhase(0x02u, kHeaderSize);
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(header,
                                                            kHeaderSize);
    if (header->magic != kSnapshotMagic) {
        setReject(LMState::Status::Empty, header->magic);
        return;
    }
    if (!basicHeaderValid(header)) {
        setReject(LMState::Status::BadCrc, header->version);
        return;
    }
    traceLoadPhase(0x03u, header->totalSize);
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(header,
                                                            header->totalSize);
    if (!basicHeaderValid(header) ||
        snapshotChecksum(header) != header->checksum) {
        setReject(LMState::Status::BadCrc, header->checksum);
        return;
    }
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        reinterpret_cast<void *>(kSnapshotBase + header->totalSize), 64u);
    const u32 storedSize = snapshotStoredSize(header);
    if (storedSize)
        reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
            reinterpret_cast<void *>(kSnapshotBase + header->totalSize),
            storedSize - header->totalSize);
    if (!snapshotCompanionValid(header, storedSize)) {
        setReject(LMState::Status::BadCrc, 0x53484152u);
        return;
    }
    traceLoadPhase(0x05u, header->checksum);

    LiveIdentity preflight;
    if (!actionIdentity(&preflight)) {
        setReject(LMState::Status::Busy, sStableFrames);
        return;
    }
    LmSharedArchiveDescriptor sharedBefore;
    if (!sharedArchiveMatchesSnapshot(header, &sharedBefore)) {
        rejectDirectEpoch(sCrossRoomFaultValue, header, preflight);
        return;
    }
    EpochMismatch mismatch;
    if (!grainStateValid(header, preflight, SUSAMUNE_PHASE_ACTION_LOAD, 0x07u)) {
        setReject(LMState::Status::BadHeap, sGateValue);
        return;
    }
    if (!renderTargetsValid(header, preflight)) {
        setReject(LMState::Status::BadHeap, sGateValue);
        return;
    }
    collectPreflightEpochMismatch(&mismatch, header, preflight);
    bool guardedCrossRoom = false;
    if (mismatch.mask != 0u) {
        guardedCrossRoom =
            guardedCrossRoomRestoreAllowed(header, preflight, mismatch);
        if (!guardedCrossRoom) {
            rejectEpoch(mismatch, header, preflight);
            return;
        }
        traceLoadPhase(0x06u, mismatch.mask);
    }
    const u32 liveCurrentScene = readWord(kCurrentSceneGlobal);
    const u32 liveGameMode = readWord(kGameModeGlobal);
    clearEpochMismatch(&mismatch);
    if (!savedPointerCompatible(header->currentScene, liveCurrentScene,
                                header)) {
        addEpochMismatch(&mismatch, SUSAMUNE_LM_EPOCH_CURRENT_SCENE,
                         header->currentScene, liveCurrentScene);
    }
    if (!savedPointerCompatible(header->gameMode, liveGameMode, header)) {
        addEpochMismatch(&mismatch, SUSAMUNE_LM_EPOCH_GAME_MODE,
                         header->gameMode, liveGameMode);
    }
    if (mismatch.mask != 0u) {
        rejectEpoch(mismatch, header, preflight);
        return;
    }
    if (!heapsHealthy(preflight)) {
        setReject(LMState::Status::BadHeap, preflight.heap);
        return;
    }
    if (!cameraObjectsValid(preflight, true)) {
        rejectDirectEpoch(kCameraObjectPointerTable, header, preflight);
        return;
    }

    traceLoadPhase(0x20u, preflight.audioBasic);
    if (!quiesceAudio(preflight)) {
        setReject(LMState::Status::Busy, preflight.audioBasic);
        return;
    }
    traceLoadPhase(0x21u, preflight.audioBasic);
    LiveIdentity before;
    if (!buildIdentity(&before) || !ioIdle() ||
        !cameraObjectsValid(before, true)) {
        rejectDirectEpoch(preflight.heap, header, preflight);
        return;
    }
    collectPreflightEpochMismatch(&mismatch, header, before);
    const bool beforeEpochAllowed =
        guardedCrossRoom
            ? guardedCrossRoomRestoreAllowed(header, before, mismatch)
            : mismatch.mask == 0u;
    if (!beforeEpochAllowed ||
        !headerMatchesLive(header, before, guardedCrossRoom)) {
        if (mismatch.mask != 0u) rejectEpoch(mismatch, header, before);
        else rejectDirectEpoch(preflight.heap, header, before);
        return;
    }
    if (!heapsHealthy(before)) {
        rejectDirectEpoch(preflight.heap, header, before);
        return;
    }
    if (!persistentProfileMatches(header, before)) {
        rejectDirectEpoch(LM_PERSISTENT_MAGIC, header, before);
        return;
    }

    traceLoadPhase(0x40u, before.heap);
    const FreezeState freeze = freezeBegin();
    traceLoadPhase(0x43u, before.heap);
    LiveIdentity live;
    const bool liveBuilt = buildIdentity(&live, true);
    if (liveBuilt) {
        collectPreflightEpochMismatch(&mismatch, header, live);
    }
    const bool liveEpochAllowed =
        liveBuilt &&
        (guardedCrossRoom
             ? guardedCrossRoomRestoreAllowed(header, live, mismatch)
             : mismatch.mask == 0u);
    LmSharedArchiveDescriptor sharedLive;
    const bool sharedAllowed = sharedArchiveMatchesSnapshot(header, &sharedLive) &&
        sharedDescriptorsEqual(sharedBefore, sharedLive);
    if (!liveBuilt ||
        !gateCheck(sameIdentity(before, live), Gate::IdentityChanged, live.heap) ||
        !ioIdle(true) ||
        !gateCheck(liveEpochAllowed && headerMatchesLive(header, live, guardedCrossRoom),
                   Gate::RestoreProof, live.heap) ||
        !gateCheck(sharedAllowed, Gate::RestoreProof, sCrossRoomFaultValue) ||
        !renderTargetsValid(nullptr, live) ||
        !gateCheck(persistentProfileMatches(header, live), Gate::RestoreProof,
                   LM_PERSISTENT_MAGIC) ||
        !gateCheck(cameraObjectsValid(live, true), Gate::Cameras,
                   kCameraObjectPointerTable)) {
        freezeEnd(freeze);
        setReject(LMState::Status::Busy, live.heap);
        return;
    }

    traceLoadPhase(0x5Cu, sharedLive.size);
    if (!restoreSharedArchive(header, sharedLive)) __builtin_trap();
    traceLoadPhase(0x5Du, sharedLive.size);
    traceLoadPhase(0x60u, live.heapSize);
    copyWords(reinterpret_cast<void *>(live.heapStart),
              reinterpret_cast<void *>(kSnapshotBase + kHeapDataOffset),
              live.heapSize);
    traceLoadPhase(0x61u, live.heapSize);
    copyWords(reinterpret_cast<void *>(live.heap + kHeapMetadataStart),
              reinterpret_cast<void *>(kSnapshotBase + kHeapMetadataOffset),
              kHeapMetadataSize);
    traceLoadPhase(0x62u, kHeapMetadataSize);

    // Restore game-owned statics but leave JAudio, SDK, and hardware-facing
    // queues live.  The guarded room path also rewinds the fixed model/room
    // owner tables and JKR volume anchors that refer into the game heap.
    restoreStaticRanges();
    restoreCameraObjects();
    writeWord(kRandomStateGlobal, header->randomState);
    traceLoadPhase(0x63u, kStateStaticsSize);

    if (guardedCrossRoom) {
        // Common system/root volumes are not part of the game-heap copy. Their
        // embedded list links still need to point back through the saved
        // game-owned prefix after the raw allocator rewind.
        repairSavedVolumeList(header);
        canonicalizeDvdTransport();
    }
    traceLoadPhase(0x64u, guardedCrossRoom ? header->volume[2] : 0u);

    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(live.heapStart), live.heapSize);
    traceLoadPhase(0x65u, live.heapSize);
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(live.heap + kHeapMetadataStart),
        kHeapMetadataSize);
    traceLoadPhase(0x66u, kHeapMetadataSize);
    storeStaticRanges();
    storeCameraObjects();
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(kRandomStateGlobal), sizeof(u32));
    traceLoadPhase(0x67u, kStateStaticsSize);
    traceLoadPhase(0x68u, live.heap);
    asm volatile("sync" ::: "memory");
    reinterpret_cast<VoidFn>(kGXInvalidateVtxCacheAddr)();
    traceLoadPhase(0x69u, kGXInvalidateVtxCacheAddr);
    reinterpret_cast<VoidFn>(kGXInvalidateTexAllAddr)();
    asm volatile("sync" ::: "memory");
    traceLoadPhase(0x6Au, kGXInvalidateTexAllAddr);

    // Source validation already ran before any writes. A failure NOW cannot
    // be returned as a harmless refusal: GAME has already been overwritten.
    // Leave the exact fault in the critical journal and enter the installed
    // program-exception dumper without resuming a known-corrupt renderer.
    // Do not repair lists or free objects from either side of the rewind.
    if (!grainStateValid(nullptr, live, SUSAMUNE_PHASE_ACTION_LOAD, 0x6Bu)) {
        __builtin_trap();
    }

    traceLoadPhase(0x70u, live.heap);
    freezeEnd(freeze, true);
    traceLoadPhase(0x74u, live.heap);

    traceLoadPhase(0x75u, live.rootHeap);
    bool healthyAfter = LMState::heapHealthy(live.rootHeap);
    traceLoadPhase(0x76u, healthyAfter ? 1u : 0u);
    if (healthyAfter) {
        traceLoadPhase(0x77u, live.systemHeap);
        healthyAfter = LMState::heapHealthy(live.systemHeap);
        traceLoadPhase(0x78u, healthyAfter ? 1u : 0u);
    }
    if (healthyAfter) {
        traceLoadPhase(0x79u, live.heap);
        healthyAfter = LMState::heapHealthy(live.heap);
        traceLoadPhase(0x7Au, healthyAfter ? 1u : 0u);
    }

    sSnapshotSize = header->totalSize;
    sStableFrames = 0u;
    if (!healthyAfter) {
        __builtin_trap();
    }
    LMRumble::afterLoad(live.heapStart, live.heapEnd, live.systemHeap,
                       live.systemHeapStart, live.systemHeapEnd);
    ++sTimelineRevision;
    ++sLoadRevision;
    sStatus = LMState::Status::Loaded;
    LMNotice::show(LM_POPUP_LOADED);
    sPostLoadTraceFrame = 0u;
    sPostLoadTraceBurst = false;
    sPostLoadTracePresentationBurst = false;
    sPostLoadTraceMovement = false;
    sPostLoadDoorWindow = 0u;
    sPostLoadTraceBurstUpdates = 0u;
    sPostLoadTraceState = 1u;
    traceLoadPhase(0x7Fu, header->totalSize);
    LMCrash::note(kEventStateLoad, live.heap, live.heapSize);
}

#include "lm_state_storage.inc"

void updateStability() {
    LiveIdentity live;
    if (!buildIdentity(&live, true) || !ioIdle(true)) {
        sHaveIdentity = false;
        sStableFrames = 0u;
        return;
    }
    if (sHaveIdentity && sameIdentity(live, sLastIdentity)) {
        if (sStableFrames < 999u) {
            ++sStableFrames;
        }
    } else {
        sLastIdentity = live;
        sHaveIdentity = true;
        sStableFrames = 1u;
    }
}

const VolumeDescriptor *volumeChangeEntry(u32 displayIndex, bool *added) {
    *added = false;
    if (!sVolumeDiff.ready || displayIndex >= kVolumeChangeRows) {
        return nullptr;
    }
    if (displayIndex < kVolumeDisplayedPerKind) {
        if (displayIndex >= sVolumeDiff.removedCount) {
            return nullptr;
        }
        const u32 index = sVolumeDiff.removedIndices[displayIndex];
        return index < sSavedVolumeCensus.count
                   ? &sSavedVolumeCensus.entries[index]
                   : nullptr;
    }
    const u32 addedIndex = displayIndex - kVolumeDisplayedPerKind;
    if (addedIndex < sVolumeDiff.addedCount &&
        addedIndex < kVolumeDisplayedPerKind) {
        const u32 index = sVolumeDiff.addedIndices[addedIndex];
        if (index < sLiveVolumeCensus.count) {
            *added = true;
            return &sLiveVolumeCensus.entries[index];
        }
    }
    return nullptr;
}

const char *volumeOwnerText(u32 owner) {
    switch (owner & kVolumeOwnerMask) {
    case kVolumeOwnerGame:
        return "G";
    case kVolumeOwnerSystem:
        return "S";
    case kVolumeOwnerRoot:
        return "R";
    default:
        return "?";
    }
}

}  // namespace

namespace LMState {

bool heapHealthy(u32 heap) {
    unsigned int fault = 0u, value = 0u;
    const bool interrupts = reinterpret_cast<DisableInterruptsFn>(kOSDisableInterruptsAddr)();
    const bool valid = LmExpHeapValidate(nullptr, grainReadWord, heap, &fault, &value) != 0;
    reinterpret_cast<RestoreInterruptsFn>(kOSRestoreInterruptsAddr)(interrupts);
    if (!valid) {
        LMCrash::note(kEventStateLoadPhase, 0xF0u, fault);
        LMCrash::note(kEventStateLoadPhase, 0xF1u, value);
        LMCrash::note(kEventStateLoadPhase, 0xF2u, heap);
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xF4u, fault, value);
    }
    return valid;
}

void postLoadMilestone(u32 phase) {
    if (phase == 0x8Bu && (sPostLoadTraceState == 1u || sPostLoadTraceState == 2u) &&
        readWord(kMainLoopModeGlobal) == 2u && readWord(kMainLoopExitGlobal) == 0u &&
        !heapHealthy(readWord(kGameHeapGlobal))) __builtin_trap();
    if (sRejectTelemetryCount != 0u) {
        return;
    }
    if (sPostLoadTraceState == 1u || sPostLoadTraceState == 2u) {
        tracePostLoadPhase(phase, sPostLoadTraceFrame);
        return;
    }
    if (sPostLoadTraceState != 3u) {
        return;
    }

    // Keep exact wrappers off until input or streaming activity makes the
    // next frame interesting; the ARM fsyncs every phase it observes. Retain
    // the flag through presentation so the loop tail cannot become a blind
    // gap after MAIN GAME update.
    if (phase == 0x8Du) {
        const bool carriedTrace = sPostLoadTracePresentationBurst;
        const u16 buttons = readHalf(kPadStatusGlobal);
        const u16 changedButtons = buttons ^ sPostLoadTraceButtons;
        const bool aEdge = (buttons & kButtonA) != 0u &&
                           (sPostLoadTraceButtons & kButtonA) == 0u;
        sPostLoadTraceButtons = buttons;
        const bool movement = padMovementActive();
        const bool movementEdge = movement && !sPostLoadTraceMovement;
        sPostLoadTraceMovement = movement;
        const bool transitionChanged =
            (aEdge || sPostLoadDoorWindow != 0u) &&
            refreshPostLoadTransitionWatch();
        if (aEdge) {
            sPostLoadDoorWindow = kPostLoadDoorWindowFrames;
            sPostLoadTraceHeartbeat = 0u;
            if (sPostLoadTraceBurstUpdates < kPostLoadInputBurstUpdates) {
                sPostLoadTraceBurstUpdates = kPostLoadInputBurstUpdates;
            }
        }
        if (movement || changedButtons != 0u) {
            sPostLoadTraceHeartbeat = 0u;
            if (sPostLoadTraceBurstUpdates < kPostLoadInputBurstUpdates) {
                sPostLoadTraceBurstUpdates = kPostLoadInputBurstUpdates;
            }
        }
        if (transitionChanged && sPostLoadDoorWindow != 0u &&
            sPostLoadTraceBurstUpdates <
                kPostLoadTransitionBurstUpdates) {
            sPostLoadTraceBurstUpdates =
                kPostLoadTransitionBurstUpdates;
        }
        sPostLoadTraceBurst = sPostLoadTraceBurstUpdates != 0u;
        sPostLoadTracePresentationBurst = sPostLoadTraceBurst;
        if (carriedTrace || sPostLoadTracePresentationBurst) {
            tracePostLoadPhase(phase, sPostLoadTraceFrame);
        }
        if (transitionChanged) {
            tracePostLoadTransitionChange();
        }
        if (movementEdge || changedButtons != 0u) {
            const u32 sticks =
                static_cast<u32>(readByte(kPadStatusGlobal +
                                          kPadStickXOffset)) <<
                    24 |
                static_cast<u32>(readByte(kPadStatusGlobal +
                                          kPadStickYOffset)) <<
                    16 |
                buttons;
            LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, 0xE6u,
                           sticks, sPostLoadTraceFrame);
        }
    } else if (phase == 0x8Eu) {
        const bool traced = sPostLoadTraceBurst;
        const bool transitionChanged =
            sPostLoadDoorWindow != 0u &&
            refreshPostLoadTransitionWatch();
        if (traced && sPostLoadTraceBurstUpdates != 0u) {
            --sPostLoadTraceBurstUpdates;
        }
        sPostLoadTraceBurst = false;
        if (transitionChanged) {
            sPostLoadTraceBurstUpdates =
                kPostLoadTransitionBurstUpdates;
        }
        sPostLoadTracePresentationBurst =
            sPostLoadTracePresentationBurst || traced || transitionChanged;
        if (sPostLoadTracePresentationBurst) {
            tracePostLoadPhase(phase, sPostLoadTraceFrame);
        }
        if (transitionChanged) {
            tracePostLoadTransitionChange();
        }
    } else if (sPostLoadTracePresentationBurst ||
               (phase >= 0x96u && phase <= 0x9Bu)) {
        tracePostLoadPhase(phase, sPostLoadTraceFrame);
    }
}

void postLoadDetail(u32 phase, u32 arg0, u32 arg1) {
    if (postLoadDetailEnabled()) {
        LMCrash::phase(SUSAMUNE_PHASE_ACTION_POST_LOAD, phase, arg0, arg1);
    }
}

bool postLoadDetailEnabled() {
    return sRejectTelemetryCount == 0u &&
           (sPostLoadTraceState == 1u || sPostLoadTraceState == 2u ||
           (sPostLoadTraceState == 3u &&
            sPostLoadTracePresentationBurst));
}

void presenterEnter() {
    if (sPostLoadTraceState == 1u) {
        if (sPostLoadTraceFrame >= kPostLoadTraceFrameLimit) {
            sPostLoadTraceState = 3u;
            sPostLoadTraceHeartbeat = 0u;
            sPostLoadTraceButtons = readHalf(kPadStatusGlobal);
            sPostLoadTraceBurst = false;
            sPostLoadTracePresentationBurst = false;
            sPostLoadTraceMovement = false;
            sPostLoadDoorWindow = 0u;
            sPostLoadTraceBurstUpdates = 0u;
            samplePostLoadTransitionWatch(&sPostLoadTransitionWatch);
            tracePostLoadPhase(0x87u, sPostLoadTraceFrame);
            return;
        }
        ++sPostLoadTraceFrame;
        tracePostLoadPhase(0x81u, sPostLoadTraceFrame);
        sPostLoadTraceState = 2u;
    } else if (sPostLoadTraceState == 3u) {
        if (sPostLoadTraceFrame >=
            kPostLoadTraceFrameLimit + kPostLoadTraceLingeringFrameLimit) {
            sPostLoadTraceState = 0u;
            sPostLoadTraceBurst = false;
            sPostLoadTracePresentationBurst = false;
            sPostLoadTraceMovement = false;
            sPostLoadDoorWindow = 0u;
            sPostLoadTraceBurstUpdates = 0u;
            return;
        }
        ++sPostLoadTraceFrame;
        if (sPostLoadTracePresentationBurst) {
            tracePostLoadPhase(0x81u, sPostLoadTraceFrame);
        }
        if (++sPostLoadTraceHeartbeat >= kPostLoadTraceHeartbeatFrames) {
            sPostLoadTraceHeartbeat = 0u;
            if (sPostLoadDoorWindow == 0u &&
                sPostLoadTraceBurstUpdates == 0u) {
                tracePostLoadPhase(0x87u, sPostLoadTraceFrame);
            }
        }
        if (sPostLoadDoorWindow != 0u) {
            --sPostLoadDoorWindow;
        }
    }
}

void presenterAfterSample() {
    if (sPostLoadTraceState == 2u ||
        (sPostLoadTraceState == 3u &&
         sPostLoadTracePresentationBurst)) {
        tracePostLoadPhase(0x82u, sPostLoadTraceFrame);
    }
}

void presenterAfterDrawDone() {
    if ((sPostLoadTraceState == 1u || sPostLoadTraceState == 2u) &&
        readWord(kMainLoopModeGlobal) == 2u && readWord(kMainLoopExitGlobal) == 0u &&
        !heapHealthy(readWord(kGameHeapGlobal))) __builtin_trap();
    if (sPostLoadTraceState == 2u ||
        (sPostLoadTraceState == 3u &&
         sPostLoadTracePresentationBurst)) {
        tracePostLoadPhase(0x83u, sPostLoadTraceFrame);
    }
}

void presenterAfterRetail() {
    if (sPostLoadTraceState == 2u ||
        (sPostLoadTraceState == 3u &&
         sPostLoadTracePresentationBurst)) {
        tracePostLoadPhase(0x84u, sPostLoadTraceFrame);
    }
}

void presenterBeforeTick() {
    if (sPostLoadTraceState == 2u ||
        (sPostLoadTraceState == 3u &&
         sPostLoadTracePresentationBurst)) {
        tracePostLoadPhase(0x85u, sPostLoadTraceFrame);
    }
}

void presenterAfterTick() {
    if (sPostLoadTraceState == 1u) {
        // The load returned at the true post-presenter transaction boundary.
        tracePostLoadPhase(0x80u, sPostLoadTraceFrame);
    } else if (sPostLoadTraceState == 2u) {
        tracePostLoadPhase(0x86u, sPostLoadTraceFrame);
        // Keep the final frame's following loop tail visible. The next
        // presenter entry retires tracing before a ninth frame is recorded.
        sPostLoadTraceState = 1u;
    } else if (sPostLoadTraceState == 3u &&
               sPostLoadTracePresentationBurst) {
        tracePostLoadPhase(0x86u, sPostLoadTraceFrame);
    }
}

void samplePad(const PADStatus &pad, bool available) {
    const bool edge = available &&
        (LmStateDirectionEdge(pad.mButton, sHotkeys.previous, 1u, pad.mCurError == 0u) ||
         LmStateDirectionEdge(pad.mButton, sHotkeys.previous, 2u, pad.mCurError == 0u));
    LmStateSampleHotkeys(&sHotkeys, pad.mButton, pad.mCurError == 0u, available);
    if (LmStatusPopupGameplayEvent(LMPractice::isOpen(), edge))
        LMNotice::show(storageInFlight() || sMenuRequest ? LM_POPUP_BUSY :
        sHotkeys.pending == 1u ? LM_POPUP_SAVING : LM_POPUP_LOADING);
}

u32 hotkeyDebug(u32 field) {
    switch (field) {
    case 0u: return sHotkeys.lastButtons;
    case 1u: return sHotkeys.lastConnected;
    case 2u: return sHotkeys.lastLatched;
    case 3u: return sHotkeys.lastDisposition;
    default: return 0u;
    }
}

void tick(bool allowRequests) {
    initializeSlot();
    if (!sSlotInitialized) return;
    serviceStorage();
    updateStability();
    serviceRejectTelemetry();
    const u32 pendingHotkey = sHotkeys.pending;
    const u32 hotkey = LmStateConsumeHotkey(&sHotkeys, allowRequests && !storageInFlight());
    if (LmStatusPopupGameplayEvent(LMPractice::isOpen(), pendingHotkey && !hotkey))
        LMNotice::show(LM_POPUP_BUSY);

    if (storageInFlight()) {
        return;
    }
    const u32 request = sMenuRequest;
    sMenuRequest = 0u;
    if (request == 1u || hotkey == 1u) {
        LMNotice::show(LM_POPUP_SAVING);
        LMNotice::present();
        saveState();
    } else if (request == 2u || hotkey == 2u) {
        LMNotice::show(LM_POPUP_LOADING);
        LMNotice::present();
        loadState();
    }
}

Status status() {
    return sStatus;
}

const char *statusText() {
    switch (sStatus) {
    case Status::Empty:
        return "EMPTY";
    case Status::Saved:
        return "SAVED";
    case Status::Loaded:
        return "LOADED";
    case Status::Busy:
        return "BUSY";
    case Status::BadCrc:
        return "BADCRC";
    case Status::BadHeap:
        return "BADHEAP";
    case Status::Epoch:
        return "EPOCH";
    case Status::TooLarge:
        return "TOOBIG";
    }
    return "UNKNOWN";
}

u32 snapshotKiB() {
    return sSnapshotSize >> 10;
}

u32 stableFrames() {
    return sStableFrames;
}

bool readyForAction() {
    return !storageInFlight() && sMenuRequest == 0u &&
        sGate == Gate::Ready && sStableFrames >= kRequiredStableFrames;
}

bool readyForActionNow() {
    if (!readyForAction()) return false;
    LiveIdentity live;
    return actionIdentity(&live);
}

bool requestSave() {
    if (storageInFlight() || sMenuRequest) { LMNotice::show(LM_POPUP_BUSY); return false; }
    LMNotice::show(LM_POPUP_SAVING);
    sMenuRequest = 1u; return true;
}
bool requestLoad() {
    if (storageInFlight() || sMenuRequest) { LMNotice::show(LM_POPUP_BUSY); return false; }
    LMNotice::show(LM_POPUP_LOADING);
    sMenuRequest = 2u; return true;
}
u32 slotCount() { return kResidentSlots; }
u32 selectedSlot() { return sSelectedSlot; }
bool selectSlot(u32 slot) { return switchSlot(slot); }
bool clearSelectedSlot() {
    if (storageInFlight() || sMenuRequest) return false;
    sSlots[sSelectedSlot].rawSize = 0u;
    sPersistentLoaded = false;
    clearWords(&sSavedPersistentProfile, sizeof(sSavedPersistentProfile));
    writeWord(kSnapshotBase, 0u);
    flushRawSlot(32u);
    sSnapshotSize = 0u;
    sStatus = Status::Empty;
    clearSlotDiagnostics();
    sStorageText = "SLOT CLEARED";
    return true;
}
bool slotHasState(u32 slot) { return slot < kResidentSlots && sSlots[slot].rawSize != 0u; }
u32 slotKiB(u32 slot) { return slot < kResidentSlots ? sSlots[slot].rawSize >> 10 : 0u; }
bool requestExport(const char *name) {
    return beginStorage(LM_STATE_STORAGE_EXPORT,
        sArchiveId < LM_STATE_STORAGE_MAX_ID ? sArchiveId + 1u : 1u, name);
}
bool requestImport(u32 archiveId) { return beginStorage(LM_STATE_STORAGE_IMPORT, archiveId); }
bool requestRename(u32 archiveId, const char *name) {
    return beginStorage(LM_STATE_STORAGE_RENAME, archiveId, name);
}
u32 catalogDeleteToken(u32 index) { return deleteToken(index); }
bool requestDelete(u32 archiveId, u32 token) {
    return beginStorage(LM_STATE_STORAGE_DELETE, archiveId, nullptr, token);
}
bool requestCatalog(u32 afterId) { return beginStorage(LM_STATE_STORAGE_CATALOG, afterId); }
bool catalogBusy() { return sStorageCommand == LM_STATE_STORAGE_CATALOG; }
u32 catalogCount() { return sCatalogCount; }
u32 catalogCursor() { return sCatalogCursor; }
u32 catalogNextCursor() { return sCatalogNext; }
bool catalogHasMore() { return sCatalogMore; }
u32 catalogId(u32 index) { return index < sCatalogCount ? sCatalog[index].id : 0u; }
u32 catalogBytes(u32 index) { return index < sCatalogCount ? sCatalog[index].bytes : 0u; }
bool catalogCompatible(u32 index) { return catalogEntryCompatible(index); }
const char *catalogEntryText(u32 index) { return catalogEntryStatus(index); }
const char *catalogName(u32 index) { return index < sCatalogCount ? sCatalog[index].name : ""; }
const char *catalogText() { return sCatalogText; }
bool storageBusy() { return storageInFlight(); }
const char *storageText() { return sStorageText; }
u32 packedNeededKiB() { return (sPackedNeeded + 1023u) >> 10; }
u32 packedCacheFreeKiB() { return (sSlotCacheSize - sCacheUsed) >> 10; }
u32 packStagingKiB() { return (sStaging[0].size + sStaging[1].size) >> 10; }
u32 lastArchiveId() { return sArchiveId; }
u32 timelineRevision() { return sTimelineRevision; }
u32 loadRevision() { return sLoadRevision; }

const char *gateText() {
    // BUSY keeps the failed action's reason instead of next frame's idle gate.
    switch (sStatus == Status::Busy ? sRejectedGate : sGate) {
    case Gate::Ready:
        return "OK";
    case Gate::Boot:
        return "BOOT";
    case Gate::GameHeap:
        return "GAME";
    case Gate::RootHeap:
        return "ROOT";
    case Gate::SystemHeap:
        return "SYS";
    case Gate::Size:
        return "SIZE";
    case Gate::Distinct:
        return "DIST";
    case Gate::SystemNest:
        return "SNEST";
    case Gate::GameNest:
        return "GNEST";
    case Gate::Overlap:
        return "OVER";
    case Gate::CurrentHeap:
        return "CUR";
    case Gate::MissionNull:
        return "MNUL";
    case Gate::MissionRange:
        return "MRNG";
    case Gate::ModeMismatch:
        return "MODE";
    case Gate::ModeCount:
        return "MCNT";
    case Gate::GameRoot:
        return "GROOT";
    case Gate::Particle:
        return "PTCL";
    case Gate::Grain:
        return "GRAIN";
    case Gate::DoorOwner:
        return "DPLAYER";
    case Gate::Door:
        return "DOOR";
    case Gate::RoomName:
        return "RNAME";
    case Gate::Scene:
        return "SCENE";
    case Gate::LoopMode:
        return "LOOP";
    case Gate::LoopExit:
        return "EXIT";
    case Gate::LoopScene:
        return "PEND";
    case Gate::DrawState:
        return "DRAW";
    case Gate::DvdPredicate:
        return "DVDP";
    case Gate::DvdCount:
        return "DVDC";
    case Gate::DvdPrimary:
        return "DVD1";
    case Gate::DvdSecondary:
        return "DVD2";
    case Gate::Aram0:
        return "AR0";
    case Gate::Aram1:
        return "AR1";
    case Gate::Card0:
        return "CARD0";
    case Gate::Card1:
        return "CARD1";
    case Gate::Audio:
        return "AUDIO";
    case Gate::Stability:
        return "STABLE";
    case Gate::IdentityChanged:
        return "CHANGE";
    case Gate::Cameras:
        return "CAMERA";
    case Gate::HeapHealth:
        return "HEALTH";
    case Gate::RestoreProof:
        return "PROOF";
    }
    return "?";
}

u32 gateValue() {
    return sStatus == Status::Busy ? sRejectedGateValue : sGateValue;
}

u32 crossRoomGuardCode() {
    return sCrossRoomGuard;
}

const char *epochText() {
    const u32 mask = sEpochMismatch.mask;
    if (mask & SUSAMUNE_LM_EPOCH_MAP_VALUE)
        return "MAPV";
    if (mask & SUSAMUNE_LM_EPOCH_SCENE_VALUE)
        return "SCNV";
    if (mask & SUSAMUNE_LM_EPOCH_CURRENT_SCENE)
        return "SCNP";
    if (mask & SUSAMUNE_LM_EPOCH_PENDING_SCENE)
        return "PEND";
    if (mask & SUSAMUNE_LM_EPOCH_LOOP_MODE)
        return "LOOP";
    if (mask & SUSAMUNE_LM_EPOCH_AUDIO_SCENE)
        return "AUDS";
    if (mask & SUSAMUNE_LM_EPOCH_MAP_ARCHIVE)
        return "MARC";
    if (mask & SUSAMUNE_LM_EPOCH_VOLUME_COUNT)
        return "VOLN";
    if (mask & SUSAMUNE_LM_EPOCH_VOLUME_HEAD)
        return "VOLH";
    if (mask & SUSAMUNE_LM_EPOCH_VOLUME_TAIL)
        return "VOLT";
    if (mask & SUSAMUNE_LM_EPOCH_MISSION_MODE)
        return "MISS";
    if (mask & SUSAMUNE_LM_EPOCH_GAME_MODE)
        return "GMOD";
    if (mask & SUSAMUNE_LM_EPOCH_SIMPLE_MODELER)
        return "SIMP";
    if (mask & SUSAMUNE_LM_EPOCH_MAP_COL)
        return "MCOL";
    if (mask & SUSAMUNE_LM_EPOCH_EN_TYPES)
        return "ENTY";
    if (mask & SUSAMUNE_LM_EPOCH_GAME_HEAP)
        return "HEAP";
    if (mask & SUSAMUNE_LM_EPOCH_GAME_HEAP_START)
        return "HBEG";
    if (mask & SUSAMUNE_LM_EPOCH_GAME_HEAP_END)
        return "HEND";
    if (mask & SUSAMUNE_LM_EPOCH_ROOT_HEAP)
        return "ROOT";
    if (mask & SUSAMUNE_LM_EPOCH_SYSTEM_HEAP)
        return "SYSP";
    if (mask & SUSAMUNE_LM_EPOCH_AUDIO_BASIC)
        return "AUDO";
    if (mask & SUSAMUNE_LM_EPOCH_DRAW_STATE)
        return "DRAW";
    return "NONE";
}

u32 epochMask() {
    return sEpochMismatch.mask;
}

u32 epochSaved() {
    return sEpochMismatch.saved;
}

u32 epochLive() {
    return sEpochMismatch.live;
}

const char *volumeTopologyText() {
    if (!sVolumeDiff.ready) return "WAIT";
    if (!sVolumeDiff.savedValid) return "SBAD";
    if (!sVolumeDiff.liveValid) return "LBAD";
    if (sVolumeDiff.removedCount == 0u && sVolumeDiff.addedCount == 0u) {
        return sVolumeDiff.commonOrder ? "SAME" : "MIX";
    }
    if (sVolumeDiff.headOnly) {
        if (sVolumeDiff.removedCount == 1u) return "HEAD1";
        if (sVolumeDiff.removedCount == 2u) return "HEAD2";
        return "HEADN";
    }
    return sVolumeDiff.commonOrder ? "ORDER" : "MIX";
}

u32 volumeSavedCount() {
    return sVolumeDiff.savedCount;
}

u32 volumeLiveCount() {
    return sVolumeDiff.liveCount;
}

u32 volumeRemovedCount() {
    return sVolumeDiff.removedCount;
}

u32 volumeAddedCount() {
    return sVolumeDiff.addedCount;
}

u32 volumeSavedFault() {
    return sVolumeDiff.ready ? sSavedVolumeCensus.fault : 0u;
}

u32 volumeLiveFault() {
    return sVolumeDiff.ready ? sLiveVolumeCensus.fault : 0u;
}

u32 volumeSavedCurrent() {
    return sVolumeDiff.ready ? sSavedVolumeCensus.currentVolume : 0u;
}

u32 volumeLiveCurrent() {
    return sVolumeDiff.ready ? sLiveVolumeCensus.currentVolume : 0u;
}

u32 volumeSavedDir() {
    return sVolumeDiff.ready ? sSavedVolumeCensus.currentDirId : 0u;
}

u32 volumeLiveDir() {
    return sVolumeDiff.ready ? sLiveVolumeCensus.currentDirId : 0u;
}

const char *volumeChangeKind(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    return entry ? (added ? "+" : "-") : " ";
}

const char *volumeChangeName(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    return entry && (entry->stateFlags & kVolumeNameValid) != 0u
               ? entry->name
               : "--";
}

const char *volumeChangeObjectOwnerText(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    if (!entry) return "?";
    u32 owner = entry->ownerFlags >> kVolumeObjectOwnerShift;
    owner &= kVolumeOwnerMask;
    if (owner == kVolumeOwnerOther) {
        owner = entry->ownerFlags >> kVolumeObjectLocationShift;
    }
    return volumeOwnerText(owner);
}

const char *volumeChangeBackingOwnerText(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    if (!entry) return "?";
    const u32 owner =
        (entry->ownerFlags >> kVolumeBackingLocationShift) & kVolumeOwnerMask;
    return volumeOwnerText(owner);
}

u32 volumeChangeObject(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    return entry ? entry->object : 0u;
}

u32 volumeChangeArchive(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    return entry ? entry->archiveHeader : 0u;
}

u32 volumeChangeBytes(u32 index) {
    bool added;
    const VolumeDescriptor *entry = volumeChangeEntry(index, &added);
    return entry ? entry->fileLength : 0u;
}

u32 volumeObjectReuseMask() {
    return sVolumeDiff.ready ? sVolumeDiff.objectReuseMask : 0u;
}

u32 volumeArchiveReuseMask() {
    return sVolumeDiff.ready ? sVolumeDiff.archiveReuseMask : 0u;
}

u32 resourceSavedFault() {
    return sResourceDiff.ready ? sSavedResourceCensus.fault : 0u;
}

u32 resourceLiveFault() {
    return sResourceDiff.ready ? sLiveResourceCensus.fault : 0u;
}

u32 resourceActiveMismatchMask() {
    return sResourceDiff.ready ? sResourceDiff.activeMismatchMask : 0u;
}

u32 resourceRecordMismatchMask() {
    return sResourceDiff.ready ? sResourceDiff.recordMismatchMask : 0u;
}

u32 resourceLayoutChanged() {
    return sResourceDiff.ready ? sResourceDiff.layoutChanged : 0u;
}

u32 resourceMapChanged() {
    return sResourceDiff.ready ? sResourceDiff.mapChanged : 0u;
}

u32 resourceSavedBackingBadMask() {
    return sResourceDiff.ready ? sSavedResourceCensus.backingBadMask : 0u;
}

u32 resourceLiveBackingBadMask() {
    return sResourceDiff.ready ? sLiveResourceCensus.backingBadMask : 0u;
}

u32 resourceSavedMarkMask() {
    return sResourceDiff.ready ? sSavedResourceCensus.markMask : 0u;
}

u32 resourceLiveMarkMask() {
    return sResourceDiff.ready ? sLiveResourceCensus.markMask : 0u;
}

u32 resourceSavedWantedCount() {
    return sResourceDiff.ready ? sSavedResourceCensus.wantedCount : 0u;
}

u32 resourceLiveWantedCount() {
    return sResourceDiff.ready ? sLiveResourceCensus.wantedCount : 0u;
}

u32 resourceWantedRemovedCount() {
    return sResourceDiff.ready ? sResourceDiff.wantedRemovedCount : 0u;
}

u32 resourceWantedAddedCount() {
    return sResourceDiff.ready ? sResourceDiff.wantedAddedCount : 0u;
}

u32 resourceWantedSequenceChanged() {
    return sResourceDiff.ready ? sResourceDiff.wantedSequenceChanged : 0u;
}

u32 resourceActiveChangeSlot(u32 index) {
    if (!sResourceDiff.ready) return 0xFFu;
    for (u32 slot = 0; slot < kResourceSlotCount; ++slot) {
        if ((sResourceDiff.activeMismatchMask & (1u << slot)) != 0u) {
            if (index == 0u) return slot;
            --index;
        }
    }
    return 0xFFu;
}

u32 resourceActiveSavedId(u32 index) {
    const u32 slot = resourceActiveChangeSlot(index);
    return slot < kResourceSlotCount
               ? sSavedResourceCensus.activeIds[slot]
               : 0xFFFFFFFFu;
}

u32 resourceActiveLiveId(u32 index) {
    const u32 slot = resourceActiveChangeSlot(index);
    return slot < kResourceSlotCount
               ? sLiveResourceCensus.activeIds[slot]
               : 0xFFFFFFFFu;
}

u32 resourceWantedRemovedId(u32 index) {
    return sResourceDiff.ready && index < 2u &&
                   index < sResourceDiff.wantedRemovedCount
               ? sResourceDiff.wantedRemovedIds[index]
               : 0xFFFFFFFFu;
}

u32 resourceWantedAddedId(u32 index) {
    return sResourceDiff.ready && index < 2u &&
                   index < sResourceDiff.wantedAddedCount
               ? sResourceDiff.wantedAddedIds[index]
               : 0xFFFFFFFFu;
}

u32 modelSavedFault() {
    return sModelDiff.ready ? sSavedModelCensus.metadata->fault : 0u;
}

u32 modelLiveFault() {
    return sModelDiff.ready ? sLiveModelCensus.metadata->fault : 0u;
}

u32 modelSavedSignature() {
    return sModelDiff.ready ? sSavedModelCensus.metadata->signature : 0u;
}

u32 modelLiveSignature() {
    return sModelDiff.ready ? sLiveModelCensus.metadata->signature : 0u;
}

u32 modelSavedRegistrySignature() {
    return sModelDiff.ready
               ? sSavedModelCensus.metadata->registrySignature
               : 0u;
}

u32 modelLiveRegistrySignature() {
    return sModelDiff.ready
               ? sLiveModelCensus.metadata->registrySignature
               : 0u;
}

u32 modelChangedCount() {
    return sModelDiff.ready ? sModelDiff.changedCount : 0u;
}

u32 modelChangeIndex(u32 index) {
    return sModelDiff.ready && index < kModelChangeSlots &&
                   index < sModelDiff.changedCount
               ? sModelDiff.changes[index].index
               : 0xFFFFFFFFu;
}

const char *modelChangeKind(u32 index) {
    if (modelChangeIndex(index) >= kModelEntryCount) return "?";
    switch (sModelDiff.changes[index].sourceMask) {
    case kModelChangedPrimary:
        return "P";
    case kModelChangedRegistry:
        return "R";
    case kModelChangedPrimary | kModelChangedRegistry:
        return "B";
    default:
        return "?";
    }
}

const char *modelChangeName(u32 index) {
    return sModelDiff.ready && index < kModelChangeSlots &&
                   index < sModelDiff.changedCount
               ? sModelDiff.changes[index].name
               : "--";
}

u32 modelChangeSavedState(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].savedState
               : 0u;
}

u32 modelChangeLiveState(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].liveState
               : 0u;
}

u32 modelChangeSavedRoot(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].savedRoot
               : 0u;
}

u32 modelChangeLiveRoot(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].liveRoot
               : 0u;
}

u32 modelChangeSavedHandle(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].savedHandle
               : 0u;
}

u32 modelChangeLiveHandle(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].liveHandle
               : 0u;
}

u32 modelChangeSavedRegistrySignature(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].savedRegistrySignature
               : 0u;
}

u32 modelChangeLiveRegistrySignature(u32 index) {
    return modelChangeIndex(index) < kModelEntryCount
               ? sModelDiff.changes[index].liveRegistrySignature
               : 0u;
}

}  // namespace LMState

#endif  // defined(SUSAMUNE_VERSION_LMJ)
