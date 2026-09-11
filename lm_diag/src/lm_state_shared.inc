// A single compiled resource kind, never a file-selected SYS write command.
// GAME remains contiguous. Its mandatory companion is stored compressed so
// both extents and the SD census still fit below the existing payload ceiling.
constexpr u32 kSharedCompanionMagic = 0x4C4D5343u;  // LMSC
constexpr u32 kSharedCompanionKind = 2u; // Format 28: bounded LML4 or legacy zlib.
constexpr u32 kSharedCodecWorkspace = SUSAMUNE_LM_CACHE_PPC_BASE + SUSAMUNE_LM_MAILBOX_SIZE;
constexpr u32 kSharedStagingStart = kSharedCodecWorkspace + LM_STATE_DEFLATE_WORKSPACE;
constexpr u32 kSharedStagingSize = SUSAMUNE_LM_CACHE_SIZE - SUSAMUNE_LM_MAILBOX_SIZE - LM_STATE_DEFLATE_WORKSPACE;
static_assert(sizeof(LmStateStorageMailbox) <= SUSAMUNE_LM_MAILBOX_SIZE,
              "LM mailbox overlaps shared codec workspace");
static_assert((SUSAMUNE_LM_MAILBOX_SIZE & 31u) == 0u,
              "LM codec workspace must start on a separate cache line");
constexpr u32 kSharedPayloadLimit = LM_STATE_STORAGE_PAYLOAD_MAX - 0x2000u;

struct SharedCompanion {
    u32 magic, kind, generation, coreChecksum, packedSize, checksum;
    LmSharedArchiveDescriptor archive;
};
static_assert(sizeof(SharedCompanion) == 64u, "Shared archive descriptor must be two cache lines");

int sharedReadWord(void *, unsigned int address, unsigned int *value) {
    if (!isMem1Range(address, 4u)) return 0;
    *value = readWord(address);
    return 1;
}

bool sharedArchiveIdentity(LmSharedArchiveDescriptor *out) {
    unsigned int fault = 0u, value = 0u;
    if (LmSharedArchiveValidate(nullptr, sharedReadWord, out, &fault, &value)) return true;
    sCrossRoomFault = 0x50u;
    sCrossRoomFaultValue = fault;
    return false;
}

bool sharedDescriptorsEqual(const LmSharedArchiveDescriptor &a,
                            const LmSharedArchiveDescriptor &b) {
    const u8 *left = reinterpret_cast<const u8 *>(&a);
    const u8 *right = reinterpret_cast<const u8 *>(&b);
    for (u32 i = 0u; i < sizeof(a); ++i) if (left[i] != right[i]) return false;
    return true;
}

u32 snapshotStoredSize(const SnapshotHeader *h) {
    if (h->totalSize < kHeapDataOffset || (h->totalSize & 31u) != 0u ||
        h->totalSize > kSharedPayloadLimit - sizeof(SharedCompanion)) return 0u;
    const SharedCompanion *c = reinterpret_cast<const SharedCompanion *>(kSnapshotBase + h->totalSize);
    const u32 available = kSharedPayloadLimit - h->totalSize - sizeof(*c);
    if (c->magic != kSharedCompanionMagic || c->kind != kSharedCompanionKind ||
        c->generation != h->generation || c->coreChecksum != h->checksum ||
        !c->packedSize || c->packedSize > available ||
        c->archive.size != 0x419B00u ||
        c->archive.systemHeap != h->systemHeap ||
        c->archive.systemStart != h->systemHeapStart ||
        c->archive.systemEnd != h->systemHeapEnd) return 0u;
    const u32 rounded = (c->packedSize + 31u) & ~31u;
    if (rounded > available) return 0u;
    return h->totalSize + sizeof(*c) + rounded;
}

u32 sharedCompanionChecksum(const SharedCompanion *c) {
    const u8 *bytes = reinterpret_cast<const u8 *>(c);
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0u; i < sizeof(*c) + c->packedSize; ++i) {
        const bool omitted = i >= __builtin_offsetof(SharedCompanion, checksum) &&
            i < __builtin_offsetof(SharedCompanion, checksum) + 4u;
        crc = crcByte(crc, omitted ? 0u : bytes[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

bool snapshotCompanionValid(const SnapshotHeader *h, u32 storedSize) {
    if (!storedSize || snapshotStoredSize(h) != storedSize) return false;
    const SharedCompanion *c = reinterpret_cast<const SharedCompanion *>(kSnapshotBase + h->totalSize);
    if (sharedCompanionChecksum(c) != c->checksum) return false;
    const u32 end = h->totalSize + sizeof(*c) + c->packedSize;
    for (u32 i = end; i < storedSize; ++i) if (readByte(kSnapshotBase + i)) return false;
    const LmStateSegment packed[2] = {
        {reinterpret_cast<u8 *>(kSnapshotBase + h->totalSize + sizeof(*c)), c->packedSize},
        {nullptr, 0u}};
    // Dry-run decode checks the complete stream in the private codec workspace.
    // Corrupt or truncated input never reaches a live game destination.
    return LmStateInflate(packed, c->packedSize, nullptr, c->archive.size,
                          reinterpret_cast<void *>(kSharedCodecWorkspace)) != 0;
}

bool sharedArchiveMatchesSnapshot(const SnapshotHeader *h, LmSharedArchiveDescriptor *live) {
    if (!snapshotStoredSize(h) || !sharedArchiveIdentity(live)) return false;
    const SharedCompanion *c = reinterpret_cast<const SharedCompanion *>(kSnapshotBase + h->totalSize);
    if (sharedDescriptorsEqual(c->archive, *live)) return true;
    return crossRoomFault(0x580u, live->base);
}

u32 stageSharedArchive(u32 coreSize, LmSharedArchiveDescriptor *identity) {
    if ((coreSize & 31u) != 0u || coreSize < kHeapDataOffset ||
        !sharedArchiveIdentity(identity) ||
        coreSize > kSharedPayloadLimit - sizeof(SharedCompanion)) return 0u;
    const u32 capacity = kSharedPayloadLimit - coreSize - sizeof(SharedCompanion);
    const u32 available = capacity < kSharedStagingSize ? capacity : kSharedStagingSize;
    const LmStateSegment output[2] = {
        {reinterpret_cast<u8 *>(kSharedStagingStart), available}, {nullptr, 0u}};
    u32 packed = LmStateDeflateFast(reinterpret_cast<const u8 *>(identity->base),
        identity->size, output, reinterpret_cast<void *>(kSharedCodecWorkspace));
    if (!packed) return 0u;
    if (packed > available || ((packed + 31u) & ~31u) > capacity)
        packed = LmStateDeflate(reinterpret_cast<const u8 *>(identity->base),
            identity->size, output, reinterpret_cast<void *>(kSharedCodecWorkspace));
    if (!packed || packed > available || ((packed + 31u) & ~31u) > capacity) return 0u;
    return packed;
}

u32 commitSharedArchive(const SnapshotHeader *h, const LmSharedArchiveDescriptor &identity,
                        u32 packedSize) {
    SharedCompanion *c = reinterpret_cast<SharedCompanion *>(kSnapshotBase + h->totalSize);
    clearWords(c, sizeof(*c));
    c->magic = kSharedCompanionMagic;
    c->kind = kSharedCompanionKind;
    c->generation = h->generation;
    c->coreChecksum = h->checksum;
    c->packedSize = packedSize;
    c->archive = identity;
    copyBytes(c + 1, reinterpret_cast<const void *>(kSharedStagingStart), packedSize);
    const u32 rounded = (packedSize + 31u) & ~31u;
    u8 *payload = reinterpret_cast<u8 *>(c + 1);
    for (u32 i = packedSize; i < rounded; ++i) payload[i] = 0u;
    c->checksum = sharedCompanionChecksum(c);
    return h->totalSize + sizeof(*c) + rounded;
}

bool restoreSharedArchive(const SnapshotHeader *h, const LmSharedArchiveDescriptor &live) {
    const SharedCompanion *c = reinterpret_cast<const SharedCompanion *>(kSnapshotBase + h->totalSize);
    const LmStateSegment packed[2] = {
        {reinterpret_cast<u8 *>(kSnapshotBase + h->totalSize + sizeof(*c)), c->packedSize},
        {nullptr, 0u}};
    // This target is derived from the live validated owner, not the archive.
    if (!LmStateInflate(packed, c->packedSize, reinterpret_cast<u8 *>(live.base),
                        live.size, reinterpret_cast<void *>(kSharedCodecWorkspace))) return false;
    reinterpret_cast<CacheRangeFn>(kDCStoreRangeAddr)(reinterpret_cast<void *>(live.base), live.size);
    return true;
}
