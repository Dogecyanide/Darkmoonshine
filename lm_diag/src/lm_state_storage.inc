// Included inside lm_state.cpp's private namespace: the archive trailer owns
// the census belonging to its snapshot, never the currently observed room.
constexpr u32 kResidentSlots = 1u;
constexpr u32 kCodecWorkspace = SUSAMUNE_LM_CACHE_PPC_BASE + SUSAMUNE_LM_MAILBOX_SIZE;
constexpr u32 kSlotStagingStart = kCodecWorkspace + LM_STATE_DEFLATE_WORKSPACE;
constexpr u32 kSlotStagingSize = SUSAMUNE_LM_CACHE_SIZE - SUSAMUNE_LM_MAILBOX_SIZE - LM_STATE_DEFLATE_WORKSPACE;
static_assert(sizeof(LmStateStorageMailbox) <= SUSAMUNE_LM_MAILBOX_SIZE, "LM mailbox overlaps codec workspace");
constexpr u32 kSlotTrailerSize = sizeof(VolumeCensus) +
    sizeof(ResourceCensus) + sizeof(ModelCensusMetadata) + sizeof(LmPersistentProfile);
static_assert(kSlotTrailerSize <= 0x2000u, "LM slot census trailer outgrew its reserve");
static_assert(LM_STATE_STORAGE_PAYLOAD_MAX < kSnapshotCapacity,
              "SD imports must stop before all live census scratch");

struct ResidentSlot {
    u32 rawSize;
    u32 offset;
    u32 packedSize;
    u32 packedCrc;
    u32 codec;
};
ResidentSlot sSlots[kResidentSlots] = {};
u32 sSelectedSlot;
u32 sCacheUsed;
u32 sSlotCacheStart;
u32 sSlotCacheSize;
u32 sPackedNeeded;
char sCapacityMessage[64];
u32 sStorageSeq;
u32 sStorageCommand;
u32 sStorageRequestedId;
u32 sStorageExpectedPayload;
bool sStorageExportPortable;
char sStorageName[LM_STATE_NAME_BYTES] = {};
u32 sArchiveId;
u32 sArchiveSession;
u32 sArchiveBuild;
u64 sArchiveKey0;
u64 sArchiveKey1;
u32 sPersistentKeyId;
u64 sPersistentKey0, sPersistentKey1;
const char *sStorageText = "READY";
char sStorageMessage[96];
bool sStorageInitialized;
bool sStorageStartupPending;
LmStateSegment sStaging[2] = {};
LmStateCatalogEntry sCatalog[LM_STATE_CATALOG_CAPACITY] = {};
u32 sCatalogCount, sCatalogCursor, sCatalogNext;
u32 sCatalogSeq, sCatalogIdentity[LM_STATE_CATALOG_CAPACITY];
bool sCatalogMore;
const char *sCatalogText = "OPEN TO LIST SD FILES";

u32 refreshPersistentKey() {
#if IS_EMULATOR
    sPersistentKeyReady = false;
    return LM_STATE_KEY_UNAVAILABLE;
#else
    volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        const_cast<unsigned int *>(&m->keyMagic), 64u);
    sPersistentKeyReady = false;
    if (m->keyMagic != LM_STATE_KEY_MAGIC || m->keyVersion != LM_STATE_KEY_VERSION)
        return LM_STATE_KEY_UNAVAILABLE;
    for (u32 i = 0u; i < 7u; ++i) if (m->keyReserved[i]) return LM_STATE_KEY_INVALID;
    if (m->keyStatus == LM_STATE_KEY_READY && m->keyId && m->keyConfigId &&
        (m->keyWords[0] | m->keyWords[1] | m->keyWords[2] | m->keyWords[3])) {
        sPersistentKeyId = m->keyId;
        sPersistentKey0 = (static_cast<u64>(m->keyWords[0]) << 32) | m->keyWords[1];
        sPersistentKey1 = (static_cast<u64>(m->keyWords[2]) << 32) | m->keyWords[3];
        sPersistentConfigId = m->keyConfigId;
        sPersistentKeyReady = true;
    }
    return m->keyStatus;
#endif
}

const char *catalogEntryStatus(u32 index) {
    if (index >= sCatalogCount) return "NO FILE";
    const LmStateCatalogEntry &entry = sCatalog[index];
    if (!(entry.flags & LM_STATE_CATALOG_HEADER_VALID)) return "INVALID FILE";
    if (entry.buildCrc != sArchiveBuild) return "DIFFERENT BUILD";
    if (entry.snapshotVersion != kSnapshotVersion) return "STATE FORMAT";
    if (entry.flags & LM_STATE_CATALOG_PERSISTENT) {
        if (!sPersistentKeyReady || entry.session != sPersistentKeyId) return "DIFFERENT SD KEY";
        if (entry.reserved != sPersistentConfigId) return "DIFFERENT SETUP";
        return "REBOOT ARCHIVE";
    }
    if (entry.session != sArchiveSession) return "DIFFERENT SESSION";
    return "SAME SESSION";
}

bool catalogEntryCompatible(u32 index) {
    return index < sCatalogCount &&
        (sCatalog[index].flags & LM_STATE_CATALOG_HEADER_VALID) &&
        sCatalog[index].buildCrc == sArchiveBuild &&
        sCatalog[index].snapshotVersion == kSnapshotVersion &&
        ((sCatalog[index].flags & LM_STATE_CATALOG_PERSISTENT) ?
         (sPersistentKeyReady && sCatalog[index].session == sPersistentKeyId &&
          sCatalog[index].reserved == sPersistentConfigId) :
         sCatalog[index].session == sArchiveSession);
}

void storageMessage(u32 command, u32 id, const char *detail, const char *suffix = "") {
    char *out = sStorageMessage;
    const char *label = command == LM_STATE_STORAGE_EXPORT ? "EXPORT " :
        command == LM_STATE_STORAGE_RENAME ? "RENAME " :
        command == LM_STATE_STORAGE_DELETE ? "DELETE " : "IMPORT ";
    while (*label) *out++ = *label++;
    u32 divisor = 10000000u;
    for (u32 i = 0; i < 8u; ++i) {
        *out++ = static_cast<char>('0' + id / divisor % 10u);
        divisor /= 10u;
    }
    *out++ = ':'; *out++ = ' ';
    const char *parts[] = {detail, suffix};
    for (u32 i = 0; i < 2u; ++i)
        for (const char *p = parts[i]; *p && out < sStorageMessage + sizeof(sStorageMessage) - 1u; ++p)
            *out++ = *p;
    *out = 0;
    sStorageText = sStorageMessage;
}

char *capacityNumber(char *out, u32 value) {
    char digits[10]; u32 count = 0u;
    do { digits[count++] = static_cast<char>('0' + value % 10u); value /= 10u; } while (value);
    while (count) *out++ = digits[--count];
    *out++ = 'K'; return out;
}

void capacityFailure(u32 freeBytes) {
    char *p = sCapacityMessage;
    const char *labels[] = {"FULL NEED ", " FREE ", " STAGE "};
    const u32 values[] = {(sPackedNeeded + 1023u) >> 10, freeBytes >> 10,
                         (sStaging[0].size + sStaging[1].size) >> 10};
    for (u32 i = 0; i < 3u; ++i) {
        for (const char *s = labels[i]; *s; ++s) *p++ = *s;
        p = capacityNumber(p, values[i]);
    }
    *p = 0; sStorageText = sCapacityMessage;
}

u32 storageCrc(const void *data, u32 size) {
    const u8 *p = static_cast<const u8 *>(data);
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < size; ++i) crc = crcByte(crc, p[i]);
    return crc ^ 0xFFFFFFFFu;
}

bool storageInFlight() { return sStorageCommand != 0u || sStorageStartupPending; }

void flushRawSlot(u32 size) {
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        reinterpret_cast<void *>(kSnapshotBase), size);
    asm volatile("sync" ::: "memory");
}

void copySlotTrailer(bool capture, u32 rawSize) {
    u32 target = kSnapshotBase + rawSize;
    if (capture) {
        copyBytes(reinterpret_cast<void *>(target), &sSavedVolumeCensus,
                  sizeof(sSavedVolumeCensus));
        target += sizeof(sSavedVolumeCensus);
        copyBytes(reinterpret_cast<void *>(target), &sSavedResourceCensus,
                  sizeof(sSavedResourceCensus));
        target += sizeof(sSavedResourceCensus);
        copyBytes(reinterpret_cast<void *>(target), sSavedModelCensus.metadata,
                  sizeof(ModelCensusMetadata));
        target += sizeof(ModelCensusMetadata);
        copyBytes(reinterpret_cast<void *>(target), &sSavedPersistentProfile,
                  sizeof(sSavedPersistentProfile));
    } else {
        copyBytes(&sSavedVolumeCensus, reinterpret_cast<void *>(target),
                  sizeof(sSavedVolumeCensus));
        target += sizeof(sSavedVolumeCensus);
        copyBytes(&sSavedResourceCensus, reinterpret_cast<void *>(target),
                  sizeof(sSavedResourceCensus));
        target += sizeof(sSavedResourceCensus);
        copyBytes(sSavedModelCensus.metadata, reinterpret_cast<void *>(target),
                  sizeof(ModelCensusMetadata));
        target += sizeof(ModelCensusMetadata);
        copyBytes(&sSavedPersistentProfile, reinterpret_cast<void *>(target),
                  sizeof(sSavedPersistentProfile));
    }
}

bool rawSlotValid(u32 rawSize) {
    const SnapshotHeader *h = reinterpret_cast<const SnapshotHeader *>(kSnapshotBase);
    if (rawSize < kHeaderSize ||
        rawSize > LM_STATE_STORAGE_PAYLOAD_MAX - kSlotTrailerSize ||
        !basicHeaderValid(h) || h->totalSize > rawSize || rawSize - h->totalSize < 64u ||
        snapshotStoredSize(h) != rawSize ||
        snapshotChecksum(h) != h->checksum ||
        !snapshotCompanionValid(h, rawSize)) return false;
    const VolumeCensus *volume = reinterpret_cast<const VolumeCensus *>(kSnapshotBase + rawSize);
    const ResourceCensus *resource = reinterpret_cast<const ResourceCensus *>(volume + 1);
    const ModelCensusMetadata *model = reinterpret_cast<const ModelCensusMetadata *>(resource + 1);
    if (volume->generation != h->generation || volume->count > kMaxVolumes ||
        resource->generation != h->generation ||
        model->generation != h->generation) return false;
    const LmPersistentProfile *profile = reinterpret_cast<const LmPersistentProfile *>(model + 1);
    if (profile->magic) {
        if (!LmPersistentProfileValid(profile, h->generation)) return false;
    } else {
        const u32 *words = reinterpret_cast<const u32 *>(profile);
        for (u32 i = 0u; i < sizeof(*profile) / 4u; ++i) if (words[i]) return false;
    }
    // These are the only restore destinations carried by payload records.
    // All other destinations come from compiled-in static ranges/live heaps.
    for (u32 i = 0; i < kCameraObjectCount; ++i) {
        const u32 record = cameraObjectRecordAddress(i);
        const u32 target = readWord(record), bytes = readWord(record + 4u);
        if ((bytes != 0u && bytes != kCameraObjectSize) ||
            !isMem1ByteRange(target, kCameraObjectSize) ||
            !rangeInside(target, target + kCameraObjectSize,
                         h->rootHeapStart, h->rootHeapEnd)) return false;
    }
    return true;
}

void clearSlotDiagnostics() {
    clearEpochMismatch();
    clearVolumeDiff();
    clearResourceDiff();
    clearModelDiff();
}

void removePackedSlot(u32 slot) {
    sSlots[slot].packedSize = 0u;
    sCacheUsed = 0u;
}

u32 stagingCrc(u32 size) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 segment = 0u; segment < 2u; ++segment) {
        const u32 bytes = size < sStaging[segment].size ? size : sStaging[segment].size;
        for (u32 i = 0u; i < bytes; ++i) crc = crcByte(crc, sStaging[segment].data[i]);
        size -= bytes;
    }
    return crc ^ 0xFFFFFFFFu;
}

u32 sPackedCodec;

u32 packCurrent() {
    const u32 raw = sSlots[sSelectedSlot].rawSize;
    if (!raw) return 0u;
    // Both rollback segments are outside the entire ARM import window.
    sStaging[0] = {reinterpret_cast<u8 *>(sSlotCacheStart), sSlotCacheSize};
    sStaging[1] = {reinterpret_cast<u8 *>(kSlotStagingStart), kSlotStagingSize};
    sPackedCodec = LM_STATE_FAST_CODEC;
    sPackedNeeded = LmStateDeflateFast(reinterpret_cast<const u8 *>(kSnapshotBase),
        raw + kSlotTrailerSize, sStaging, reinterpret_cast<void *>(kCodecWorkspace));
    if (!sPackedNeeded) return 0u;
    if (sPackedNeeded > sStaging[0].size + sStaging[1].size) {
        sPackedCodec = LM_STATE_DEFLATE_CODEC;
        sPackedNeeded = LmStateDeflate(reinterpret_cast<const u8 *>(kSnapshotBase),
            raw + kSlotTrailerSize, sStaging, reinterpret_cast<void *>(kCodecWorkspace));
    }
    return sPackedNeeded <= sStaging[0].size + sStaging[1].size ? sPackedNeeded : 0u;
}

bool unpackSlot(const ResidentSlot &slot, u8 *destination) {
    const LmStateSegment segments[2] = {
        {reinterpret_cast<u8 *>(sSlotCacheStart), sSlotCacheSize},
        {reinterpret_cast<u8 *>(kSlotStagingStart), kSlotStagingSize}};
    return LmStateInflate(segments, slot.packedSize, destination,
        slot.rawSize + kSlotTrailerSize, reinterpret_cast<void *>(kCodecWorkspace));
}

bool packedSlotValid(const ResidentSlot &slot) {
    return (slot.codec == LM_STATE_DEFLATE_CODEC || slot.codec == LM_STATE_FAST_CODEC) && slot.packedSize &&
        slot.packedSize <= sSlotCacheSize + kSlotStagingSize &&
        slot.rawSize >= kHeaderSize &&
        slot.rawSize <= LM_STATE_STORAGE_PAYLOAD_MAX - kSlotTrailerSize &&
        stagingCrc(slot.packedSize) == slot.packedCrc &&
        unpackSlot(slot, nullptr);
}

bool switchSlot(u32 target) {
    return target == 0u && !storageInFlight() && !sMenuRequest;
}

void initializeStorage() {
    if (sStorageInitialized) return;
    // Time-base entropy plus uncaptured process state makes old-session files
    // unusable even when a reset happens to reproduce identical heap addresses.
    u32 high, low, later;
    asm volatile("mftbu %0\n\tmftb %1" : "=r"(high), "=r"(low));
    sArchiveKey0 = (static_cast<u64>(high) << 32) | low;
    sArchiveKey1 = LmStateAuthenticate(reinterpret_cast<const u8 *>(0x80000000u),
        0x1000u, sArchiveKey0, 0x4C4D4A534553534EULL);
    asm volatile("mftb %0" : "=r"(later));
    sArchiveSession = later ^ high ^ 0x4C4D4A31u;
    const SusamuneCrashReport *report = SUSAMUNE_CRASH_PPC_PTR;
    sArchiveBuild = report->modFileCrc32;
#if IS_EMULATOR
    sSlotCacheStart = SUSAMUNE_LM_DOLPHIN_PACKED_PPC_BASE;
    sSlotCacheSize = SUSAMUNE_LM_PACKED_MAX_SIZE;
#else
    volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        const_cast<LmStateStorageMailbox *>(m), 64u);
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        const_cast<unsigned int *>(&m->cacheMagic), 32u);
    const u32 prefix = LmStateCachePrefix(m->filePatchCount);
    if (m->magic == LM_STATE_STORAGE_MAGIC &&
        m->version == LM_STATE_STORAGE_VERSION && m->cacheMagic == LM_STATE_CACHE_MAGIC &&
        m->cachePhysicalBase == NIN_MEM2_FILE_PATCH_PHYS_BASE + prefix &&
        m->cacheSize == NIN_MEM2_FILE_PATCH_SIZE - prefix && m->cacheSize) {
        sSlotCacheStart = m->cachePhysicalBase + 0x80000000u;
        sSlotCacheSize = m->cacheSize;
    }
    sStorageSeq = m->requestSeq;
    // A reset cancels any old in-flight ARM transfer before it can touch the
    // new process's slot. Response ownership remains entirely on ARM.
    m->processSession = sArchiveSession;
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        const_cast<LmStateStorageMailbox *>(m), 32u);
    sStorageStartupPending = m->magic == LM_STATE_STORAGE_MAGIC &&
        m->version == LM_STATE_STORAGE_VERSION && m->ackSeq != sStorageSeq;
#endif
    sStorageInitialized = true;
}

bool storageStartupReady() {
#if !IS_EMULATOR
    if (sStorageStartupPending) {
        volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
        reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
            const_cast<unsigned int *>(&m->magic), 32u);
        sStorageStartupPending = m->ackSeq != sStorageSeq;
    }
#endif
    if (sStorageStartupPending) return false;
#if !IS_EMULATOR
    if (!sStorageCommand && refreshPersistentKey() == LM_STATE_KEY_MISSING) {
        volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
        m->keySeed[0] = static_cast<u32>(sArchiveKey0 >> 32);
        m->keySeed[1] = static_cast<u32>(sArchiveKey0);
        m->keySeed[2] = static_cast<u32>(sArchiveKey1 >> 32);
        m->keySeed[3] = static_cast<u32>(sArchiveKey1);
        for (u32 i = 0; i < 4u; ++i) m->keySeedReserved[i] = 0u;
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            const_cast<unsigned int *>(m->keySeed), 32u);
        m->command = LM_STATE_STORAGE_KEY;
        m->archiveId = m->payloadSize = 0u;
        if (++sStorageSeq == 0u) ++sStorageSeq;
        m->requestSeq = sStorageSeq;
        asm volatile("sync" ::: "memory");
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            const_cast<LmStateStorageMailbox *>(m), 32u);
        sStorageCommand = LM_STATE_STORAGE_KEY;
        sStorageRequestedId = sStorageExpectedPayload = 0u;
        sStorageText = "PREPARING SD KEY...";
    }
#endif
    return true;
}

void savedSlotCommitted() {
    ResidentSlot &entry = sSlots[sSelectedSlot];
    entry.rawSize = snapshotStoredSize(reinterpret_cast<const SnapshotHeader *>(kSnapshotBase));
    if (!entry.rawSize || entry.rawSize > LM_STATE_STORAGE_PAYLOAD_MAX - kSlotTrailerSize) {
        entry.rawSize = 0u;
        sStorageText = "STATE STORAGE BOUNDS";
        return;
    }
    copySlotTrailer(true, entry.rawSize);
    flushRawSlot(entry.rawSize + kSlotTrailerSize);
}

bool storageAvailable() {
#if IS_EMULATOR
    sStorageText = "SD: WII ONLY";
    return false;
#else
    volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        const_cast<unsigned int *>(&m->magic), 32u);
    if (m->magic != LM_STATE_STORAGE_MAGIC ||
        m->version != LM_STATE_STORAGE_VERSION || !m->available) {
        sStorageText = "SD UNAVAILABLE"; return false;
    }
    return true;
#endif
}

bool backupImportTarget() {
    if (!sSlots[sSelectedSlot].rawSize) return true;
    ResidentSlot &slot = sSlots[sSelectedSlot];
    if (!rawSlotValid(slot.rawSize)) { sStorageText = "OLD SLOT INVALID - NOT STARTED"; return false; }
    const u32 size = packCurrent();
    if (!size) {
        capacityFailure(sSlotCacheSize + kSlotStagingSize); return false;
    }
    slot.offset = 0u;
    slot.packedSize = size;
    slot.codec = sPackedCodec;
    slot.packedCrc = stagingCrc(size);
    if (!packedSlotValid(slot)) {
        removePackedSlot(sSelectedSlot);
        sStorageText = "BACKUP CRC ERROR - NOT STARTED";
        return false;
    }
    sCacheUsed = size < sSlotCacheSize ? size : sSlotCacheSize;
    return true;
}

bool prepareStorageName(const char *name) {
    for (u32 i = 0u; i < LM_STATE_NAME_BYTES; ++i) sStorageName[i] = 0;
    if (!name) return true;
    for (u32 i = 0u; i < LM_STATE_NAME_BYTES; ++i) {
        const u8 c = static_cast<u8>(name[i]);
        if (!c) return true;
        if (i + 1u == LM_STATE_NAME_BYTES || c < 0x20u || c > 0x7Eu) return false;
        sStorageName[i] = static_cast<char>(c);
    }
    return false;
}

u32 deleteToken(u32 index) {
    return index < sCatalogCount && !storageInFlight() && !sMenuRequest ? sCatalogSeq : 0u;
}

bool beginStorage(u32 command, u32 id, const char *name = nullptr, u32 token = 0u) {
    initializeStorage();
    const bool catalog = command == LM_STATE_STORAGE_CATALOG;
    const bool rename = command == LM_STATE_STORAGE_RENAME;
    const bool deleting = command == LM_STATE_STORAGE_DELETE;
    if (storageInFlight() || sMenuRequest) {
        if (catalog) sCatalogText = "BUSY - NOT STARTED";
        else storageMessage(command, id, "BUSY - NOT STARTED");
        return false;
    }
    if (command != LM_STATE_STORAGE_EXPORT && command != LM_STATE_STORAGE_IMPORT && !catalog && !rename && !deleting) {
        storageMessage(command, id, "INVALID COMMAND"); return false;
    }
    if ((command == LM_STATE_STORAGE_EXPORT || rename) && !prepareStorageName(name)) {
        storageMessage(command, id, "INVALID NAME"); return false;
    }
    const char *previousText = sStorageText;
    const bool available = storageAvailable();
    if (catalog) {
        if (!available) sCatalogText = sStorageText;
        sStorageText = previousText;
    }
    if (!available) return false;
    volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
    ResidentSlot &slot = sSlots[sSelectedSlot];
    if (command == LM_STATE_STORAGE_EXPORT) {
        if (!slot.rawSize || !rawSlotValid(slot.rawSize)) {
            storageMessage(command, id, "SAVE A STATE FIRST"); return false;
        }
        LmStateArchiveHeader header = {};
        const SnapshotHeader *snapshot = reinterpret_cast<const SnapshotHeader *>(kSnapshotBase);
        header.magic = LM_STATE_ARCHIVE_MAGIC;
        header.version = LM_STATE_ARCHIVE_VERSION;
        header.headerSize = sizeof(header);
        header.payloadSize = slot.rawSize + kSlotTrailerSize;
        header.gameId = SUSAMUNE_MOD_GAME_ID_LMJ;
        header.snapshotVersion = kSnapshotVersion;
        header.buildCrc = sArchiveBuild;
        header.session = sArchiveSession;
        header.rawSize = slot.rawSize;
        header.trailerSize = kSlotTrailerSize;
        header.payloadCrc = storageCrc(reinterpret_cast<const void *>(kSnapshotBase), header.payloadSize);
        header.generation = snapshot->generation;
        const bool portable = sPersistentKeyReady &&
            LmPersistentProfileValid(&sSavedPersistentProfile, snapshot->generation) &&
            sSavedPersistentProfile.configId == sPersistentConfigId;
        sStorageExportPortable = portable;
        if (portable) {
            header.version = LM_STATE_ARCHIVE_PERSISTENT_VERSION;
            header.session = sPersistentKeyId;
            header.reserved0 = LM_STATE_PERSISTENT_PROFILE;
            header.reserved1 = sPersistentConfigId;
        }
        const u64 tag = portable ? LmStateAuthenticateArchive(reinterpret_cast<const u8 *>(&header),
            reinterpret_cast<const u8 *>(kSnapshotBase), header.payloadSize, sPersistentKey0, sPersistentKey1) :
            LmStateAuthenticate(reinterpret_cast<const u8 *>(kSnapshotBase),
                header.payloadSize, sArchiveKey0, sArchiveKey1);
        header.authHigh = static_cast<u32>(tag >> 32);
        header.authLow = static_cast<u32>(tag);
        copyBytes(const_cast<LmStateArchiveHeader *>(&m->header), &header, sizeof(header));
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            const_cast<LmStateArchiveHeader *>(&m->header), sizeof(header));
        m->payloadSize = header.payloadSize;
        sStorageExpectedPayload = header.payloadSize;
        flushRawSlot(header.payloadSize);
    } else if (command == LM_STATE_STORAGE_IMPORT) {
        if (id == 0u || id > LM_STATE_STORAGE_MAX_ID) {
            storageMessage(command, id, "INVALID ID"); return false;
        }
        if (!backupImportTarget()) return false;
        m->payloadSize = 0u;
        sStorageExpectedPayload = 0u;
        clearSlotDiagnostics();
        // No dirty PPC cache lines may race ARM writes to the import window.
        flushRawSlot(LM_STATE_STORAGE_PAYLOAD_MAX);
    } else if (rename || deleting) {
        if (id == 0u || id > LM_STATE_STORAGE_MAX_ID) {
            storageMessage(command, id, "INVALID ID"); return false;
        }
        m->payloadSize = sStorageExpectedPayload = 0u;
        if (deleting) {
            u32 index = 0u;
            while (index < sCatalogCount && sCatalog[index].id != id) ++index;
            if (!token || token != sCatalogSeq || index == sCatalogCount) {
                storageMessage(command, id, "REFRESH LIST FIRST"); return false;
            }
            m->deleteCatalogSeq = token;
            m->deleteHeaderCrc = sCatalogIdentity[index];
            m->deleteBytes = sCatalog[index].bytes;
            m->deleteVersion = LM_STATE_STORAGE_VERSION;
            for (u32 i = 0; i < 4u; ++i) m->deleteReserved[i] = 0u;
            reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
                const_cast<unsigned int *>(&m->deleteCatalogSeq), 32u);
        }
    } else {
        if (id > LM_STATE_STORAGE_MAX_ID) { sCatalogText = "INVALID PAGE"; return false; }
        m->payloadSize = sStorageExpectedPayload = 0u;
        sCatalogCount = 0u; sCatalogMore = false;
        sCatalogSeq = 0u;
        sCatalogCursor = sCatalogNext = id;
    }
    if (command == LM_STATE_STORAGE_EXPORT || rename) {
        copyBytes(const_cast<char *>(m->requestName), sStorageName, sizeof(sStorageName));
        reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
            const_cast<char *>(m->requestName), sizeof(sStorageName));
    }
    m->command = command;
    m->archiveId = id;
    if (++sStorageSeq == 0u) ++sStorageSeq;
    m->requestSeq = sStorageSeq;
    asm volatile("sync" ::: "memory");
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(
        const_cast<LmStateStorageMailbox *>(m), 32u);
    sStorageCommand = command;
    sStorageRequestedId = id;
    if (catalog) sCatalogText = "READING SD...";
    else storageMessage(command, id, "WORKING...");
    return true;
}

const char *importedArchiveError(const LmStateArchiveHeader &h) {
    const bool portable = h.version == LM_STATE_ARCHIVE_PERSISTENT_VERSION;
    if (portable) {
        if (!sPersistentKeyReady || h.session != sPersistentKeyId) return "DIFFERENT SD KEY";
        if (h.reserved0 != LM_STATE_PERSISTENT_PROFILE || h.reserved1 != sPersistentConfigId)
            return "DIFFERENT SETUP";
    } else if (h.session != sArchiveSession) return "DIFFERENT SESSION";
    if (h.buildCrc != sArchiveBuild) return "DIFFERENT BUILD";
    if (h.snapshotVersion != kSnapshotVersion) return "STATE FORMAT";
    if (h.magic != LM_STATE_ARCHIVE_MAGIC || (!portable && h.version != LM_STATE_ARCHIVE_VERSION) ||
        h.headerSize != sizeof(h) || h.gameId != SUSAMUNE_MOD_GAME_ID_LMJ ||
        h.snapshotVersion != kSnapshotVersion || h.buildCrc != sArchiveBuild ||
        h.trailerSize != kSlotTrailerSize ||
        h.rawSize < kHeaderSize || h.rawSize > LM_STATE_STORAGE_PAYLOAD_MAX - kSlotTrailerSize ||
        h.payloadSize != h.rawSize + kSlotTrailerSize ||
        (!portable && (h.reserved0 || h.reserved1))) return "BAD HEADER";
    if (storageCrc(reinterpret_cast<const void *>(kSnapshotBase), h.payloadSize) != h.payloadCrc) return "CRC ERROR";
    const u64 tag = portable ? LmStateAuthenticateArchive(reinterpret_cast<const u8 *>(&h),
        reinterpret_cast<const u8 *>(kSnapshotBase), h.payloadSize, sPersistentKey0, sPersistentKey1) :
        LmStateAuthenticate(reinterpret_cast<const u8 *>(kSnapshotBase),
            h.payloadSize, sArchiveKey0, sArchiveKey1);
    if (h.authHigh != static_cast<u32>(tag >> 32) || h.authLow != static_cast<u32>(tag)) return "AUTH ERROR";
    const LmPersistentProfile *profile = reinterpret_cast<const LmPersistentProfile *>(
        kSnapshotBase + h.payloadSize - sizeof(LmPersistentProfile));
    if (portable && (!LmPersistentProfileValid(profile, h.generation) ||
        profile->configId != sPersistentConfigId)) return "BAD REBOOT PROFILE";
    return rawSlotValid(h.rawSize) &&
        reinterpret_cast<const SnapshotHeader *>(kSnapshotBase)->generation == h.generation ? nullptr : "BAD STATE";
}

void serviceStorage() {
    if (!sStorageCommand) return;
    volatile LmStateStorageMailbox *m = LM_STATE_STORAGE_PPC_PTR;
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(const_cast<unsigned int *>(&m->magic), 32u);
    if (m->ackSeq != sStorageSeq) return;
    reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
        const_cast<unsigned int *>(&m->responseSession), 32u);
    const bool receipt = LmStateStorageReceiptMatches(m, sStorageSeq, sArchiveSession,
        sStorageCommand, sStorageRequestedId);
    bool nameOnlyFailure = receipt && sStorageCommand == LM_STATE_STORAGE_EXPORT &&
        m->status == LM_STATE_STORAGE_NAME_ERROR;
    const char *error = nullptr;
    if (!receipt) error = "BAD RESPONSE";
    else switch (m->status) {
    case LM_STATE_STORAGE_OK: break;
    case LM_STATE_STORAGE_NOT_FOUND: error = "NOT FOUND"; break;
    case LM_STATE_STORAGE_WRONG_SESSION: error = "DIFFERENT SESSION"; break;
    case LM_STATE_STORAGE_UNAVAILABLE: error = "SD UNAVAILABLE"; break;
    case LM_STATE_STORAGE_BAD_FILE: error = "BAD FILE"; break;
    case LM_STATE_STORAGE_FULL: error = "SD FULL"; break;
    case LM_STATE_STORAGE_CANCELLED: error = "CANCELLED"; break;
    case LM_STATE_STORAGE_KEY_ERROR: error = "SD KEY ERROR"; break;
    case LM_STATE_STORAGE_CONFIG_ERROR: error = "DIFFERENT SETUP"; break;
    case LM_STATE_STORAGE_STALE_SELECTION: error = "FILE CHANGED - REFRESH LIST"; break;
    case LM_STATE_STORAGE_DELETE_CLEANUP: error = "DELETED; NAME CLEANUP FAILED"; break;
    case LM_STATE_STORAGE_NAME_ERROR: error = nameOnlyFailure ? "NAME NOT SAVED" : "BAD RESPONSE"; break;
    default: error = "FILE ERROR"; break;
    }
    if (sStorageCommand == LM_STATE_STORAGE_KEY) {
        if (m->transferred || m->responseLength || m->resultId) error = "BAD RESPONSE";
        refreshPersistentKey();
        sStorageText = error ? error : sPersistentKeyReady ? "SD KEY READY" : "SD KEY ERROR";
        sStorageCommand = 0u;
        return;
    }
    if (sStorageCommand == LM_STATE_STORAGE_DELETE) {
        if (receipt && (m->transferred || m->responseLength || m->resultId != sStorageRequestedId))
            error = "BAD RESPONSE";
        storageMessage(sStorageCommand, sStorageRequestedId, error ? error : "DELETED FROM SD");
        sCatalogSeq = sCatalogCount = 0u;
        sCatalogMore = false;
        sCatalogText = "REFRESH TO LIST SD FILES";
        sStorageCommand = 0u;
        return;
    }
    if (sStorageCommand == LM_STATE_STORAGE_CATALOG) {
        if (!error) {
            reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
                const_cast<unsigned int *>(&m->catalogCount), 32u + sizeof(sCatalog));
            if (m->transferred || m->responseLength || m->catalogCount > LM_STATE_CATALOG_CAPACITY ||
                m->catalogAfter != sStorageRequestedId || m->catalogMore > 1u ||
                (m->catalogMore && m->catalogCount != LM_STATE_CATALOG_CAPACITY)) error = "BAD RESPONSE";
            u32 previous = sStorageRequestedId;
            for (u32 i = 0; !error && i < m->catalogCount; ++i) {
                const volatile LmStateCatalogEntry &entry = m->catalog[i];
                if (entry.id <= previous || entry.id > LM_STATE_STORAGE_MAX_ID ||
                    (entry.flags != 0u && entry.flags != LM_STATE_CATALOG_HEADER_VALID &&
                     entry.flags != (LM_STATE_CATALOG_HEADER_VALID | LM_STATE_CATALOG_PERSISTENT)) ||
                    (!(entry.flags & LM_STATE_CATALOG_PERSISTENT) && entry.reserved) ||
                    !LmStateNameValid(const_cast<const char *>(entry.name))) error = "BAD RESPONSE";
                previous = entry.id;
            }
            if (!error && m->catalogNext != previous) error = "BAD RESPONSE";
            if (!error) {
                reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
                    const_cast<unsigned int *>(m->catalogIdentity), sizeof(sCatalogIdentity));
                copyBytes(sCatalogIdentity, const_cast<const unsigned int *>(m->catalogIdentity), sizeof(sCatalogIdentity));
                sCatalogSeq = sStorageSeq;
                copyBytes(sCatalog, const_cast<const LmStateCatalogEntry *>(m->catalog), sizeof(sCatalog));
                sCatalogCount = m->catalogCount;
                sCatalogCursor = m->catalogAfter;
                sCatalogNext = m->catalogNext;
                sCatalogMore = m->catalogMore != 0u;
            }
        }
        sCatalogText = error ? error : sCatalogCount ? "SELECT A FILE" : "NO ARCHIVES FOUND";
        sStorageCommand = 0u;
        return;
    }
    if ((!error || nameOnlyFailure) && (m->transferred != m->responseLength ||
        m->resultId == 0u || m->resultId > LM_STATE_STORAGE_MAX_ID ||
        (sStorageCommand == LM_STATE_STORAGE_EXPORT ?
         (m->resultId < sStorageRequestedId || m->transferred != sStorageExpectedPayload) :
         (m->resultId != sStorageRequestedId ||
          (sStorageCommand == LM_STATE_STORAGE_RENAME && m->transferred))))) {
        error = "BAD RESPONSE";
        nameOnlyFailure = false;
    }
    bool success = error == nullptr;
    const char *suffix = "";
    if (sStorageCommand == LM_STATE_STORAGE_IMPORT) {
        reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
            const_cast<LmStateArchiveHeader *>(&m->header), sizeof(m->header));
        reinterpret_cast<CacheRangeFn>(kDCInvalidateRangeAddr)(
            reinterpret_cast<void *>(kSnapshotBase), LM_STATE_STORAGE_PAYLOAD_MAX);
        LmStateArchiveHeader header;
        copyBytes(&header, const_cast<const LmStateArchiveHeader *>(&m->header), sizeof(header));
        if (success) {
            error = m->transferred == header.payloadSize ? importedArchiveError(header) : "SHORT TRANSFER";
            success = error == nullptr;
        }
        ResidentSlot &slot = sSlots[sSelectedSlot];
        if (success) {
            slot.rawSize = header.rawSize;
            sPersistentLoaded = header.version == LM_STATE_ARCHIVE_PERSISTENT_VERSION;
            if (header.generation > sGeneration) sGeneration = header.generation;
        } else if (slot.packedSize && packedSlotValid(slot)) {
            if (!unpackSlot(slot, reinterpret_cast<u8 *>(kSnapshotBase)) || !rawSlotValid(slot.rawSize)) {
                slot.rawSize = 0u; writeWord(kSnapshotBase, 0u);
            }
        } else { slot.rawSize = 0u; writeWord(kSnapshotBase, 0u); }
        if (!slot.rawSize) sPersistentLoaded = false;
        removePackedSlot(sSelectedSlot);
        sSnapshotSize = slot.rawSize ? reinterpret_cast<const SnapshotHeader *>(kSnapshotBase)->totalSize : 0u;
        if (slot.rawSize) copySlotTrailer(false, slot.rawSize);
        flushRawSlot(slot.rawSize ? slot.rawSize + kSlotTrailerSize : 32u);
        sStatus = slot.rawSize ? LMState::Status::Saved : LMState::Status::Empty;
        if (!success) suffix = slot.rawSize ? "; STATE KEPT" : " (SLOT EMPTY)";
    }
    if (success && sStorageCommand == LM_STATE_STORAGE_RENAME) {
        for (u32 i = 0u; i < sCatalogCount; ++i)
            if (sCatalog[i].id == sStorageRequestedId)
                copyBytes(sCatalog[i].name, sStorageName, sizeof(sStorageName));
    }
    if (success || nameOnlyFailure) sArchiveId = m->resultId;
    const char *detail = success ?
        (sStorageCommand == LM_STATE_STORAGE_EXPORT ?
         (sStorageExportPortable ? "EXPORTED: REBOOT READY" : "EXPORTED: THIS BOOT ONLY") :
         sStorageCommand == LM_STATE_STORAGE_RENAME ? "RENAMED" : "READY - PRESS LOAD") :
        nameOnlyFailure ? (sStorageExportPortable ? "REBOOT READY; NAME NOT SAVED" :
            "THIS BOOT ONLY; NAME NOT SAVED") : error;
    storageMessage(sStorageCommand, success || nameOnlyFailure ? m->resultId : sStorageRequestedId,
        detail, suffix);
    sStorageCommand = 0u;
    ++sTimelineRevision;
}
