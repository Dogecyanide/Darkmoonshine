#if defined(SUSAMUNE_VERSION_LMJ)

#include "Dolphin/types.h"
#include "lm_crash.hxx"
#include "lm_practice.hxx"
#include "lm_state.hxx"
#include "lm_tools.hxx"
#include "lm_warp.hxx"
#include "lm_colour.hxx"
#include "lm_preferences.hxx"
#include "lm_notice.hxx"
#include "susamune/mod_bin.h"

namespace {

// GLMJ01 retail addresses.  Keeping every call explicit makes this payload
// independent of the inherited Sunshine symbol maps and C++ object graph.
const u32 kOSArenaLoAddr = 0x804A0A18u;

const u32 kLMRootHeapAddr = 0x804A0B90u;
const u32 kLMSystemHeapAddr = 0x804A0B94u;
const u32 kLMGameHeapAddr = 0x804A0B98u;
const u32 kDirectPrintPtrAddr = 0x804A2088u;
const u32 kLMDoubleBufferAddr = 0x804A0BBAu;

const u32 kLMFrameBeginAddr = 0x800076D8u;
const u32 kLMChangeFrameBufferAddr = 0x800077E8u;
const u32 kLMConditionalTailAddr = 0x80007FB4u;
const u32 kLMGameLoopAddr = 0x8000B4E8u;
const u32 kLMLoopTailClockAddr = 0x80005E04u;
const u32 kLMLoopTailSyncAddr = 0x8000B378u;
const u32 kLMOuterCleanupAddr = 0x8000AC78u;
const u32 kLMOuterRestartAddr = 0x80006070u;
const u32 kLMPreMainUpdateAddr = 0x8000ACA4u;
const u32 kLMPostMainUpdateAddr = 0x80008004u;
const u32 kLMTransitionStateAddr = 0x803985D4u;
const u32 kLMMainSceneStepAddr = 0x8000B248u;
const u32 kLMMainDrawStateAddr = 0x804A0C44u;
const u32 kLMAnimatedModelPoolGlobal = 0x804A0E48u;
const u32 kLMAnimatedModelControllerPoolGlobal = 0x804A0E4Cu;
const u32 kLMAnimatedModelPoolUpdateAddr = 0x80026750u;
const u32 kLMAnimatedModelControllerUpdateAddr = 0x8001EA84u;
const u32 kLMDefaultOrthoViewAddr = 0x800078FCu;
const u32 kGXCopyDispAddr = 0x801F045Cu;
const u32 kGXDrawDoneAddr = 0x801EF5F0u;
const u32 kGXLoadPosMtxImmAddr = 0x801F4C30u;
const u32 kGXLoadNrmMtxImmAddr = 0x801F4C6Cu;
const u32 kDCInvalidateRangeAddr = 0x801D5DF4u;
const u32 kDCFlushRangeAddr = 0x801D5E24u;
const u32 kDirectPrintEraseAddr = 0x801D4294u;
const u32 kDirectPrintChangeFrameBufferAddr = 0x801D4830u;
const u32 kDirectPrintDrawStringAddr = 0x801D49F8u;
const u32 kExpHeapVtable = 0x8038886Cu;

const u32 kModEnd = SUSAMUNE_MOD_BASE_LMJ + SUSAMUNE_MOD_REGION_SIZE;
const u32 kCanaryAddr = kModEnd - 0x10u;
const u32 kMem1Start = 0x80000000u;
const u32 kMem1End = 0x81800000u;
const u32 kXfbWidth = 640u;
const u32 kXfbHeight = 480u;
const u32 kXfbRowBytes = kXfbWidth * 2u;
const u32 kXfbSize = kXfbRowBytes * kXfbHeight;
const u16 kPopupLeft = 12u, kPopupTop = 16u;
const u16 kPopupWidth = 56u, kPopupHeight = 11u;
const u32 kAnimatedModelSlotCount = 80u;
const u32 kAnimatedModelSlotSize = 0x11Cu;
const u32 kAnimatedModelControllerSize = 0x318u;
const u32 kAnimatedModelPrimaryCapacity = 16u;
const u32 kAnimatedModelSecondaryCapacity = 10u;
const u32 kCanary[4] = {
    0x474C4D4Au,  // GLMJ
    0x4D454D31u,  // MEM1
    0x43414E31u,  // CAN1
    0x43414E32u,  // CAN2
};

static_assert(SUSAMUNE_MOD_REGION_SIZE == 0x80000u,
              "GLMJ diagnostic expects Moonshine's 512 KiB window");
static_assert(SUSAMUNE_ARENA_RESERVE_SIZE == 0x82000u,
              "GLMJ arena reserve must include the retail debug stack");
static_assert(kCanaryAddr >=
                  SUSAMUNE_MOD_BASE_LMJ + SUSAMUNE_MOD_SCRATCH_OFFSET,
              "diagnostic canary must stay in the reserved scratch tail");
static_assert(kCanaryAddr + sizeof(kCanary) <= kModEnd,
              "diagnostic canary exceeds the reserved mod window");
static_assert((kPopupTop + kPopupHeight) * 2u <= kXfbHeight &&
              (kPopupLeft + kPopupWidth) * 2u <= kXfbWidth,
              "status popup exceeds the framebuffer");

typedef void (*VoidFn)();
typedef void (*VoidPtrFn)(void *);
typedef void (*VoidU32Fn)(u32);
typedef f32 (*MatrixPtr)[4];
typedef void (*GXLoadMtxFn)(MatrixPtr, u32);
typedef void (*GXCopyDispFn)(void *, bool);
typedef void (*CacheRangeFn)(void *, u32);
typedef void (*DirectPrintEraseFn)(void *, u16, u16, u16, u16);
typedef void (*DirectPrintChangeFrameBufferFn)(void *, void *, u16, u16);
typedef void (*DirectPrintDrawStringFn)(void *, u16, u16, const char *, ...);
typedef void (*RetailCall4Fn)(u32, u32, u32, u32);

struct HeapSample {
    u32 pointer;
    bool valid;
};

u32 sFrames;
bool sFloorObserved;
bool sFloorOk;
bool sCanaryReady;
bool sCanaryOk;
bool sHeapCheckReady;
bool sHeapCheckOk;
bool sFloorFaultReported;
bool sCanaryFaultReported;
void *sPopupXfb;
bool sPopupSurfaceReady;

inline u32 readWord(u32 address) {
    return *reinterpret_cast<volatile u32 *>(address);
}

inline u8 readByte(u32 address) {
    return *reinterpret_cast<volatile u8 *>(address);
}

inline void writeWord(u32 address, u32 value) {
    *reinterpret_cast<volatile u32 *>(address) = value;
}

inline bool isMem1Range(u32 address, u32 size) {
    return size <= kMem1End - kMem1Start && address >= kMem1Start &&
           address <= kMem1End - size && (address & 3u) == 0;
}

bool isExpHeapPointer(u32 address) {
    if (!isMem1Range(address, 0x84u) ||
        readWord(address) != kExpHeapVtable) {
        return false;
    }

    const u32 start = readWord(address + 0x30u);
    const u32 end = readWord(address + 0x34u);
    const u32 size = readWord(address + 0x38u);
    return start >= kMem1Start && start <= end && end <= kMem1End &&
           (start & 3u) == 0 && (end & 3u) == 0 && size == end - start;
}

inline volatile u32 *canaryWords() {
    return reinterpret_cast<volatile u32 *>(kCanaryAddr);
}

HeapSample sampleHeap(u32 globalAddress) {
    HeapSample sample = {readWord(globalAddress), false};
    if (!isExpHeapPointer(sample.pointer)) {
        return sample;
    }

    sample.valid = true;
    return sample;
}

void sampleFloorAndCanary() {
    const u32 root = readWord(kLMRootHeapAddr);
    if (!root) {
        return;
    }

    bool floorNow = false;
    if (isExpHeapPointer(root)) {
        const u32 heapStart = readWord(root + 0x30u);
        const u32 heapEnd = readWord(root + 0x34u);
        floorNow = root >= kModEnd && heapStart >= kModEnd &&
                   heapStart <= heapEnd && heapEnd <= kMem1End;
    }

    if (!sFloorObserved) {
        sFloorObserved = true;
        sFloorOk = floorNow;
    } else if (!floorNow) {
        sFloorOk = false;
    }

    // A bad floor means this address may belong to a live retail heap.  Never
    // write a test pattern until both the root object and its managed range
    // prove that the entire Moonshine window is outside the heap.
    if (sFloorOk && !sCanaryReady) {
        canaryWords()[0] = kCanary[0];
        canaryWords()[1] = kCanary[1];
        canaryWords()[2] = kCanary[2];
        canaryWords()[3] = kCanary[3];
        sCanaryReady = true;
        sCanaryOk = true;
    }

    if (sCanaryReady &&
        (canaryWords()[0] != kCanary[0] ||
         canaryWords()[1] != kCanary[1] ||
         canaryWords()[2] != kCanary[2] ||
         canaryWords()[3] != kCanary[3])) {
        sCanaryOk = false;
    }
}

void sampleHeapChecks(const HeapSample &system, const HeapSample &game) {
    if (!system.valid || !game.valid) {
        // LM legitimately tears down and recreates its game heap at room
        // boundaries.  Preserve the last structural result during that gap;
        // only a complete bounded allocator check may latch corruption.
        return;
    }

    if (++sFrames < 60u) {
        return;
    }
    sFrames = 0;

    const bool systemOk = LMState::heapHealthy(system.pointer);
    const bool gameOk = systemOk && LMState::heapHealthy(game.pointer);
    if (!sHeapCheckReady) {
        sHeapCheckReady = true;
        sHeapCheckOk = systemOk && gameOk;
    } else if (!systemOk || !gameOk) {
        sHeapCheckOk = false;
    }
    if (!systemOk || !gameOk) __builtin_trap();
}

void drawStatusPopup(void *directPrint, void *xfb) {
    const char *message = LMNotice::text();
    if (!*message || LMPractice::isOpen()) return;
    // Retail coordinates are doubled: a 112x22 physical-pixel notice, inset
    // 24 pixels horizontally and 32 vertically to clear TV overscan.
    reinterpret_cast<DirectPrintChangeFrameBufferFn>(
        kDirectPrintChangeFrameBufferAddr)(directPrint, xfb, kXfbWidth, kXfbHeight);
    reinterpret_cast<DirectPrintEraseFn>(kDirectPrintEraseAddr)(
        directPrint, kPopupLeft, kPopupTop, kPopupWidth, kPopupHeight);
    reinterpret_cast<DirectPrintDrawStringFn>(kDirectPrintDrawStringAddr)(
        directPrint, kPopupLeft + 4u, kPopupTop + 2u, "%s", message);
    reinterpret_cast<CacheRangeFn>(kDCFlushRangeAddr)(
        reinterpret_cast<u8 *>(xfb) + kPopupTop * 2u * kXfbRowBytes,
        kPopupHeight * 2u * kXfbRowBytes);
}

void sampleDiagnostic(HeapSample *system, HeapSample *game) {
    sampleFloorAndCanary();
    *system = sampleHeap(kLMSystemHeapAddr);
    *game = sampleHeap(kLMGameHeapAddr);
    sampleHeapChecks(*system, *game);
}

}  // namespace

void LMNotice::present() {
    if (!sPopupSurfaceReady) return;
    const u32 directPrint = readWord(kDirectPrintPtrAddr);
    if (isMem1Range(directPrint, 0x18u))
        drawStatusPopup(reinterpret_cast<void *>(directPrint), sPopupXfb);
}

// Replaces GLMJ01's two-instruction OSGetArenaLo getter.  Because patches.py
// installs a plain branch, returning here goes directly to the retail caller.
// The threshold makes repeated calls safe after createRoot consumes the arena.
extern "C" void *getArenaLo() {
    u32 arenaLo = readWord(kOSArenaLoAddr);
    if (arenaLo < kModEnd) {
        arenaLo += SUSAMUNE_ARENA_RESERVE_SIZE;
    }
    return reinterpret_cast<void *>(arenaLo);
}

// Services state requests only after the complete retail presenter returns.
// Moonshine uses the same kind of after-draw boundary: restoring from inside
// GXCopyDisp left LM's VI/retrace tail observing a mixture of two timelines.
extern "C" void diagnosticChangeFrameBuffer() {
    sPopupSurfaceReady = false;
    LMState::presenterEnter();
    reinterpret_cast<VoidFn>(kLMChangeFrameBufferAddr)();
    LMState::presenterAfterRetail();
    LMState::presenterBeforeTick();
    LMWarp::tick();
    LMPractice::tick();
    LMPreferences::tick();
    LMState::tick(!LMPractice::isOpen() && !LMWarp::active());
    LMTools::tick(LMPractice::isOpen());
    LMColour::tick();
    LMState::presenterAfterTick();
    LMNotice::tick();
    sPopupSurfaceReady = false;
}

// These main-loop calls are the first useful boundaries after a restore.
// Persistent entry/exit phases survive a hard lock without requiring an
// exception, so one hardware run can identify the first stalled game step.
extern "C" void diagnosticFrameBegin() {
    LMState::postLoadMilestone(0x88u);
    reinterpret_cast<VoidFn>(kLMFrameBeginAddr)();
    LMState::postLoadMilestone(0x89u);
}

extern "C" void diagnosticMainSceneStep() {
    LMState::postLoadMilestone(0x8Au);
    reinterpret_cast<VoidFn>(kLMMainSceneStepAddr)();
    LMState::postLoadMilestone(0x8Bu);
}

// Split LM's first restored draw into GX matrix setup, scene callback, and
// projection reset. These wrappers are always transparent outside tracing.
extern "C" void diagnosticFirstPosMatrix(MatrixPtr matrix, u32 index) {
    LMState::postLoadMilestone(0xA0u);
    reinterpret_cast<GXLoadMtxFn>(kGXLoadPosMtxImmAddr)(matrix, index);
    LMState::postLoadMilestone(0xA1u);
}

extern "C" void diagnosticLastNrmMatrix(MatrixPtr matrix, u32 index) {
    LMState::postLoadMilestone(0xA2u);
    reinterpret_cast<GXLoadMtxFn>(kGXLoadNrmMtxImmAddr)(matrix, index);
    LMState::postLoadMilestone(0xA3u);
}

extern "C" void diagnosticSceneDraw(void *scene) {
    const u32 callback = readWord(reinterpret_cast<u32>(scene) + 0x1Cu);
    LMState::postLoadDetail(0xA4u, readWord(kLMMainDrawStateAddr), callback);
    // Retail deliberately leaves sCurScene in r3 for this dynamic call.
    reinterpret_cast<VoidPtrFn>(callback)(scene);
    LMState::postLoadDetail(0xA5u, readWord(kLMMainDrawStateAddr), callback);
}

bool animatedModelControllerSafe(u32 slot, u32 controller, u32 *fault) {
    const u32 controllerFlags = readWord(controller);
    const u32 primaryCount = readWord(controller + 0x120u);

    // Retail treats these two lazy-controller shapes as intentional no-ops.
    if (!(controllerFlags & 1u) && primaryCount == 0u) {
        return true;
    }
    if (primaryCount == 0u || primaryCount > kAnimatedModelPrimaryCapacity) {
        *fault = primaryCount;
        return false;
    }

    const u32 primaryIndex = readWord(controller + 0x11Cu);
    if (primaryIndex >= primaryCount) {
        *fault = primaryIndex;
        return false;
    }
    const u32 model = readWord(controller + 4u + primaryIndex * sizeof(u32));
    if (!isMem1Range(model, 0x40u)) {
        *fault = model;
        return false;
    }
    if (!(controllerFlags & 1u) && readByte(model) != 2u) {
        return true;
    }

    const u32 descriptor = readWord(model + 0x3Cu);
    if (!isMem1Range(descriptor, 0x30u)) {
        *fault = descriptor;
        return false;
    }

    const u32 secondaryCount = readWord(controller + 0x124u);
    if (secondaryCount > kAnimatedModelSecondaryCapacity) {
        *fault = secondaryCount;
        return false;
    }
    if (secondaryCount != 0u) {
        const s32 secondaryIndex = static_cast<s8>(readByte(slot + 0x38u));
        if (secondaryIndex < 0 ||
            static_cast<u32>(secondaryIndex) >= secondaryCount) {
            *fault = static_cast<u32>(secondaryIndex);
            return false;
        }
        const u32 animation =
            readWord(controller + 0x44u +
                     static_cast<u32>(secondaryIndex) * sizeof(u32));
        if (!isMem1Range(animation, 0x14u)) {
            *fault = animation;
            return false;
        }
    }

    // Once initialised, retail unconditionally uses this matrix-output area.
    const u32 output = readWord(controller + 0x128u);
    if ((controllerFlags & 1u) && !isMem1Range(output, 0x30u)) {
        *fault = output;
        return false;
    }
    return true;
}

// The heap pool and its fixed owner registry must describe the same epoch.
// Reject a broken entry/controller association before retail enters the slot;
// its post-controller audio step also assumes this mapping is valid.
extern "C" void diagnosticAnimatedModelPoolUpdate() {
    const u32 pool = readWord(kLMAnimatedModelPoolGlobal);
    const u32 controllers = readWord(kLMAnimatedModelControllerPoolGlobal);
    if (!isMem1Range(pool,
                     kAnimatedModelSlotCount * kAnimatedModelSlotSize) ||
        !isMem1Range(controllers,
                     kAnimatedModelSlotCount *
                         kAnimatedModelControllerSize)) {
        LMState::postLoadDetail(0xE4u, pool, controllers);
        return;
    }

    for (u32 index = 0; index < kAnimatedModelSlotCount; ++index) {
        const u32 slot = pool + index * kAnimatedModelSlotSize;
        const u32 flags = readWord(slot + 0x3Cu);
        if (!(flags & 2u)) {
            continue;
        }

        const u32 controller = readWord(slot + 0x70u);
        if (controller == 0u) {
            continue;
        }
        const u32 expectedController =
            controllers + index * kAnimatedModelControllerSize;
        if (controller != expectedController) {
            LMState::postLoadDetail(0xE4u, index, controller);
            writeWord(slot + 0x3Cu, flags & ~3u);
        }
    }

    reinterpret_cast<VoidFn>(kLMAnimatedModelPoolUpdateAddr)();
}

// This is the final call before LM consumes the selected model's relocated
// joint table. Suppressing only that call lets an asynchronously repaired
// cosmetic resource recover on a later frame without destroying its owner.
extern "C" void diagnosticAnimatedModelControllerUpdate(u32 controller,
                                                         u32 slot,
                                                         u32 animationIndex,
                                                         u32 frame) {
    const u32 pool = readWord(kLMAnimatedModelPoolGlobal);
    const u32 controllers = readWord(kLMAnimatedModelControllerPoolGlobal);
    const bool poolValid =
        isMem1Range(pool, kAnimatedModelSlotCount * kAnimatedModelSlotSize);
    const bool controllersValid =
        isMem1Range(controllers, kAnimatedModelSlotCount *
                                    kAnimatedModelControllerSize);
    const u32 slotOffset = slot - pool;
    const bool slotValid =
        poolValid && slot >= pool && slotOffset % kAnimatedModelSlotSize == 0u &&
        slotOffset / kAnimatedModelSlotSize < kAnimatedModelSlotCount;
    if (!slotValid || !controllersValid) {
        LMState::postLoadDetail(0xE5u, slot, controller);
        return;
    }

    const u32 index = slotOffset / kAnimatedModelSlotSize;
    const u32 expectedController =
        controllers + index * kAnimatedModelControllerSize;
    if (controller != expectedController) {
        // The caller reloads +0x70 and consumes +0x314 after we return.
        writeWord(slot + 0x70u, expectedController);
        writeWord(slot + 0x3Cu, readWord(slot + 0x3Cu) & ~3u);
        LMState::postLoadDetail(0xE5u, index, controller);
        return;
    }

    u32 fault = 0u;
    if (!animatedModelControllerSafe(slot, controller, &fault)) {
        LMState::postLoadDetail(0xE5u, index, fault);
        return;
    }
    reinterpret_cast<RetailCall4Fn>(kLMAnimatedModelControllerUpdateAddr)(
        controller, slot, animationIndex, frame);
}

extern "C" void diagnosticOrthoReset() {
    LMState::postLoadMilestone(0xA6u);
    reinterpret_cast<VoidFn>(kLMDefaultOrthoViewAddr)();
    LMState::postLoadMilestone(0xA7u);
}

extern "C" void diagnosticPreMainUpdate() {
    LMState::postLoadMilestone(0x8Cu);
    reinterpret_cast<VoidFn>(kLMPreMainUpdateAddr)();
    LMState::postLoadMilestone(0x8Du);
}

extern "C" void diagnosticPostMainUpdate(void *state) {
    LMState::postLoadMilestone(0x8Eu);
    LMState::postLoadDetail(0xE2u, readWord(kLMTransitionStateAddr),
                            readWord(kLMTransitionStateAddr + 8u));
    reinterpret_cast<VoidPtrFn>(kLMPostMainUpdateAddr)(state);
    LMState::postLoadDetail(0xE3u, readWord(kLMTransitionStateAddr),
                            readWord(kLMTransitionStateAddr + 8u));
    LMState::postLoadMilestone(0x8Fu);
}

// This is the only non-trivial retail call between the fade-controller update
// and the presenter. The following 0x80186868 target is a bare blr and remains
// unwrapped to avoid adding ABI and timing noise around a retail no-op.
extern "C" void diagnosticAudioTailB618() {
    LMState::postLoadDetail(0xF0u, 0x8000B618u, 0x801867B4u);
    reinterpret_cast<VoidFn>(0x801867B4u)();
    LMState::postLoadDetail(0xF1u, 0x8000B618u, 0x801867B4u);
}

// These execute immediately after the post-presenter transaction wrapper and
// close the remaining blind spot before the next frame-begin milestone.
extern "C" void diagnosticConditionalTail(void *state) {
    LMState::postLoadMilestone(0x90u);
    reinterpret_cast<VoidPtrFn>(kLMConditionalTailAddr)(state);
    LMState::postLoadMilestone(0x91u);
}

extern "C" void diagnosticLoopTailSync() {
    LMState::postLoadMilestone(0x92u);
    reinterpret_cast<VoidFn>(kLMLoopTailSyncAddr)();
    LMState::postLoadMilestone(0x93u);
}

extern "C" void diagnosticLoopTailClock() {
    LMState::postLoadMilestone(0x94u);
    reinterpret_cast<VoidFn>(kLMLoopTailClockAddr)();
    LMState::postLoadMilestone(0x95u);
}

// If restored state makes LM leave its inner game loop, these markers follow
// the return through the outer scene-transition path. The vtable call between
// 0x97 and 0x98 remains deliberately unwrapped, so a final 0x97 isolates it.
extern "C" void diagnosticGameLoop() {
    LMState::postLoadMilestone(0x96u);
    reinterpret_cast<VoidFn>(kLMGameLoopAddr)();
    LMState::postLoadMilestone(0x97u);
}

extern "C" void diagnosticOuterCleanup() {
    LMState::postLoadMilestone(0x98u);
    reinterpret_cast<VoidFn>(kLMOuterCleanupAddr)();
    LMState::postLoadMilestone(0x99u);
}

extern "C" void diagnosticOuterRestart(u32 heapCount) {
    LMState::postLoadMilestone(0x9Au);
    reinterpret_cast<VoidU32Fn>(kLMOuterRestartAddr)(heapCount);
    LMState::postLoadMilestone(0x9Bu);
}

// Wraps both GLMJ01 GXCopyDisp call sites.  The original copy is allowed to
// complete before touching the XFB, so this path cannot depend on the EFB's
// projection, vertex descriptors, resource fonts, or scene draw order.
extern "C" void diagnosticCopyDisp(void *xfb, bool clear) {
    HeapSample system;
    HeapSample game;
    sampleDiagnostic(&system, &game);
    LMState::presenterAfterSample();

    reinterpret_cast<GXCopyDispFn>(kGXCopyDispAddr)(xfb, clear);
    reinterpret_cast<VoidFn>(kGXDrawDoneAddr)();
    LMState::presenterAfterDrawDone();

    // Crash registration is lazy because LM's JUTException constructor clears
    // the callback during early boot. All journaling stays enabled even
    // though the permanent diagnostic panel is no longer drawn.
    LMCrash::init();
    if (sFloorObserved && !sFloorOk && !sFloorFaultReported) {
        LMCrash::note(0x130u, readWord(kLMRootHeapAddr), kModEnd);
        sFloorFaultReported = true;
    }
    if (sCanaryReady && !sCanaryOk && !sCanaryFaultReported) {
        LMCrash::note(0x131u, kCanaryAddr, sizeof(kCanary));
        sCanaryFaultReported = true;
    }

    const u32 rawAddress = reinterpret_cast<u32>(xfb);
    const u32 segment = rawAddress & 0xC0000000u;
    const u32 physical = rawAddress & 0x3FFFFFFFu;
    const bool validXfb =
        (segment == 0x80000000u || segment == 0xC0000000u) &&
        (physical & 31u) == 0u && physical >= 0x00003100u &&
        physical <= 0x01800000u - kXfbSize;
    if (validXfb) {
        void *const cachedXfb =
            reinterpret_cast<void *>(kMem1Start | physical);
        reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(cachedXfb,
                                                                kXfbSize);

        const u32 directPrintAddress = readWord(kDirectPrintPtrAddr);
        const bool directPrintReady =
            isMem1Range(directPrintAddress, 0x18u);
        if (directPrintReady) {
            // Other overlays borrow this binding even when no notice is visible.
            reinterpret_cast<DirectPrintChangeFrameBufferFn>(
                kDirectPrintChangeFrameBufferAddr)(reinterpret_cast<void *>(directPrintAddress),
                    cachedXfb, kXfbWidth, kXfbHeight);
            LMPractice::draw(reinterpret_cast<void *>(directPrintAddress), cachedXfb);
            // Native screenshot/menu mode uses the other XFB as a texture.
            // Its sole display buffer is already being scanned by VI.
            if (!LMPractice::isOpen() && readByte(kLMDoubleBufferAddr) == 1u) {
                LMTools::draw(reinterpret_cast<void *>(directPrintAddress), cachedXfb);
            }
            drawStatusPopup(reinterpret_cast<void *>(directPrintAddress), cachedXfb);
        }
        sPopupXfb = cachedXfb;
        sPopupSurfaceReady = true;
    }

    // The transaction runs from diagnosticChangeFrameBuffer only after LM's
    // entire VI/retrace presenter tail has completed. It can paint its start
    // notice onto this completed XFB; results appear on the next normal copy.
}

#endif  // defined(SUSAMUNE_VERSION_LMJ)
