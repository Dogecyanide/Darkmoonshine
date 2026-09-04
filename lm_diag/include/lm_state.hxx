#ifndef LM_DIAG_STATE_HXX
#define LM_DIAG_STATE_HXX

#include "Dolphin/types.h"

namespace LMState {

enum class Status : u32 {
    Empty = 0,
    Saved,
    Loaded,
    Busy,
    BadCrc,
    BadHeap,
    Epoch,
    TooLarge,
};

// Polls port 1, updates the stability gate, and services one edge-triggered
// snapshot request. Call only after LM's complete retail presenter returns.
void tick();

// These keep an eight-frame detailed trace after a successful load, then a
// low-rate tail with a bounded door/streaming watch. Transition edges re-arm
// exact update-call tracing without journaling every quiet frame.
void postLoadMilestone(u32 phase);
void postLoadDetail(u32 phase, u32 arg0, u32 arg1);
void presenterEnter();
void presenterAfterSample();
void presenterAfterDrawDone();
void presenterAfterRetail();
void presenterBeforeTick();
void presenterAfterTick();

Status status();
const char *statusText();
u32 snapshotKiB();
u32 stableFrames();
const char *gateText();
u32 gateValue();
u32 crossRoomGuardCode();
const char *epochText();
u32 epochMask();
u32 epochSaved();
u32 epochLive();
const char *volumeTopologyText();
u32 volumeSavedCount();
u32 volumeLiveCount();
u32 volumeRemovedCount();
u32 volumeAddedCount();
u32 volumeSavedFault();
u32 volumeLiveFault();
u32 volumeSavedCurrent();
u32 volumeLiveCurrent();
u32 volumeSavedDir();
u32 volumeLiveDir();
const char *volumeChangeKind(u32 index);
const char *volumeChangeName(u32 index);
const char *volumeChangeObjectOwnerText(u32 index);
const char *volumeChangeBackingOwnerText(u32 index);
u32 volumeChangeObject(u32 index);
u32 volumeChangeArchive(u32 index);
u32 volumeChangeBytes(u32 index);
u32 volumeObjectReuseMask();
u32 volumeArchiveReuseMask();
u32 resourceSavedFault();
u32 resourceLiveFault();
u32 resourceActiveMismatchMask();
u32 resourceRecordMismatchMask();
u32 resourceLayoutChanged();
u32 resourceMapChanged();
u32 resourceSavedBackingBadMask();
u32 resourceLiveBackingBadMask();
u32 resourceSavedMarkMask();
u32 resourceLiveMarkMask();
u32 resourceSavedWantedCount();
u32 resourceLiveWantedCount();
u32 resourceWantedRemovedCount();
u32 resourceWantedAddedCount();
u32 resourceWantedSequenceChanged();
u32 resourceActiveChangeSlot(u32 index);
u32 resourceActiveSavedId(u32 index);
u32 resourceActiveLiveId(u32 index);
u32 resourceWantedRemovedId(u32 index);
u32 resourceWantedAddedId(u32 index);
u32 modelSavedFault();
u32 modelLiveFault();
u32 modelSavedSignature();
u32 modelLiveSignature();
u32 modelSavedRegistrySignature();
u32 modelLiveRegistrySignature();
u32 modelChangedCount();
u32 modelChangeIndex(u32 index);
const char *modelChangeKind(u32 index);
const char *modelChangeName(u32 index);
u32 modelChangeSavedState(u32 index);
u32 modelChangeLiveState(u32 index);
u32 modelChangeSavedRoot(u32 index);
u32 modelChangeLiveRoot(u32 index);
u32 modelChangeSavedHandle(u32 index);
u32 modelChangeLiveHandle(u32 index);
u32 modelChangeSavedRegistrySignature(u32 index);
u32 modelChangeLiveRegistrySignature(u32 index);

}  // namespace LMState

#endif  // LM_DIAG_STATE_HXX
