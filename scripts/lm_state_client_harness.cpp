// Real PPC storage client and ARM worker sharing low-address host buffers.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vector>
#include "susamune/lm_state_storage.h"
#include "susamune/lm_state_deflate.h"
#include "susamune/lm_state_auth.h"
#include "susamune/lm_persistent_profile.h"
#include "susamune/lm_crc32.h"
#define IS_EMULATOR 0
typedef uint32_t u32;
typedef uint8_t u8;
typedef uint64_t u64;
#define LM_STATE_TEST_SHARED_SNAPSHOT 0x20000000u
namespace Worker {
#include "lm_state_kernel_harness.c"
}
#undef SUSAMUNE_LM_CACHE_PPC_BASE
#define SUSAMUNE_LM_CACHE_PPC_BASE 0x22000000u
#undef LM_STATE_STORAGE_PPC_PTR
#define LM_STATE_STORAGE_PPC_PTR (&Worker::TestMailbox)
static constexpr u32 kSnapshotBase = 0x20000000u;
static constexpr u32 kSnapshotCapacity = SUSAMUNE_MEM2_SNAPSHOT_SIZE;
static constexpr u32 kHeaderSize = 256u, kSnapshotVersion = 28u;
static constexpr u32 kCameraObjectCount = 0, kCameraObjectSize = 0xEC;
static constexpr u32 kMaxVolumes = 64;
static constexpr u32 kPackedBase = 0x23000000u;
struct SnapshotHeader {
    u32 magic, version, totalSize, checksum, generation, rootHeapStart, rootHeapEnd;
};
struct VolumeCensus { u32 generation, count; };
struct ResourceCensus { u32 generation; };
struct ModelCensusMetadata { u32 generation; };
static VolumeCensus sSavedVolumeCensus;
static ResourceCensus sSavedResourceCensus;
static ModelCensusMetadata TestModelMetadata;
static struct { ModelCensusMetadata *metadata; } sSavedModelCensus = {&TestModelMetadata};
static u32 sMenuRequest, sSnapshotSize, sTimelineRevision, sGeneration;
static bool sPersistentKeyReady, sPersistentLoaded, TestPersistentProfile;
static u32 sPersistentConfigId, TestBoot;
static u32 TestPatternMask = 255u;
static LmPersistentProfile sSavedPersistentProfile;
namespace LMState { enum class Status { Saved, Empty }; }
static LMState::Status sStatus;
static void cacheNoop(void *, u32) {}
typedef void (*CacheRangeFn)(void *, u32);
static const uintptr_t kDCStoreRangeAddr = (uintptr_t)cacheNoop;
static const uintptr_t kDCInvalidateRangeAddr = (uintptr_t)cacheNoop;
struct SusamuneCrashReport { u32 modFileCrc32; };
static SusamuneCrashReport TestReport = {123};
#define SUSAMUNE_CRASH_PPC_PTR (&TestReport)
static u32 readWord(u32 p) { return *(u32*)(uintptr_t)p; }
static void writeWord(u32 p, u32 v) { *(u32*)(uintptr_t)p = v; }
static void copyBytes(void *p, const void *q, u32 n) { memmove(p, q, n); }
static void clearEpochMismatch() {}
static void clearVolumeDiff() {}
static void clearResourceDiff() {}
static void clearModelDiff() {}
static u32 cameraObjectRecordAddress(u32) { return kSnapshotBase; }
static bool isMem1ByteRange(u32, u32) { return true; }
static bool rangeInside(u32 a, u32 b, u32 c, u32 d) { return a >= c && b <= d; }
static u32 crcByte(u32 crc, u8 byte) {
    return LmCrc32Byte(crc, byte);
}
static u32 snapshotChecksum(const SnapshotHeader *h) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = kHeaderSize; i < h->totalSize; ++i)
        crc = crcByte(crc, ((const u8*)h)[i]);
    return crc ^ 0xFFFFFFFFu;
}
static bool basicHeaderValid(const SnapshotHeader *h) {
    return h->magic == 0x4C4D5354 && h->version == kSnapshotVersion &&
        h->totalSize >= kHeaderSize && h->totalSize < LM_STATE_STORAGE_PAYLOAD_MAX;
}
// The companion is a fixture of the production helper contract, not a game
// owner proof. Real compression and transaction code still execute below.
struct TestCompanion {
    u32 magic, packedSize, rawSize, packedCrc, coreCrc, generation, reserved[10];
};
static_assert(sizeof(TestCompanion) == 64, "Companion descriptor fixture");
static u32 testCrc(const void *data, u32 size) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < size; ++i) crc = crcByte(crc, ((const u8*)data)[i]);
    return crc ^ 0xFFFFFFFFu;
}
static u32 snapshotStoredSize(const SnapshotHeader *h) {
    if (!basicHeaderValid(h) || h->totalSize > LM_STATE_STORAGE_PAYLOAD_MAX - 64u) return 0;
    const TestCompanion *c = (const TestCompanion*)((const u8*)h + h->totalSize);
    if (c->magic != 0x434F4D50 || !c->packedSize || !c->rawSize ||
        c->packedSize > LM_STATE_STORAGE_PAYLOAD_MAX - h->totalSize - 64u ||
        c->packedSize > 0xFFFFFFE0u) return 0;
    const u32 aligned = (c->packedSize + 31u) & ~31u;
    if (aligned > LM_STATE_STORAGE_PAYLOAD_MAX - h->totalSize - 64u) return 0;
    return h->totalSize + 64u + aligned;
}
static bool snapshotCompanionValid(const SnapshotHeader *h, u32 size) {
    if (snapshotStoredSize(h) != size) return false;
    const TestCompanion *c = (const TestCompanion*)((const u8*)h + h->totalSize);
    if (c->coreCrc != h->checksum || c->generation != h->generation ||
        c->packedCrc != testCrc(c + 1, c->packedSize)) return false;
    for (u32 value : c->reserved) if (value) return false;
    const LmStateSegment source[2] = {{(u8*)(c + 1), c->packedSize}, {nullptr, 0}};
    return LmStateInflate(source, c->packedSize, nullptr, c->rawSize,
                         (void*)(uintptr_t)(SUSAMUNE_LM_CACHE_PPC_BASE + SUSAMUNE_LM_MAILBOX_SIZE));
}
// Test runner changes only PPC assembly to deterministic/no-op host shims.
#include "lm_storage_under_test.inc"

#undef API
#define API extern "C" __declspec(dllexport)
static void resetClientState() {
    memset((void*)(uintptr_t)kSnapshotBase, 0, SUSAMUNE_MEM2_SNAPSHOT_SIZE);
    memset((void*)(uintptr_t)kPackedBase, 0, 0x600000);
    memset(sSlots, 0, sizeof(sSlots));
    memset(sCatalog, 0, sizeof(sCatalog));
    memset(&sSavedVolumeCensus, 0, sizeof(sSavedVolumeCensus));
    memset(&sSavedResourceCensus, 0, sizeof(sSavedResourceCensus));
    memset(&TestModelMetadata, 0, sizeof(TestModelMetadata));
    memset(&sSavedPersistentProfile, 0, sizeof(sSavedPersistentProfile));
    sSelectedSlot = sCacheUsed = sStorageSeq = sStorageCommand = 0;
    sStorageRequestedId = sStorageExpectedPayload = sPackedNeeded = 0;
    sPackedCodec = 0;
    TestPatternMask = 255u;
    sArchiveId = sMenuRequest = sSnapshotSize = sTimelineRevision = sGeneration = 0;
    sPersistentKeyReady = sPersistentLoaded = TestPersistentProfile = false;
    sPersistentKeyId = sPersistentConfigId = 0;
    sPersistentKey0 = sPersistentKey1 = 0;
    sStorageInitialized = true; sStorageStartupPending = false;
    sStorageText = "READY";
    sCatalogText = "OPEN TO LIST SD FILES";
    sCatalogSeq = sCatalogCount = sCatalogCursor = sCatalogNext = 0; sCatalogMore = false;
    memset(sCatalogIdentity, 0, sizeof(sCatalogIdentity));
    sArchiveSession = 987u + TestBoot; sArchiveBuild = 123;
    sArchiveKey0 = 111u + TestBoot; sArchiveKey1 = 222u + TestBoot;
    Worker::TestMailbox.processSession = sArchiveSession;
    sSlotCacheStart = kPackedBase; sSlotCacheSize = 0x600000;
}
API int client_reset() {
    static bool mapped;
    if (!mapped) {
        if (!VirtualAlloc((void*)(uintptr_t)kSnapshotBase, SUSAMUNE_MEM2_SNAPSHOT_SIZE + 0x10000,
                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc((void*)(uintptr_t)SUSAMUNE_LM_CACHE_PPC_BASE, SUSAMUNE_LM_CACHE_SIZE,
                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc((void*)(uintptr_t)kPackedBase, 0x600000,
                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) return 0;
        mapped = true;
    }
    Worker::reset_worker();
    TestBoot = 0;
    resetClientState();
    return 1;
}
API void client_reboot() {
    Worker::restart_worker();
    ++TestBoot;
    resetClientState();
}
API int client_prepare_key() { return storageStartupReady(); }
API void client_enable_persistent(u32 enabled) { TestPersistentProfile = enabled != 0; }
API u32 client_key_ready() { return sPersistentKeyReady; }
API u32 client_key_id() { return sPersistentKeyId; }
API u32 client_config_id() { return sPersistentConfigId; }
API u32 client_process_session() { return sArchiveSession; }
API u32 client_persistent_loaded() { return sPersistentLoaded; }
API u32 client_profile_crc() { return sSavedPersistentProfile.checksum; }
API u32 client_trailer_size() { return kSlotTrailerSize; }
API u32 client_raw_valid() { return rawSlotValid(sSlots[0].rawSize); }
API void client_set_config(u32 field, u32 value) { Worker::set_config(field, value); }
static void testFill(u8 *data, u32 bytes, u32 marker, u32 seed) {
    if (!seed) { memset(data, marker, bytes); return; }
    for (u32 i = 0; i < bytes; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        data[i] = seed & TestPatternMask;
    }
}
API int client_save_profile(u32 marker, u32 bytes, u32 shared, u32 seed) {
    if (bytes < kHeaderSize || bytes > LM_STATE_STORAGE_PAYLOAD_MAX - 96u - kSlotTrailerSize || !shared) return 0;
    memset((void*)(uintptr_t)kSnapshotBase, marker, bytes);
    testFill((u8*)(uintptr_t)kSnapshotBase, bytes, marker, seed);
    SnapshotHeader *h = (SnapshotHeader*)(uintptr_t)kSnapshotBase;
    h->magic = 0x4C4D5354; h->version = kSnapshotVersion; h->totalSize = bytes;
    h->generation = ++sGeneration; h->checksum = snapshotChecksum(h);
    TestCompanion *c = (TestCompanion*)((u8*)h + bytes);
    memset(c, 0, sizeof(*c));
    std::vector<u8> data(shared);
    testFill(data.data(), shared, marker, seed);
    const u32 capacity = LM_STATE_STORAGE_PAYLOAD_MAX - bytes - 64u - kSlotTrailerSize;
    const LmStateSegment output[2] = {{(u8*)(c + 1), capacity}, {nullptr, 0}};
    c->packedSize = LmStateDeflate(data.data(), shared, output,
                                 (void*)(uintptr_t)kCodecWorkspace);
    if (!c->packedSize || c->packedSize > capacity ||
        ((c->packedSize + 31u) & ~31u) > capacity) return 0;
    c->magic = 0x434F4D50; c->rawSize = shared;
    c->coreCrc = h->checksum; c->generation = h->generation;
    c->packedCrc = testCrc(c + 1, c->packedSize);
    memset((u8*)(c + 1) + c->packedSize, 0, ((c->packedSize + 31u) & ~31u) - c->packedSize);
    sSavedVolumeCensus.generation = h->generation; sSavedVolumeCensus.count = 1;
    sSavedResourceCensus.generation = h->generation;
    TestModelMetadata.generation = h->generation;
    // Opt-in wire-format fixture only; native retained-owner proof runs elsewhere.
    memset(&sSavedPersistentProfile, 0, sizeof(sSavedPersistentProfile));
    if (TestPersistentProfile) {
        sSavedPersistentProfile.magic = LM_PERSISTENT_MAGIC;
        sSavedPersistentProfile.version = 1u;
        sSavedPersistentProfile.generation = h->generation;
        sSavedPersistentProfile.configId = sPersistentConfigId;
        sSavedPersistentProfile.rootHeap = 0x80538420u;
        sSavedPersistentProfile.systemHeap = 0x805384C0u;
        sSavedPersistentProfile.count = 1u;
        sSavedPersistentProfile.checksum = LmPersistentChecksum(&sSavedPersistentProfile);
    }
    sSnapshotSize = bytes;
    sPersistentLoaded = false;
    savedSlotCommitted();
    return rawSlotValid(sSlots[0].rawSize);
}
API void client_save(u32 marker) { client_save_profile(marker, 50000u, 4096u, 0u); }
API int client_save_nibble_profile(u32 marker, u32 bytes, u32 seed) {
    TestPatternMask = 15u;
    const int result = client_save_profile(marker, bytes, 4096u, seed);
    TestPatternMask = 255u;
    return result;
}
API int client_start(u32 command, u32 id) { return beginStorage(command, id); }
API int client_start_named(u32 command, u32 id, const char *name) { return beginStorage(command, id, name); }
API void client_step() { Worker::step(); serviceStorage(); }
API int client_pending() { return storageInFlight() || Worker::pending(); }
API const char *client_text() { return sStorageText; }
API u32 client_marker() { return sSlots[sSelectedSlot].rawSize ? readWord(kSnapshotBase + 256) & 255 : 0; }
API u32 client_size() { return sSlots[sSelectedSlot].rawSize; }
API u32 client_core_size() { return sSnapshotSize; }
API u32 client_payload_crc() { return storageCrc((void*)(uintptr_t)kSnapshotBase, sSlots[0].rawSize + kSlotTrailerSize); }
API u32 client_slots() { return kResidentSlots; }
API int client_switch(u32 slot) { return switchSlot(slot); }
API void client_pool_size(u32 bytes) { if (bytes <= 0x600000u) sSlotCacheSize = bytes; }
API u32 client_backup_size() { return sSlots[0].packedSize; }
API u32 client_backup_codec() { return sSlots[0].codec; }
API int client_backup_valid() { return packedSlotValid(sSlots[0]); }
API void client_backup_set_codec(u32 codec) { sSlots[0].codec = codec; }
API u32 client_staging_capacity() { return kSlotStagingSize; }
API u32 client_codec_size(int fast) {
    const u32 size = sSlots[0].rawSize + kSlotTrailerSize;
    return fast ? LmStateDeflateFast((const u8*)(uintptr_t)kSnapshotBase, size,
                                   nullptr, (void*)(uintptr_t)kCodecWorkspace) :
                  LmStateDeflate((const u8*)(uintptr_t)kSnapshotBase, size,
                                 nullptr, (void*)(uintptr_t)kCodecWorkspace);
}
API u32 client_backup_byte(u32 offset) {
    if (offset >= sSlots[0].packedSize) return 0;
    if (offset < sSlotCacheSize) return *(const u8*)(uintptr_t)(sSlotCacheStart + offset);
    return *(const u8*)(uintptr_t)(kSlotStagingStart + offset - sSlotCacheSize);
}
API u32 client_limit() { return LM_STATE_STORAGE_PAYLOAD_MAX; }
API u32 client_archive() { return sArchiveId; }
API void client_empty() { sSlots[sSelectedSlot].rawSize = 0; writeWord(kSnapshotBase, 0); }
API void client_session() { ++sArchiveSession; ++Worker::TestMailbox.processSession; }
API void client_busy(u32 value) { sMenuRequest = value; }
API void client_fault(u32 operation, u32 at) { Worker::fault(operation, at); }
API void client_finish_worker() { while (Worker::pending()) Worker::step(); }
API void client_bad_receipt(u32 field) {
    volatile u32 *p = &Worker::TestMailbox.responseSession;
    if (field < 5) ++p[field];
    else if (field == 5) ++Worker::TestMailbox.resultId;
    else ++Worker::TestMailbox.transferred;
    serviceStorage();
}
API void client_corrupt_file(u32 id, u32 offset) {
    char path[72]; sprintf(path, "/lm_states/archive_%08u.lms", id);
    int i = Worker::Find(path);
    if (i >= 0 && offset < Worker::Files[i].size) Worker::Files[i].data[offset] ^= 1;
}
static int clientFile(u32 id) {
    char path[72]; sprintf(path, "/lm_states/archive_%08u.lms", id);
    return Worker::Find(path);
}
static void resealFile(int i) {
    LmStateArchiveHeader *h = (LmStateArchiveHeader*)Worker::Files[i].data;
    h->payloadCrc = storageCrc(h + 1, h->payloadSize);
    const u64 tag = h->version == LM_STATE_ARCHIVE_PERSISTENT_VERSION ?
        LmStateAuthenticateArchive((const u8*)h, (const u8*)(h + 1), h->payloadSize,
                                  sPersistentKey0, sPersistentKey1) :
        LmStateAuthenticate((const u8*)(h + 1), h->payloadSize, sArchiveKey0, sArchiveKey1);
    h->authHigh = (u32)(tag >> 32); h->authLow = (u32)tag;
}
API u32 client_file_version(u32 id) {
    const int i = clientFile(id);
    return i < 0 ? 0u : ((LmStateArchiveHeader*)Worker::Files[i].data)->version;
}
API int client_header_binding_changes(u32 id, u32 offset) {
    const int i = clientFile(id); if (i < 0 || offset >= 64u) return -1;
    const LmStateArchiveHeader *h = (const LmStateArchiveHeader*)Worker::Files[i].data;
    LmStateArchiveHeader changed = *h;
    ((u8*)&changed)[offset] ^= 1u;
    return LmStateAuthenticateArchive((const u8*)h, (const u8*)(h + 1), h->payloadSize,
                                     sPersistentKey0, sPersistentKey1) !=
        LmStateAuthenticateArchive((const u8*)&changed, (const u8*)(h + 1), h->payloadSize,
                                   sPersistentKey0, sPersistentKey1);
}
API void client_bad_profile(u32 id, u32 field) {
    const int i = clientFile(id); if (i < 0 || field > 8u) return;
    LmStateArchiveHeader *h = (LmStateArchiveHeader*)Worker::Files[i].data;
    LmPersistentProfile *p = (LmPersistentProfile*)((u8*)(h + 1) + h->payloadSize - sizeof(*p));
    if (field == 0u) p->magic ^= 1u;
    if (field == 1u) ++p->version;
    if (field == 2u) ++p->generation;
    if (field == 3u) ++p->checksum;
    if (field == 4u) ++p->configId;
    if (field == 5u) p->count = 0u;
    if (field == 6u) p->count = LM_PERSISTENT_RECORDS + 1u;
    if (field == 7u) memset(p, 0, sizeof(*p));
    if (field == 8u) p->magic = 0u;
    if (field != 3u && field != 7u) p->checksum = LmPersistentChecksum(p);
    resealFile(i);
}
API void client_wrong_key_same_id() { sPersistentKey0 ^= 1u; }
API int client_named_size(const char *name) { return Worker::named_file_size(name); }
API void *client_named_bytes(const char *name) { return Worker::named_file_bytes(name); }
API void client_put_named(const char *name, const u8 *data, u32 size) {
    Worker::add_named_file(name, data, size);
}
API void client_remove_named(const char *name) { Worker::remove_named_file(name); }
API void client_bad_companion(u32 id, u32 field) {
    const int i = clientFile(id); if (i < 0 || field > 6u) return;
    LmStateArchiveHeader *h = (LmStateArchiveHeader*)Worker::Files[i].data;
    SnapshotHeader *s = (SnapshotHeader*)(h + 1);
    TestCompanion *c = (TestCompanion*)((u8*)s + s->totalSize);
    ((u32*)c)[field] ^= 1u;
    if (field == 1u) c->packedSize = 0xFFFFFFF0u;
    resealFile(i);
}
API void client_bad_core_size(u32 id, u32 size) {
    const int i = clientFile(id); if (i < 0) return;
    LmStateArchiveHeader *h = (LmStateArchiveHeader*)Worker::Files[i].data;
    ((SnapshotHeader*)(h + 1))->totalSize = size;
    resealFile(i);
}
API void client_bad_archive_extent(u32 id, u32 raw, u32 payload) {
    const int i = clientFile(id); if (i < 0) return;
    LmStateArchiveHeader *h = (LmStateArchiveHeader*)Worker::Files[i].data;
    h->rawSize = raw; h->payloadSize = payload;
}
API void client_shutdown() {
    Worker::reset_worker();
    VirtualFree((void*)(uintptr_t)kSnapshotBase, 0, MEM_RELEASE);
    VirtualFree((void*)(uintptr_t)SUSAMUNE_LM_CACHE_PPC_BASE, 0, MEM_RELEASE);
    VirtualFree((void*)(uintptr_t)kPackedBase, 0, MEM_RELEASE);
}
API u32 client_catalog_count() { return sCatalogCount; }
API u32 client_delete_token(u32 index) { return deleteToken(index); }
API int client_delete(u32 id, u32 token) { return beginStorage(LM_STATE_STORAGE_DELETE, id, nullptr, token); }
API void client_storage_capability(u32 version, u32 available) {
    Worker::TestMailbox.version = version;
    Worker::TestMailbox.available = available;
}
API u32 client_catalog_next() { return sCatalogNext; }
API u32 client_catalog_more() { return sCatalogMore; }
API u32 client_catalog_id(u32 i) { return i < sCatalogCount ? sCatalog[i].id : 0; }
API u32 client_catalog_bytes(u32 i) { return i < sCatalogCount ? sCatalog[i].bytes : 0; }
API int client_catalog_compatible(u32 i) { return catalogEntryCompatible(i); }
API const char *client_catalog_entry_text(u32 i) { return catalogEntryStatus(i); }
API const char *client_catalog_name(u32 i) { return i < sCatalogCount ? sCatalog[i].name : ""; }
API const char *client_catalog_text() { return sCatalogText; }
API u32 client_directory_open() { return Worker::DirectoryOpen; }
API u32 client_io_calls(u32 operation) { return Worker::Calls[operation]; }
API u32 client_slot_crc() { return storageCrc((void*)(uintptr_t)kSnapshotBase, LM_STATE_STORAGE_PAYLOAD_MAX); }
API u32 client_file_crc(u32 id) {
    const int i = clientFile(id);
    return i < 0 ? 0u : storageCrc(Worker::Files[i].data, Worker::Files[i].size);
}
API void client_name_error_reply() {
    Worker::TestMailbox.status = LM_STATE_STORAGE_NAME_ERROR;
    serviceStorage();
}
API void client_add_file(const char *name) {
    int i = Worker::Add(name); if (i >= 0) Worker::Files[i].size = 0;
}
API void client_bad_catalog(u32 field) {
    if (field == 0) Worker::TestMailbox.catalogCount = LM_STATE_CATALOG_CAPACITY + 1;
    if (field == 1) ++Worker::TestMailbox.catalogAfter;
    if (field == 2) ++Worker::TestMailbox.catalogNext;
    if (field == 3) Worker::TestMailbox.catalogMore = 2;
    if (field == 4) Worker::TestMailbox.catalog[0].id = 0;
    if (field == 5) Worker::TestMailbox.catalog[0].flags = 2;
    if (field == 6) Worker::TestMailbox.catalog[0].reserved = 1;
    if (field == 7) Worker::TestMailbox.responseLength = 1;
    if (field == 8) memset(Worker::TestMailbox.catalog[0].name, 'A', LM_STATE_NAME_BYTES);
    if (field == 9) Worker::TestMailbox.catalog[0].name[0] = '\n';
    if (field == 10) Worker::TestMailbox.catalog[0].name[LM_STATE_NAME_BYTES - 1] = 'A';
    serviceStorage();
}
