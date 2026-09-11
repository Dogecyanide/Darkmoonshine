#ifndef SUSAMUNE_LM_STATE_STORAGE_H
#define SUSAMUNE_LM_STATE_STORAGE_H

#include "mem2_map.h"

#define LM_STATE_STORAGE_MAGIC 0x4C4D5344u
#define LM_STATE_STORAGE_VERSION 6u
#define LM_STATE_CACHE_MAGIC 0x4C4D4348u
#define LM_STATE_ARCHIVE_MAGIC 0x4C4D5341u
#define LM_STATE_ARCHIVE_VERSION 1u
#define LM_STATE_ARCHIVE_PERSISTENT_VERSION 2u
#define LM_STATE_PERSISTENT_PROFILE 0x4C4D5031u
#define LM_STATE_STORAGE_EXPORT 1u
#define LM_STATE_STORAGE_IMPORT 2u
#define LM_STATE_STORAGE_CATALOG 3u
#define LM_STATE_STORAGE_RENAME 4u
#define LM_STATE_STORAGE_KEY 5u
#define LM_STATE_STORAGE_DELETE 6u
#define LM_STATE_NAME_BYTES 32u
#define LM_STATE_CATALOG_CAPACITY 8u
#define LM_STATE_CATALOG_HEADER_VALID 1u
#define LM_STATE_CATALOG_PERSISTENT 2u
#define LM_STATE_CATALOG_SCAN_SLICE 16u
#define LM_STATE_STORAGE_OK 0u
#define LM_STATE_STORAGE_UNAVAILABLE 1u
#define LM_STATE_STORAGE_IO_ERROR 2u
#define LM_STATE_STORAGE_BAD_FILE 3u
#define LM_STATE_STORAGE_FULL 4u
#define LM_STATE_STORAGE_NOT_FOUND 5u
#define LM_STATE_STORAGE_WRONG_SESSION 6u
#define LM_STATE_STORAGE_CANCELLED 7u
#define LM_STATE_STORAGE_NAME_ERROR 8u
#define LM_STATE_STORAGE_KEY_ERROR 9u
#define LM_STATE_STORAGE_CONFIG_ERROR 10u
#define LM_STATE_STORAGE_STALE_SELECTION 11u
#define LM_STATE_STORAGE_DELETE_CLEANUP 12u
#define LM_STATE_KEY_MAGIC 0x4C4D4B50u
#define LM_STATE_KEY_VERSION 1u
#define LM_STATE_KEY_UNAVAILABLE 0u
#define LM_STATE_KEY_MISSING 1u
#define LM_STATE_KEY_READY 2u
#define LM_STATE_KEY_INVALID 3u
#define LM_STATE_KEY_IO_ERROR 4u
#define LM_STATE_KEY_UNSUPPORTED 5u
#define LM_STATE_STORAGE_MAX_ID 99999999u
#define LM_STATE_STORAGE_SLICE 0x4000u
#define LM_STATE_STORAGE_PAYLOAD_MAX (SUSAMUNE_MEM2_SNAPSHOT_SIZE - 0x20000u)

/* Each side owns a complete cache line. File bytes never contain a target
 * address; the kernel transfers only the dedicated snapshot reservation. */
struct LmStateArchiveHeader {
    unsigned int magic, version, headerSize, payloadSize;
    unsigned int gameId, snapshotVersion, buildCrc, session;
    unsigned int rawSize, trailerSize, payloadCrc, generation;
    unsigned int authHigh, authLow, reserved0, reserved1;
};
/* Catalog headers are untrusted hints, not proof that a state can be loaded. */
struct LmStateCatalogEntry {
    unsigned int id, bytes, buildCrc, session;
    unsigned int snapshotVersion, generation, flags, reserved;
    char name[LM_STATE_NAME_BYTES];
};
struct LmStateStorageMailbox {
    unsigned int requestSeq, command, archiveId, payloadSize;
    unsigned int processSession, requestReserved[3];
    unsigned int magic, version, ackSeq, status;
    unsigned int resultId, transferred, available, responseReserved;
    struct LmStateArchiveHeader header;
    /* Immutable ARM publication: separate from both request and archive. */
    unsigned int cacheMagic, cachePhysicalBase, cacheSize, filePatchCount;
    unsigned int cacheReserved[4];
    /* ARM publishes this receipt before ackSeq; never shares a PPC line. */
    unsigned int responseSession, responseCommand, responseArchiveId, responseSeq;
    unsigned int responseLength, receiptReserved[3];
    unsigned int catalogCount, catalogAfter, catalogNext, catalogMore;
    unsigned int catalogReserved[4];
    struct LmStateCatalogEntry catalog[LM_STATE_CATALOG_CAPACITY];
    /* PPC owns this complete cache line; names are metadata, never paths. */
    char requestName[LM_STATE_NAME_BYTES];
    /* ARM key publication; runtime config is independent of persisted key. */
    unsigned int keyMagic, keyVersion, keyStatus, keyId;
    unsigned int keyWords[4], keyConfigId, keyReserved[7];
    /* PPC seed request, consumed only when both durable key copies are absent. */
    unsigned int keySeed[4], keySeedReserved[4];
    /* ARM catalog fingerprints; PPC binds confirmation to this completed page. */
    unsigned int catalogIdentity[LM_STATE_CATALOG_CAPACITY];
    /* PPC deletion proof, independent of archive/name/key cache lines. */
    unsigned int deleteCatalogSeq, deleteHeaderCrc, deleteBytes, deleteVersion;
    unsigned int deleteReserved[4];
};

#define LM_STATE_STORAGE_PPC_PTR \
    ((volatile struct LmStateStorageMailbox *)SUSAMUNE_LM_CACHE_PPC_BASE)
#define LM_STATE_STORAGE_PHYS_PTR \
    ((volatile struct LmStateStorageMailbox *)SUSAMUNE_LM_CACHE_PHYS_BASE)

typedef char lm_state_archive_header_size_check[
    sizeof(struct LmStateArchiveHeader) == 64 ? 1 : -1];
typedef char lm_state_mailbox_size_check[
    sizeof(struct LmStateStorageMailbox) == 928 ? 1 : -1];
typedef char lm_state_mailbox_reservation_check[
    sizeof(struct LmStateStorageMailbox) <= SUSAMUNE_LM_MAILBOX_SIZE ? 1 : -1];
typedef char lm_state_catalog_entry_size_check[
    sizeof(struct LmStateCatalogEntry) == 64 ? 1 : -1];
typedef char lm_state_receipt_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, responseSession) == 160 ? 1 : -1];
typedef char lm_state_name_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, requestName) == 736 ? 1 : -1];
typedef char lm_state_key_publication_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, keyMagic) == 768 ? 1 : -1];
typedef char lm_state_key_seed_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, keySeed) == 832 ? 1 : -1];
typedef char lm_state_catalog_identity_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, catalogIdentity) == 864 ? 1 : -1];
typedef char lm_state_delete_proof_alignment_check[
    __builtin_offsetof(struct LmStateStorageMailbox, deleteCatalogSeq) == 896 ? 1 : -1];

static int LmStateNameValid(const char *name) {
    unsigned int i, ended = 0u;
    for (i = 0u; i < LM_STATE_NAME_BYTES; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!c) ended = 1u;
        else if (ended || c < 0x20u || c > 0x7Eu) return 0;
    }
    return ended != 0u;
}

static int LmStateStorageReceiptMatches(const volatile struct LmStateStorageMailbox *m,
    unsigned int sequence, unsigned int session, unsigned int command,
    unsigned int requestedId) {
    return m->magic == LM_STATE_STORAGE_MAGIC &&
        m->version == LM_STATE_STORAGE_VERSION &&
        m->ackSeq == sequence && m->responseSeq == sequence &&
        m->responseSession == session && m->responseCommand == command &&
        m->responseArchiveId == requestedId;
}

/* PatchGame still consumes this prefix after reset. A malformed count does
 * not grant ownership of any part of the handoff. */
static unsigned int LmStateCachePrefix(unsigned int count) {
    if (count > (NIN_MEM2_FILE_PATCH_SIZE - 4u) / 8u) return NIN_MEM2_FILE_PATCH_SIZE;
    return (4u + count * 8u + 31u) & ~31u;
}

#endif
