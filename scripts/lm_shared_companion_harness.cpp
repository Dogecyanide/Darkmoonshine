// Real companion + owner validator + codec over private low-address host RAM.
#include <windows.h>
#include <stdint.h>
#include <string.h>
#define IS_EMULATOR 0
#include "susamune/lm_state_storage.h"
#include "susamune/lm_state_deflate.h"
#include "susamune/lm_shared_archive.h"
#include "susamune/lm_crc32.h"

using u8 = uint8_t;
using u32 = uint32_t;
#undef SUSAMUNE_LM_CACHE_PPC_BASE
#define SUSAMUNE_LM_CACHE_PPC_BASE 0x22000000u
constexpr u32 kSnapshotBase = 0x20000000u;
constexpr u32 kHeapDataOffset = 0x191C0u;
constexpr u32 kMem1Base = 0x80000000u, kMem1Size = 0x1800000u;
// Only the outer header fields used by the included production code are shims.
struct SnapshotHeader {
    u32 totalSize, generation, checksum, systemHeap, systemHeapStart, systemHeapEnd;
};
static u32 sCrossRoomFault, sCrossRoomFaultValue;
static u32 flushCalls, flushBase, flushSize;
static void cacheNoop(void *address, u32 bytes) {
    ++flushCalls; flushBase = (u32)(uintptr_t)address; flushSize = bytes;
}
using CacheRangeFn = void (*)(void *, u32);
static const uintptr_t kDCStoreRangeAddr = (uintptr_t)cacheNoop;
static bool isMem1Range(u32 address, u32 bytes) {
    return !(address & 3u) && address >= kMem1Base &&
           address < kMem1Base + kMem1Size && bytes <= kMem1Base + kMem1Size - address;
}
static u32 readWord(u32 address) {
    const u8 *p = (const u8 *)(uintptr_t)address;
    return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
}
static u8 readByte(u32 address) { return *(const u8 *)(uintptr_t)address; }
static void clearWords(void *address, u32 bytes) { memset(address, 0, bytes); }
static void copyBytes(void *destination, const void *source, u32 bytes) {
    memmove(destination, source, bytes);
}
static bool crossRoomFault(u32 fault, u32 value) {
    sCrossRoomFault = fault; sCrossRoomFaultValue = value; return false;
}
static u32 crcByte(u32 crc, u8 byte) {
    return LmCrc32Byte(crc, byte);
}

// No rewritten algorithms or source substitutions in this harness.
#include "../lm_diag/src/lm_state_shared.inc"

static LmSharedArchiveDescriptor identity;
static u32 staged, committed;
static void *mapped[3];
static SnapshotHeader *header() { return (SnapshotHeader *)(uintptr_t)kSnapshotBase; }
static SharedCompanion *companion() {
    return (SharedCompanion *)(uintptr_t)(kSnapshotBase + header()->totalSize);
}
#define API extern "C" __declspec(dllexport)

API void companion_shutdown() {
    for (void *&address : mapped) {
        if (address) VirtualFree(address, 0, MEM_RELEASE);
        address = nullptr;
    }
}
API int companion_reset(const void *mem1, u32 bytes) {
    if (!mem1 || bytes != kMem1Size) return 0;
    const u32 addresses[3] = {kSnapshotBase, SUSAMUNE_LM_CACHE_PPC_BASE, kMem1Base};
    const u32 sizes[3] = {SUSAMUNE_MEM2_SNAPSHOT_SIZE, SUSAMUNE_LM_CACHE_SIZE, kMem1Size};
    for (u32 i = 0; i < 3; ++i) if (!mapped[i]) {
        mapped[i] = VirtualAlloc((void *)(uintptr_t)addresses[i], sizes[i],
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (mapped[i] != (void *)(uintptr_t)addresses[i]) {
            companion_shutdown(); return 0;
        }
    }
    memcpy(mapped[2], mem1, bytes);
    memset(mapped[0], 0xA5, sizes[0]);
    memset(mapped[1], 0x5A, sizes[1]);
    sCrossRoomFault = sCrossRoomFaultValue = 0;
    staged = committed = flushCalls = flushBase = flushSize = 0;
    return sharedArchiveIdentity(&identity);
}
API u32 companion_stage(u32 coreSize) {
    staged = stageSharedArchive(coreSize, &identity);
    return staged;
}
API u32 companion_codec_size(int fast) {
    return fast ? LmStateDeflateFast((const u8 *)(uintptr_t)identity.base, identity.size,
                                  nullptr, (void *)(uintptr_t)kSharedCodecWorkspace) :
                  LmStateDeflate((const u8 *)(uintptr_t)identity.base, identity.size,
                                nullptr, (void *)(uintptr_t)kSharedCodecWorkspace);
}
API u32 companion_commit(u32 coreSize, u32 generation, u32 checksum) {
    if (!staged) return 0;
    SnapshotHeader *h = header();
    h->totalSize = coreSize; h->generation = generation; h->checksum = checksum;
    h->systemHeap = identity.systemHeap;
    h->systemHeapStart = identity.systemStart; h->systemHeapEnd = identity.systemEnd;
    committed = commitSharedArchive(h, identity, staged);
    return committed;
}
API u32 companion_make(u32 coreSize, u32 generation, u32 checksum) {
    if (!companion_stage(coreSize)) return 0;
    return companion_commit(coreSize, generation, checksum);
}
API u32 companion_stored() { return snapshotStoredSize(header()); }
API int companion_valid(u32 size) { return snapshotCompanionValid(header(), size); }
API int companion_matches() {
    LmSharedArchiveDescriptor live;
    return sharedArchiveMatchesSnapshot(header(), &live);
}
API void companion_rechecksum() { companion()->checksum = sharedCompanionChecksum(companion()); }
API int companion_restore(int requireMatch) {
    LmSharedArchiveDescriptor live;
    if (!snapshotCompanionValid(header(), committed)) return 0;
    if (requireMatch) {
        if (!sharedArchiveMatchesSnapshot(header(), &live)) return 0;
    } else if (!sharedArchiveIdentity(&live)) return 0;
    // requireMatch=0 unit-tests the restore helper's trusted-live target contract.
    return restoreSharedArchive(header(), live);
}
API u32 companion_metric(u32 index) {
    switch (index) {
    case 0: return kSnapshotBase;
    case 1: return kSharedPayloadLimit;
    case 2: return identity.base;
    case 3: return identity.size;
    case 4: return kMem1Base;
    case 5: return kMem1Size;
    case 6: return kSharedStagingStart;
    case 7: return kSharedStagingSize;
    case 8: return staged;
    case 9: return committed;
    case 10: return flushCalls;
    case 11: return flushBase;
    case 12: return flushSize;
    case 13: return sCrossRoomFault;
    case 14: return sCrossRoomFaultValue;
    case 15: return identity.owner;
    case 16: return SUSAMUNE_LM_CACHE_PPC_BASE;
    case 17: return SUSAMUNE_LM_MAILBOX_SIZE;
    case 18: return kSharedCodecWorkspace;
    case 19: return sizeof(LmStateStorageMailbox);
    case 20: return LM_STATE_DEFLATE_WORKSPACE;
    case 21: return SUSAMUNE_LM_CACHE_SIZE;
    default: return 0;
    }
}
