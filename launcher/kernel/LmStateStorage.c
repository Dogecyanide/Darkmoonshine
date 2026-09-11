#include "LmStateStorage.h"
#include "Config.h"
#include "SusamuneCfg.h"
#include "string.h"
#include "ff_utf8.h"
#include "susamune/lm_state_storage.h"
#include "susamune/mod_bin.h"

extern u32 GAME_ID;
static bool Enabled, Open, DirectoryOpen;
static u32 Ack, Seq, Command, FileId, RequestId, Length, Offset, Phase, Session;
static FIL File;
static char Directory[32], Path[72], Temporary[72];
static struct LmStateArchiveHeader Header;
static DIR CatalogDirectory;
static struct LmStateCatalogEntry Catalog[LM_STATE_CATALOG_CAPACITY];
static u32 CatalogCount, CatalogMore;
static u32 CatalogIdentity[LM_STATE_CATALOG_CAPACITY], CatalogSeq, CatalogSession;
static u32 DeleteHeaderCrc, DeleteBytes;
static char RequestName[LM_STATE_NAME_BYTES];
static u32 RequestSeed[4], KeyStatus, KeyConfig;
static bool KeyUnsupported;
struct KeyRecord {
    u32 magic, version, id, checksum, words[4], reserved[8];
};
static struct KeyRecord PersistentKey;
typedef char key_record_size_check[sizeof(struct KeyRecord) == 64 ? 1 : -1];
#define LM_KEY_FILE_MAGIC 0x4C4D4B46u

#define LM_STATE_NAME_MAGIC 0x4C4D534Eu
struct NameRecord {
    u32 magic, version, archiveId, generation;
    char name[LM_STATE_NAME_BYTES];
    u32 checksum, archiveChecksum, reserved[2];
};
typedef char name_record_size_check[sizeof(struct NameRecord) == 64 ? 1 : -1];

static u32 NameChecksum(const struct NameRecord *record)
{
    const u8 *bytes = (const u8*)record;
    u32 crc = 0xFFFFFFFFu, i, bit;
    for (i = 0; i < sizeof(*record); ++i) {
        crc ^= i >= 48 && i < 52 ? 0u : bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
    }
    return ~crc;
}

static u32 ArchiveChecksum(const struct LmStateArchiveHeader *header)
{
    const u8 *bytes = (const u8*)header;
    u32 crc = 0xFFFFFFFFu, i, bit;
    for (i = 0; i < sizeof(*header); ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
    }
    return ~crc;
}

/* Invalid/torn metadata is not a snapshot error. I/O uncertainty must still
 * stop a rename before choosing which of the two durable copies to replace. */
static bool ReadNames(u32 id, u32 archiveChecksum, struct NameRecord *latest, u32 *copy)
{
    struct NameRecord record;
    FIL file;
    char path[72];
    unsigned int done;
    FRESULT result, closed;
    u32 i;
    bool readable = true;
    memset(latest, 0, sizeof(*latest));
    *copy = 1;
    for (i = 0; i < 2; ++i) {
        _sprintf(path, "%s/archive_%08u.name%u", Directory, id, i);
        result = f_open_char(&file, path, FA_READ);
        if (result == FR_NO_FILE) continue;
        if (result != FR_OK) { readable = false; continue; }
        done = 0;
        result = f_read(&file, &record, sizeof(record), &done);
        closed = f_close(&file);
        if (result != FR_OK || closed != FR_OK) { readable = false; continue; }
        if (done != sizeof(record) || file.obj.objsize != sizeof(record) ||
            record.magic != LM_STATE_NAME_MAGIC || record.version != 1u ||
            record.archiveId != id || !record.generation ||
            record.archiveChecksum != archiveChecksum ||
            record.reserved[0] || record.reserved[1] ||
            !LmStateNameValid(record.name) || record.checksum != NameChecksum(&record)) continue;
        if (record.generation > latest->generation) {
            *latest = record;
            *copy = i;
        }
    }
    return readable;
}

static FRESULT RenamePaths(const char *source, const char *destination)
{
    WCHAR from[72], to[72];
    u32 i;
    for (i = 0; i < 72; ++i) { from[i] = 0; to[i] = 0; }
    for (i = 0; source[i] && i < 71; ++i) from[i] = (u8)source[i];
    for (i = 0; destination[i] && i < 71; ++i) to[i] = (u8)destination[i];
    return f_rename(from, to);
}

static bool WriteName(u32 id, u32 archiveChecksum)
{
    struct NameRecord record;
    FIL file;
    char path[72], temporary[72];
    u32 copy;
    unsigned int done = 0;
    FRESULT result, closed;
    if (!ReadNames(id, archiveChecksum, &record, &copy) ||
        record.generation == 0xFFFFFFFFu) return false;
    record.magic = LM_STATE_NAME_MAGIC;
    record.version = 1u;
    record.archiveId = id;
    record.archiveChecksum = archiveChecksum;
    ++record.generation;
    memcpy(record.name, RequestName, sizeof(record.name));
    record.checksum = NameChecksum(&record);
    copy ^= 1u;
    _sprintf(path, "%s/archive_%08u.name%u", Directory, id, copy);
    _sprintf(temporary, "%s/archive_%08u.name%u.tmp", Directory, id, copy);
    result = f_open_char(&file, temporary, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) return false;
    result = f_write(&file, &record, sizeof(record), &done);
    if (result == FR_OK && done == sizeof(record)) result = f_sync(&file);
    closed = f_close(&file);
    if (result != FR_OK || done != sizeof(record) || closed != FR_OK) return false;
    /* The active generation remains untouched until the replacement is durable. */
    result = f_unlink_char(path);
    if (result != FR_OK && result != FR_NO_FILE) return false;
    return RenamePaths(temporary, path) == FR_OK;
}

static FRESULT RenameArchive(void)
{
    return RenamePaths(Temporary, Path);
}

static u32 KeyCrc(const struct KeyRecord *record, bool identity)
{
    const u8 *bytes = (const u8*)record;
    u32 crc = 0xFFFFFFFFu, i, bit;
    for (i = 0; i < sizeof(*record); ++i) {
        crc ^= (i >= 12 && i < 16) || (identity && i >= 8 && i < 12) ? 0u : bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
    }
    return ~crc;
}

static u32 BootConfigId(void)
{
    const u32 mask = NIN_CFG_CHEATS | NIN_CFG_DEBUGGER | NIN_CFG_DEBUGWAIT |
        NIN_CFG_MEMCARDEMU | NIN_CFG_FORCE_WIDE | NIN_CFG_FORCE_PROG |
        NIN_CFG_REMLIMIT | NIN_CFG_USB | NIN_CFG_MC_MULTI | NIN_CFG_NATIVE_SI |
        NIN_CFG_WIIU_WIDE | NIN_CFG_ARCADE_MODE | NIN_CFG_SKIP_IPL | NIN_CFG_MC_SLOTB;
    const u32 fields[] = {NIN_VERSION, NIN_CFG_VERSION, ncfg->Version, ncfg->Config & mask,
        ncfg->VideoMode, ncfg->Language, ncfg->MaxPads, ncfg->GameID, ncfg->MemCardBlocks,
        (u32)ncfg->VideoScale, (u32)ncfg->VideoOffset, (u32)ncfg->SramOffset,
        ncfg->SkipProgAsk, ncfg->CardDelay, GAME_ID, BI2region};
    u32 crc = 0xFFFFFFFFu, i, shift, bit;
    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        for (shift = 0; shift < 4; ++shift) {
            crc ^= (fields[i] >> (24u - shift * 8u)) & 0xFFu;
            for (bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
        }
    }
    return ~crc ? ~crc : 1u;
}

static void PublishKey(void)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    memset((void*)&m->keyMagic, 0, 64);
    m->keyMagic = LM_STATE_KEY_MAGIC;
    m->keyVersion = LM_STATE_KEY_VERSION;
    m->keyStatus = KeyStatus;
    m->keyConfigId = KeyConfig;
    if (KeyStatus == LM_STATE_KEY_READY) {
        m->keyId = PersistentKey.id;
        memcpy((void*)m->keyWords, PersistentKey.words, sizeof(PersistentKey.words));
    }
    sync_after_write((void*)&m->keyMagic, 64);
}

static u32 ReadKeyCopy(u32 copy, struct KeyRecord *record)
{
    FIL file;
    char path[72];
    FRESULT result, closed;
    unsigned int done = 0;
    u32 i, size, nonzero = 0;
    _sprintf(path, "%s/archive_key%u.bin", Directory, copy);
    result = f_open_char(&file, path, FA_READ);
    if (result == FR_NO_FILE) return LM_STATE_KEY_MISSING;
    if (result != FR_OK) return LM_STATE_KEY_IO_ERROR;
    size = file.obj.objsize;
    result = f_read(&file, record, sizeof(*record), &done);
    closed = f_close(&file);
    if (result != FR_OK || closed != FR_OK) return LM_STATE_KEY_IO_ERROR;
    if (size != sizeof(*record) || done != sizeof(*record) ||
        record->magic != LM_KEY_FILE_MAGIC || record->version != 1u ||
        !record->id || record->id != KeyCrc(record, true) ||
        record->checksum != KeyCrc(record, false)) return LM_STATE_KEY_INVALID;
    for (i = 0; i < 8; ++i) if (record->reserved[i]) return LM_STATE_KEY_INVALID;
    for (i = 0; i < 4; ++i) nonzero |= record->words[i];
    return nonzero ? LM_STATE_KEY_READY : LM_STATE_KEY_INVALID;
}

static void ResolveKey(void)
{
    struct KeyRecord first, second;
    u32 a, b;
    memset(&PersistentKey, 0, sizeof(PersistentKey));
    if (!Enabled) KeyStatus = LM_STATE_KEY_UNAVAILABLE;
    else if (KeyUnsupported) KeyStatus = LM_STATE_KEY_UNSUPPORTED;
    else {
        a = ReadKeyCopy(0, &first); b = ReadKeyCopy(1, &second);
        if (a == LM_STATE_KEY_IO_ERROR || b == LM_STATE_KEY_IO_ERROR) KeyStatus = LM_STATE_KEY_IO_ERROR;
        else if (a == LM_STATE_KEY_INVALID || b == LM_STATE_KEY_INVALID) KeyStatus = LM_STATE_KEY_INVALID;
        else if (a == LM_STATE_KEY_MISSING && b == LM_STATE_KEY_MISSING) KeyStatus = LM_STATE_KEY_MISSING;
        else if (a == LM_STATE_KEY_READY && b == LM_STATE_KEY_READY && memcmp(&first, &second, sizeof(first)))
            KeyStatus = LM_STATE_KEY_INVALID;
        else {
            PersistentKey = a == LM_STATE_KEY_READY ? first : second;
            KeyStatus = LM_STATE_KEY_READY;
        }
    }
    PublishKey();
}

static bool WriteKeyCopy(u32 copy, const struct KeyRecord *record)
{
    FIL file;
    char path[72], temporary[72];
    FRESULT result, closed;
    unsigned int done = 0;
    _sprintf(path, "%s/archive_key%u.bin", Directory, copy);
    _sprintf(temporary, "%s/archive_key%u.tmp", Directory, copy);
    result = f_open_char(&file, temporary, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) return false;
    result = f_write(&file, record, sizeof(*record), &done);
    if (result == FR_OK && done == sizeof(*record)) result = f_sync(&file);
    closed = f_close(&file);
    if (result != FR_OK || done != sizeof(*record) || closed != FR_OK) return false;
    /* No existing durable key is ever deleted or overwritten. */
    return RenamePaths(temporary, path) == FR_OK;
}

static u32 EnsureKey(void)
{
    struct KeyRecord record;
    u32 i, nonzero = 0;
    bool written;
    ResolveKey();
    if (KeyStatus == LM_STATE_KEY_READY) return LM_STATE_STORAGE_OK;
    if (KeyStatus != LM_STATE_KEY_MISSING) return LM_STATE_STORAGE_KEY_ERROR;
    for (i = 0; i < 4; ++i) nonzero |= RequestSeed[i];
    if (!nonzero) return LM_STATE_STORAGE_BAD_FILE;
    memset(&record, 0, sizeof(record));
    record.magic = LM_KEY_FILE_MAGIC; record.version = 1u;
    memcpy(record.words, RequestSeed, sizeof(record.words));
    record.id = KeyCrc(&record, true);
    if (!record.id) return LM_STATE_STORAGE_BAD_FILE;
    record.checksum = KeyCrc(&record, false);
    written = WriteKeyCopy(0, &record) && WriteKeyCopy(1, &record);
    ResolveKey();
    return written && KeyStatus == LM_STATE_KEY_READY ? LM_STATE_STORAGE_OK : LM_STATE_STORAGE_KEY_ERROR;
}

static u32 ArchiveIdentityStatus(const struct LmStateArchiveHeader *h)
{
    if (h->version == LM_STATE_ARCHIVE_VERSION)
        return h->session == Session ? LM_STATE_STORAGE_OK : LM_STATE_STORAGE_WRONG_SESSION;
    if (KeyStatus != LM_STATE_KEY_READY || h->session != PersistentKey.id)
        return LM_STATE_STORAGE_KEY_ERROR;
    return h->reserved1 == KeyConfig ? LM_STATE_STORAGE_OK : LM_STATE_STORAGE_CONFIG_ERROR;
}

static bool ValidHeader(const struct LmStateArchiveHeader *h)
{
    return h->magic == LM_STATE_ARCHIVE_MAGIC &&
        (h->version == LM_STATE_ARCHIVE_VERSION || h->version == LM_STATE_ARCHIVE_PERSISTENT_VERSION) &&
        h->headerSize == sizeof(*h) && h->gameId == SUSAMUNE_MOD_GAME_ID_LMJ &&
        h->rawSize >= 256 && h->trailerSize != 0 &&
        h->rawSize <= SUSAMUNE_MEM2_SNAPSHOT_SIZE &&
        h->trailerSize <= SUSAMUNE_MEM2_SNAPSHOT_SIZE - h->rawSize &&
        h->payloadSize == h->rawSize + h->trailerSize &&
        h->payloadSize <= LM_STATE_STORAGE_PAYLOAD_MAX &&
        (h->version == LM_STATE_ARCHIVE_VERSION ? (h->reserved0 == 0 && h->reserved1 == 0) :
         (h->reserved0 == LM_STATE_PERSISTENT_PROFILE && h->reserved1 != 0 && h->session != 0));
}

static void Finish(u32 status)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    if (Open) {
        if (f_close(&File) != FR_OK && status == LM_STATE_STORAGE_OK)
            status = LM_STATE_STORAGE_IO_ERROR;
        Open = false;
    }
    if (DirectoryOpen) {
        if (f_closedir(&CatalogDirectory) != FR_OK && status == LM_STATE_STORAGE_OK)
            status = LM_STATE_STORAGE_IO_ERROR;
        DirectoryOpen = false;
    }
    if (Command == LM_STATE_STORAGE_CATALOG) {
        CatalogSeq = status == LM_STATE_STORAGE_OK ? Seq : 0u;
        CatalogSession = status == LM_STATE_STORAGE_OK ? Session : 0u;
    }
    if (Command == LM_STATE_STORAGE_DELETE) CatalogSeq = CatalogSession = 0u;
    m->responseSession = Session;
    m->responseCommand = Command;
    m->responseArchiveId = RequestId;
    m->responseSeq = Seq;
    m->responseLength = Length;
    sync_after_write((void*)&m->responseSession, 32);
    m->status = status;
    m->resultId = FileId;
    m->transferred = Offset;
    m->ackSeq = Ack = Seq;
    sync_after_write((void*)&m->magic, 32);
    Phase = 0;
}

void LmStateStorageInit(void)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    FRESULT result;
    u32 patchCount, prefix;
    Enabled = false;
    Phase = Ack = 0;
    CatalogSeq = CatalogSession = 0u;
    Open = DirectoryOpen = false;
    if (GAME_ID != SUSAMUNE_MOD_GAME_ID_LMJ) return;
    memset((void*)m, 0, sizeof(*m));
    // DIinit previously cleared DIMM with ARM cached stores. Publish those
    // writes before handing the complete SegaBoot/DIMM cache to the PPC.
    sync_after_write((void*)SUSAMUNE_LM_CACHE_PHYS_BASE,
                     SUSAMUNE_LM_CACHE_SIZE);
    sync_before_read((void*)NIN_MEM2_FILE_PATCH_PHYS_BASE, 32u);
    patchCount = read32(NIN_MEM2_FILE_PATCH_PHYS_BASE);
    prefix = LmStateCachePrefix(patchCount);
    m->cacheMagic = LM_STATE_CACHE_MAGIC;
    m->filePatchCount = patchCount;
    m->cachePhysicalBase = NIN_MEM2_FILE_PATCH_PHYS_BASE + prefix;
    m->cacheSize = NIN_MEM2_FILE_PATCH_SIZE - prefix;
    if (m->cacheSize)
        sync_after_write((void*)m->cachePhysicalBase, m->cacheSize);
    _sprintf(Directory, "%s/lm_states", SusamuneCfgStoragePrefix());
    if (SusamuneCfgStorageAvailable()) {
        result = f_mkdir_char(Directory);
        Enabled = result == FR_OK || result == FR_EXIST;
    }
    KeyConfig = BootConfigId();
    KeyUnsupported = (ncfg->Config & (NIN_CFG_CHEATS | NIN_CFG_DEBUGGER | NIN_CFG_DEBUGWAIT)) != 0;
    ResolveKey();
    m->magic = LM_STATE_STORAGE_MAGIC;
    m->version = LM_STATE_STORAGE_VERSION;
    m->available = Enabled ? 1 : 0;
    m->status = Enabled ? LM_STATE_STORAGE_OK : LM_STATE_STORAGE_UNAVAILABLE;
    sync_after_write((void*)m, sizeof(*m));
}

bool LmStateStoragePending(void)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    if (GAME_ID != SUSAMUNE_MOD_GAME_ID_LMJ) return false;
    if (Phase) return true;
    sync_before_read((void*)m, 32);
    return m->requestSeq != Ack;
}

static u32 CatalogFileId(const FILINFO *info)
{
    static const char prefix[] = "archive_";
    u32 i, id = 0;
    if (info->fattrib & AM_DIR) return 0;
    for (i = 0; i < 8; ++i) if (info->fname[i] != prefix[i]) return 0;
    for (i = 8; i < 16; ++i) {
        if (info->fname[i] < '0' || info->fname[i] > '9') return 0;
        id = id * 10 + info->fname[i] - '0';
    }
    if (info->fname[16] != '.' || info->fname[17] != 'l' ||
        info->fname[18] != 'm' || info->fname[19] != 's' || info->fname[20]) return 0;
    return id;
}

static void CatalogService(void)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    FILINFO info;
    u32 scanned, id, pos, i;
    unsigned int done;
    FRESULT result;
    for (scanned = 0; scanned < LM_STATE_CATALOG_SCAN_SLICE; ++scanned) {
        memset(&info, 0, sizeof(info));
        result = f_readdir(&CatalogDirectory, &info);
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        if (!info.fname[0]) {
            m->catalogCount = CatalogCount;
            m->catalogAfter = RequestId;
            m->catalogNext = CatalogCount ? Catalog[CatalogCount - 1].id : RequestId;
            m->catalogMore = CatalogMore;
            memcpy((void*)m->catalog, Catalog, sizeof(Catalog));
            memcpy((void*)m->catalogIdentity, CatalogIdentity, sizeof(CatalogIdentity));
            sync_after_write((void*)m->catalogIdentity, sizeof(CatalogIdentity));
            sync_after_write((void*)&m->catalogCount, 32 + sizeof(Catalog));
            Finish(LM_STATE_STORAGE_OK); return;
        }
        id = CatalogFileId(&info);
        if (!id || id <= RequestId) continue;
        for (pos = 0; pos < CatalogCount && Catalog[pos].id < id; ++pos) {}
        if (pos < CatalogCount && Catalog[pos].id == id) continue;
        if (CatalogCount == LM_STATE_CATALOG_CAPACITY) {
            CatalogMore = 1;
            if (pos == CatalogCount) continue;
        } else ++CatalogCount;
        for (i = CatalogCount - 1; i > pos; --i) {
            Catalog[i] = Catalog[i - 1];
            CatalogIdentity[i] = CatalogIdentity[i - 1];
        }
        memset(&Catalog[pos], 0, sizeof(Catalog[pos]));
        Catalog[pos].id = id;
        Catalog[pos].bytes = info.fsize;
        _sprintf(Path, "%s/archive_%08u.lms", Directory, id);
        result = f_open_char(&File, Path, FA_READ);
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        Open = true;
        memset(&Header, 0, sizeof(Header));
        result = f_read(&File, &Header, sizeof(Header), &done);
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        if (File.obj.objsize != info.fsize ||
            done != (info.fsize < sizeof(Header) ? info.fsize : sizeof(Header))) {
            Finish(LM_STATE_STORAGE_IO_ERROR); return;
        }
        CatalogIdentity[pos] = ArchiveChecksum(&Header);
        if (done == sizeof(Header) && ValidHeader(&Header) &&
            File.obj.objsize == sizeof(Header) + Header.payloadSize) {
            Catalog[pos].buildCrc = Header.buildCrc;
            Catalog[pos].session = Header.session;
            Catalog[pos].snapshotVersion = Header.snapshotVersion;
            Catalog[pos].generation = Header.generation;
            Catalog[pos].flags = LM_STATE_CATALOG_HEADER_VALID;
            if (Header.version == LM_STATE_ARCHIVE_PERSISTENT_VERSION) {
                Catalog[pos].flags |= LM_STATE_CATALOG_PERSISTENT;
                Catalog[pos].reserved = Header.reserved1;
            }
        }
        result = f_close(&File); Open = false;
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        {
            struct NameRecord name;
            u32 copy;
            if (done == sizeof(Header) && ValidHeader(&Header) &&
                ReadNames(id, ArchiveChecksum(&Header), &name, &copy) && name.generation)
                memcpy(Catalog[pos].name, name.name, sizeof(name.name));
        }
    }
}

static u32 DeleteArchive(void)
{
    FILINFO info;
    struct LmStateArchiveHeader observed;
    unsigned int done = 0;
    FRESULT result;
    char sidecar[72];
    u32 i;
    bool cleaned = true;
    memset(&info, 0, sizeof(info));
    result = f_stat_char(Path, &info);
    if (result == FR_NO_FILE) return LM_STATE_STORAGE_NOT_FOUND;
    if (result != FR_OK || (info.fattrib & AM_DIR)) return LM_STATE_STORAGE_IO_ERROR;
    if (info.fsize != DeleteBytes) return LM_STATE_STORAGE_STALE_SELECTION;
    result = f_open_char(&File, Path, FA_READ);
    if (result != FR_OK) return LM_STATE_STORAGE_IO_ERROR;
    Open = true;
    memset(&observed, 0, sizeof(observed));
    result = f_read(&File, &observed, sizeof(observed), &done);
    if (result != FR_OK) return LM_STATE_STORAGE_IO_ERROR;
    if (File.obj.objsize != DeleteBytes ||
        done != (DeleteBytes < sizeof(observed) ? DeleteBytes : sizeof(observed)) ||
        ArchiveChecksum(&observed) != DeleteHeaderCrc) return LM_STATE_STORAGE_STALE_SELECTION;
    result = f_close(&File); Open = false;
    if (result != FR_OK) return LM_STATE_STORAGE_IO_ERROR;
    result = f_unlink_char(Path);
    if (result != FR_OK) return result == FR_NO_FILE ? LM_STATE_STORAGE_NOT_FOUND : LM_STATE_STORAGE_IO_ERROR;
    /* The archive is gone. Later metadata failure must not claim it was kept. */
    for (i = 0; i < 2u; ++i) {
        _sprintf(sidecar, "%s/archive_%08u.name%u", Directory, FileId, i);
        memset(&info, 0, sizeof(info));
        result = f_stat_char(sidecar, &info);
        if (result == FR_NO_FILE) continue;
        if (result != FR_OK || (info.fattrib & AM_DIR)) { cleaned = false; continue; }
        if (f_unlink_char(sidecar) != FR_OK) cleaned = false;
    }
    return cleaned ? LM_STATE_STORAGE_OK : LM_STATE_STORAGE_DELETE_CLEANUP;
}

void LmStateStorageService(void)
{
    volatile struct LmStateStorageMailbox *m = LM_STATE_STORAGE_PHYS_PTR;
    FRESULT result;
    unsigned int done = 0;
    u32 count;
    FILINFO info;
    sync_before_read((void*)m, 32);
    if (Phase && (m->processSession != Session || m->requestSeq != Seq ||
                  m->command != Command || m->archiveId != RequestId)) {
        Finish(LM_STATE_STORAGE_CANCELLED); return;
    }
    if (Phase == 0) {
        sync_before_read((void*)m, 32);
        Seq = m->requestSeq;
        Session = m->processSession;
        Command = m->command;
        FileId = RequestId = m->archiveId;
        Length = m->payloadSize;
        Offset = 0;
        if (!Enabled) { Finish(LM_STATE_STORAGE_UNAVAILABLE); return; }
        if ((Command != LM_STATE_STORAGE_EXPORT &&
             Command != LM_STATE_STORAGE_IMPORT && Command != LM_STATE_STORAGE_CATALOG &&
             Command != LM_STATE_STORAGE_RENAME && Command != LM_STATE_STORAGE_KEY &&
             Command != LM_STATE_STORAGE_DELETE) ||
            (FileId == 0 && Command != LM_STATE_STORAGE_CATALOG && Command != LM_STATE_STORAGE_KEY) ||
            FileId > LM_STATE_STORAGE_MAX_ID) {
            Finish(LM_STATE_STORAGE_BAD_FILE); return;
        }
        if (Command == LM_STATE_STORAGE_DELETE) {
            sync_before_read((void*)&m->deleteCatalogSeq, 32u);
            if (Length || m->deleteVersion != LM_STATE_STORAGE_VERSION) {
                Finish(LM_STATE_STORAGE_BAD_FILE); return;
            }
            for (count = 0; count < 4u; ++count)
                if (m->deleteReserved[count]) { Finish(LM_STATE_STORAGE_BAD_FILE); return; }
            for (count = 0; count < CatalogCount && Catalog[count].id != FileId; ++count) {}
            if (!CatalogSeq || m->deleteCatalogSeq != CatalogSeq || CatalogSession != Session ||
                count == CatalogCount || m->deleteHeaderCrc != CatalogIdentity[count] ||
                m->deleteBytes != Catalog[count].bytes) {
                Finish(LM_STATE_STORAGE_STALE_SELECTION); return;
            }
            DeleteHeaderCrc = m->deleteHeaderCrc;
            DeleteBytes = m->deleteBytes;
        }
        if (Command == LM_STATE_STORAGE_KEY) {
            if (FileId || Length) { Finish(LM_STATE_STORAGE_BAD_FILE); return; }
            sync_before_read((void*)m->keySeed, 32);
            for (count = 0; count < 4; ++count) {
                if (m->keySeedReserved[count]) { Finish(LM_STATE_STORAGE_BAD_FILE); return; }
                RequestSeed[count] = m->keySeed[count];
            }
        }
        if (Command == LM_STATE_STORAGE_EXPORT || Command == LM_STATE_STORAGE_RENAME) {
            sync_before_read((void*)m->requestName, LM_STATE_NAME_BYTES);
            memcpy(RequestName, (const void*)m->requestName, sizeof(RequestName));
            if (!LmStateNameValid(RequestName)) {
                Finish(LM_STATE_STORAGE_BAD_FILE); return;
            }
        }
        if (Command == LM_STATE_STORAGE_EXPORT) {
            sync_before_read((void*)&m->header, sizeof(Header));
            memcpy(&Header, (const void*)&m->header, sizeof(Header));
            if (!ValidHeader(&Header) || Length != Header.payloadSize) {
                Finish(LM_STATE_STORAGE_BAD_FILE); return;
            }
            count = ArchiveIdentityStatus(&Header);
            if (count != LM_STATE_STORAGE_OK) { Finish(count); return; }
        }
        Phase = 1;
        return;
    }
    if (Phase == 6) { CatalogService(); return; }
    if (Phase == 1) {
        if (Command == LM_STATE_STORAGE_KEY) { Finish(EnsureKey()); return; }
        if (Command == LM_STATE_STORAGE_CATALOG) {
            CatalogCount = CatalogMore = Length = 0;
            CatalogSeq = CatalogSession = 0u;
            memset(Catalog, 0, sizeof(Catalog));
            memset(CatalogIdentity, 0, sizeof(CatalogIdentity));
            result = f_opendir_char(&CatalogDirectory, Directory);
            if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            DirectoryOpen = true; Phase = 6; return;
        }
        _sprintf(Path, "%s/archive_%08u.lms", Directory, FileId);
        if (Command == LM_STATE_STORAGE_DELETE) { Finish(DeleteArchive()); return; }
        if (Command == LM_STATE_STORAGE_RENAME) {
            Length = 0;
            memset(&info, 0, sizeof(info));
            result = f_stat_char(Path, &info);
            if (result != FR_OK || (info.fattrib & AM_DIR)) {
                Finish(result == FR_NO_FILE ? LM_STATE_STORAGE_NOT_FOUND :
                       LM_STATE_STORAGE_IO_ERROR); return;
            }
            result = f_open_char(&File, Path, FA_READ);
            if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            Open = true;
            result = f_read(&File, &Header, sizeof(Header), &done);
            if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            if (done != sizeof(Header) || !ValidHeader(&Header) ||
                File.obj.objsize != sizeof(Header) + Header.payloadSize) {
                Finish(LM_STATE_STORAGE_BAD_FILE); return;
            }
            result = f_close(&File); Open = false;
            if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            Finish(WriteName(FileId, ArchiveChecksum(&Header)) ?
                   LM_STATE_STORAGE_OK : LM_STATE_STORAGE_IO_ERROR);
            return;
        }
        if (Command == LM_STATE_STORAGE_EXPORT) {
            memset(&info, 0, sizeof(info));
            result = f_stat_char(Path, &info);
            if (result == FR_OK) {
                if (++FileId > LM_STATE_STORAGE_MAX_ID)
                    Finish(LM_STATE_STORAGE_FULL);
                return;
            }
            if (result != FR_NO_FILE) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            /* Failed delete cleanup reserves its ID; never adopt stale names. */
            for (count = 0; count < 2u; ++count) {
                _sprintf(Temporary, "%s/archive_%08u.name%u", Directory, FileId, count);
                result = f_stat_char(Temporary, &info);
                if (result != FR_NO_FILE) break;
            }
            if (result == FR_OK) {
                if (++FileId > LM_STATE_STORAGE_MAX_ID) Finish(LM_STATE_STORAGE_FULL);
                return;
            }
            if (result != FR_NO_FILE) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
            _sprintf(Temporary, "%s/archive_%08u.tmp", Directory, FileId);
            result = f_open_char(&File, Temporary, FA_WRITE | FA_CREATE_NEW);
            if (result == FR_EXIST) {
                if (++FileId > LM_STATE_STORAGE_MAX_ID)
                    Finish(LM_STATE_STORAGE_FULL);
                return;
            }
        } else result = f_open_char(&File, Path, FA_READ);
        if (result != FR_OK) {
            Finish(Command == LM_STATE_STORAGE_IMPORT && result == FR_NO_FILE ?
                LM_STATE_STORAGE_NOT_FOUND : LM_STATE_STORAGE_IO_ERROR);
            return;
        }
        Open = true;
        Phase = 2;
        return;
    }
    if (Phase == 2) {
        if (Command == LM_STATE_STORAGE_EXPORT)
            result = f_write(&File, &Header, sizeof(Header), &done);
        else {
            result = f_read(&File, &Header, sizeof(Header), &done);
            if (result == FR_OK && done == sizeof(Header) &&
                (!ValidHeader(&Header) ||
                 File.obj.objsize != sizeof(Header) + Header.payloadSize)) {
                Finish(LM_STATE_STORAGE_BAD_FILE); return;
            }
            Length = Header.payloadSize;
        }
        if (result != FR_OK || done != sizeof(Header)) {
            Finish(LM_STATE_STORAGE_IO_ERROR); return;
        }
        if (Command == LM_STATE_STORAGE_IMPORT) {
            count = ArchiveIdentityStatus(&Header);
            if (count != LM_STATE_STORAGE_OK) { Finish(count); return; }
        }
        Phase = 3;
        return;
    }
    if (Phase == 3) {
        u8 *bytes = (u8*)SUSAMUNE_MEM2_SNAPSHOT_PHYS_BASE + Offset;
        count = Length - Offset;
        if (count > LM_STATE_STORAGE_SLICE) count = LM_STATE_STORAGE_SLICE;
        if (Command == LM_STATE_STORAGE_EXPORT) {
            sync_before_read(bytes, count);
            result = f_write(&File, bytes, count, &done);
        } else {
            result = f_read(&File, bytes, count, &done);
            sync_after_write(bytes, done);
        }
        if (result != FR_OK || done != count) {
            Finish(LM_STATE_STORAGE_IO_ERROR); return;
        }
        Offset += count;
        if (Offset == Length) Phase = 4;
        return;
    }
    if (Phase == 4) {
        if (Command == LM_STATE_STORAGE_EXPORT && f_sync(&File) != FR_OK) {
            Finish(LM_STATE_STORAGE_IO_ERROR); return;
        }
        result = f_close(&File);
        Open = false;
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        Phase = 5;
        return;
    }
    if (Command == LM_STATE_STORAGE_EXPORT) {
        result = RenameArchive();
        if (result != FR_OK) { Finish(LM_STATE_STORAGE_IO_ERROR); return; }
        if (!WriteName(FileId, ArchiveChecksum(&Header))) {
            Finish(LM_STATE_STORAGE_NAME_ERROR); return;
        }
    } else {
        memcpy((void*)&m->header, &Header, sizeof(Header));
        sync_after_write((void*)&m->header, sizeof(Header));
    }
    Finish(LM_STATE_STORAGE_OK);
}
