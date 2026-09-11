/* Execute the real ARM worker against an in-memory FatFS with fault injection. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "susamune/lm_state_storage.h"
#define LM_STATE_STORAGE_KERNEL_H
#define __SUSAMUNE_CFG_H__
#define __CONFIG_H__
#include "../launcher/common/include/CommonConfig.h"
#define __STRING_H__
#define _FATFS_UTF8
#define SUSAMUNE_MOD_BIN_H
#define SUSAMUNE_MOD_GAME_ID_LMJ 0x474C4D4Au
#define _sprintf sprintf
typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t WCHAR;
typedef int FRESULT;
enum { FR_OK, FR_DISK_ERR, FR_NO_FILE = 4, FR_EXIST = 8 };
enum { FA_READ = 1, FA_WRITE = 2, FA_CREATE_NEW = 4, FA_CREATE_ALWAYS = 8 };
typedef struct { struct { u32 objsize; } obj; u32 index, offset; } FIL;
typedef struct { u32 fsize, fattrib; WCHAR fname[256]; } FILINFO;
typedef struct { u32 index; } DIR;
enum { AM_DIR = 16 };
static struct LmStateStorageMailbox TestMailbox;
#ifdef LM_STATE_TEST_SHARED_SNAPSHOT
static u8 *TestSnapshot = (u8*)LM_STATE_TEST_SHARED_SNAPSHOT;
#else
static u8 TestSnapshot[SUSAMUNE_MEM2_SNAPSHOT_SIZE + 32];
#endif
#undef LM_STATE_STORAGE_PHYS_PTR
#define LM_STATE_STORAGE_PHYS_PTR (&TestMailbox)
#undef SUSAMUNE_MEM2_SNAPSHOT_PHYS_BASE
#define SUSAMUNE_MEM2_SNAPSHOT_PHYS_BASE ((uintptr_t)TestSnapshot)
u32 GAME_ID;
u32 BI2region;
static NIN_CFG TestConfig;
static NIN_CFG *const ncfg = &TestConfig;
static u32 FailOperation, FailAt, Calls[12];
static u32 TestPatchCount;
struct TestFile { char name[72]; u8 *data; u32 size; };
static struct TestFile Files[64];
static int Fail(u32 op) { return ++Calls[op] == FailAt && op == FailOperation; }
static int Find(const char *name) {
    int i;
    for (i = 0; i < 64; ++i) if (!strcmp(Files[i].name, name)) return i;
    return -1;
}
static int Add(const char *name) {
    int i;
    for (i = 0; i < 64; ++i) if (!Files[i].name[0]) {
        strcpy(Files[i].name, name);
        Files[i].data = (u8*)malloc(SUSAMUNE_MEM2_SNAPSHOT_SIZE + 64);
        Files[i].size = 0;
        return i;
    }
    return -1;
}
static void sync_before_read(void *p, u32 n) { (void)p; (void)n; }
static void sync_after_write(void *p, u32 n) { (void)p; (void)n; }
static u32 read32(uintptr_t p) {
    return p == NIN_MEM2_FILE_PATCH_PHYS_BASE ? TestPatchCount : 0u;
}
static const char *SusamuneCfgStoragePrefix(void) { return ""; }
static bool SusamuneCfgStorageAvailable(void) { return true; }
static FRESULT f_mkdir_char(const char *p) { (void)p; return FR_OK; }
static FRESULT f_opendir_char(DIR *d, const char *p) {
    (void)p; d->index = 0; return Fail(7) ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_readdir(DIR *d, FILINFO *info) {
    const char *name; u32 n;
    if (Fail(8)) return FR_DISK_ERR;
    while (d->index < 64 && !Files[d->index].name[0]) ++d->index;
    if (d->index == 64) { info->fname[0] = 0; return FR_OK; }
    name = strrchr(Files[d->index].name, '/'); name = name ? name + 1 : Files[d->index].name;
    for (n = 0; name[n]; ++n) info->fname[n] = (WCHAR)name[n];
    info->fname[n] = 0; info->fsize = Files[d->index].size;
    info->fattrib = 0; ++d->index; return FR_OK;
}
static FRESULT f_closedir(DIR *d) { (void)d; return Fail(9) ? FR_DISK_ERR : FR_OK; }
static FRESULT f_stat_char(const char *p, FILINFO *info) {
    int i = Find(p);
    if (i < 0) return FR_NO_FILE;
    info->fsize = Files[i].size; info->fattrib = 0; return FR_OK;
}
static FRESULT f_open_char(FIL *f, const char *p, u32 mode) {
    int i = Find(p);
    if (Fail(1)) return FR_DISK_ERR;
    if (mode & FA_CREATE_NEW) {
        if (i >= 0) return FR_EXIST;
        i = Add(p);
    }
    if (mode & FA_CREATE_ALWAYS) {
        if (i < 0) i = Add(p);
        if (i >= 0) Files[i].size = 0;
    }
    if (i < 0) return FR_NO_FILE;
    f->index = i; f->offset = 0; f->obj.objsize = Files[i].size;
    return FR_OK;
}
static FRESULT f_write(FIL *f, const void *data, u32 n, unsigned int *done) {
    if (Fail(3)) { *done = 0; return FR_DISK_ERR; }
    if (n > SUSAMUNE_MEM2_SNAPSHOT_SIZE + 64 - f->offset) return FR_DISK_ERR;
    memcpy(Files[f->index].data + f->offset, data, n);
    f->offset += n; Files[f->index].size = f->offset; *done = n;
    return FR_OK;
}
static FRESULT f_read(FIL *f, void *data, u32 n, unsigned int *done) {
    if (Fail(2)) { *done = 0; return FR_DISK_ERR; }
    if (n > Files[f->index].size - f->offset) n = Files[f->index].size - f->offset;
    memcpy(data, Files[f->index].data + f->offset, n);
    f->offset += n; *done = n; return FR_OK;
}
static FRESULT f_sync(FIL *f) { (void)f; return Fail(4) ? FR_DISK_ERR : FR_OK; }
static FRESULT f_close(FIL *f) { (void)f; return Fail(5) ? FR_DISK_ERR : FR_OK; }
static FRESULT f_unlink_char(const char *path) {
    int i = Find(path);
    if (Fail(10)) return FR_DISK_ERR;
    if (i < 0) return FR_NO_FILE;
    free(Files[i].data); memset(&Files[i], 0, sizeof(Files[i])); return FR_OK;
}
static FRESULT f_rename(const WCHAR *a, const WCHAR *b) {
    char from[72], to[72]; u32 i; int index;
    if (Fail(6)) return FR_DISK_ERR;
    for (i = 0; i < 72; ++i) { from[i] = (char)a[i]; to[i] = (char)b[i]; }
    index = Find(from);
    if (index < 0 || Find(to) >= 0) return FR_DISK_ERR;
    strcpy(Files[index].name, to); return FR_OK;
}
#include "../launcher/kernel/LmStateStorage.c"
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API
#endif
API void reset_worker(void) {
    int i;
    for (i = 0; i < 64; ++i) free(Files[i].data);
    memset(Files, 0, sizeof(Files)); memset(Calls, 0, sizeof(Calls));
    memset(TestSnapshot, 0xAD, SUSAMUNE_MEM2_SNAPSHOT_SIZE + 32);
    FailOperation = FailAt = TestPatchCount = 0; GAME_ID = SUSAMUNE_MOD_GAME_ID_LMJ;
    memset(&TestConfig, 0, sizeof(TestConfig));
    TestConfig.Magicbytes = 0x01070CF6u; TestConfig.Version = NIN_CFG_VERSION;
    TestConfig.GameID = GAME_ID; TestConfig.MaxPads = 4; BI2region = 0;
    LmStateStorageInit();
    memset(Calls, 0, sizeof(Calls));
}
API void fault(u32 operation, u32 at) { FailOperation = operation; FailAt = at; }
API u32 operation_calls(u32 operation) { return operation < 12 ? Calls[operation] : 0; }
API void restart_worker(void) {
    memset(Calls, 0, sizeof(Calls)); FailOperation = FailAt = 0;
    LmStateStorageInit();
    memset(Calls, 0, sizeof(Calls));
}
API void reload_keys(void) { ResolveKey(); }
API void set_seed(const u32 *seed) {
    memcpy(TestMailbox.keySeed, seed, sizeof(TestMailbox.keySeed));
    memset(TestMailbox.keySeedReserved, 0, sizeof(TestMailbox.keySeedReserved));
}
API u32 key_status(void) { return TestMailbox.keyStatus; }
API u32 key_id(void) { return TestMailbox.keyId; }
API u32 key_config(void) { return TestMailbox.keyConfigId; }
API int key_words_equal(const u32 *words) { return !memcmp(TestMailbox.keyWords, words, sizeof(TestMailbox.keyWords)); }
API void set_config(u32 field, u32 value) {
    switch (field) {
    case 0: TestConfig.Config = value; break;
    case 1: TestConfig.VideoMode = value; break;
    case 2: TestConfig.Language = value; break;
    case 3: TestConfig.MaxPads = value; break;
    case 4: TestConfig.GameID = value; break;
    case 5: TestConfig.MemCardBlocks = value; break;
    case 6: TestConfig.VideoScale = (signed char)value; break;
    case 7: TestConfig.VideoOffset = (signed char)value; break;
    case 8: TestConfig.SramOffset = (signed int)value; break;
    case 9: TestConfig.SkipProgAsk = value; break;
    case 10: TestConfig.CardDelay = value; break;
    case 11: TestConfig.Version = value; break;
    case 12: BI2region = value; break;
    }
}
API void set_name(const void *name) { memcpy(TestMailbox.requestName, name, LM_STATE_NAME_BYTES); }
API void request(u32 command, u32 id, u32 size) {
    struct LmStateArchiveHeader *h = &TestMailbox.header;
    memset(h, 0, sizeof(*h));
    h->magic = LM_STATE_ARCHIVE_MAGIC; h->version = LM_STATE_ARCHIVE_VERSION;
    h->headerSize = sizeof(*h); h->gameId = SUSAMUNE_MOD_GAME_ID_LMJ;
    h->rawSize = size - 256; h->trailerSize = 256; h->payloadSize = size; h->session = 987;
    TestMailbox.command = command; TestMailbox.archiveId = id;
    TestMailbox.payloadSize = size; TestMailbox.processSession = 987;
    ++TestMailbox.requestSeq;
}
API void request_v2(u32 command, u32 id, u32 size) {
    request(command, id, size);
    TestMailbox.header.version = LM_STATE_ARCHIVE_PERSISTENT_VERSION;
    TestMailbox.header.session = TestMailbox.keyId;
    TestMailbox.header.reserved0 = LM_STATE_PERSISTENT_PROFILE;
    TestMailbox.header.reserved1 = TestMailbox.keyConfigId;
}
API void step(void) { if (LmStateStoragePending()) LmStateStorageService(); }
API int pending(void) { return LmStateStoragePending(); }
API u32 status(void) { return TestMailbox.status; }
API u32 result_id(void) { return TestMailbox.resultId; }
API u32 transferred(void) { return TestMailbox.transferred; }
API void cancel_session(void) { ++TestMailbox.processSession; }
API int file_size(u32 id, int temporary) {
    char path[72]; int i;
    sprintf(path, "/lm_states/archive_%08u.%s", id, temporary ? "tmp" : "lms");
    i = Find(path); return i < 0 ? -1 : (int)Files[i].size;
}
API void add_import(u32 id, const u8 *data, u32 size) {
    char path[72]; int i;
    sprintf(path, "/lm_states/archive_%08u.lms", id); i = Add(path);
    memcpy(Files[i].data, data, size); Files[i].size = size;
}
API void *snapshot(void) { return TestSnapshot; }
API void *mailbox(void) { return &TestMailbox; }
API void publish_cache(u32 count) { TestPatchCount = count; LmStateStorageInit(); }
API u32 cache_base(void) { return TestMailbox.cachePhysicalBase; }
API u32 cache_size(void) { return TestMailbox.cacheSize; }
API void *file_bytes(u32 id) {
    char path[72]; int i;
    sprintf(path, "/lm_states/archive_%08u.lms", id); i = Find(path);
    return i < 0 ? NULL : Files[i].data;
}
API void add_named_file(const char *basename, const u8 *data, u32 size) {
    char path[72]; int i;
    snprintf(path, sizeof(path), "/lm_states/%s", basename); i = Find(path);
    if (i < 0) i = Add(path);
    if (i >= 0 && size <= SUSAMUNE_MEM2_SNAPSHOT_SIZE + 64) {
        memcpy(Files[i].data, data, size); Files[i].size = size;
    }
}
API int named_file_size(const char *basename) {
    char path[72]; int i;
    snprintf(path, sizeof(path), "/lm_states/%s", basename); i = Find(path);
    return i < 0 ? -1 : (int)Files[i].size;
}
API void *named_file_bytes(const char *basename) {
    char path[72]; int i;
    snprintf(path, sizeof(path), "/lm_states/%s", basename); i = Find(path);
    return i < 0 ? NULL : Files[i].data;
}
API int remove_named_file(const char *basename) {
    char path[72];
    snprintf(path, sizeof(path), "/lm_states/%s", basename);
    return f_unlink_char(path);
}
API u32 catalog_count(void) { return TestMailbox.catalogCount; }
API u32 catalog_id(u32 index) { return index < TestMailbox.catalogCount ? TestMailbox.catalog[index].id : 0; }
API u32 catalog_flags(u32 index) { return index < TestMailbox.catalogCount ? TestMailbox.catalog[index].flags : 0; }
API u32 catalog_config(u32 index) { return index < TestMailbox.catalogCount ? TestMailbox.catalog[index].reserved : 0; }
API void request_delete(u32 index) {
    if (index >= TestMailbox.catalogCount) return;
    TestMailbox.deleteCatalogSeq = TestMailbox.ackSeq;
    TestMailbox.deleteHeaderCrc = TestMailbox.catalogIdentity[index];
    TestMailbox.deleteBytes = TestMailbox.catalog[index].bytes;
    TestMailbox.deleteVersion = LM_STATE_STORAGE_VERSION;
    memset(TestMailbox.deleteReserved, 0, sizeof(TestMailbox.deleteReserved));
    request(LM_STATE_STORAGE_DELETE, TestMailbox.catalog[index].id, 0u);
}
API void delete_proof_word(u32 index, u32 value) {
    if (index < 8u) ((u32*)&TestMailbox.deleteCatalogSeq)[index] = value;
}
API const char *catalog_name(u32 index) {
    return index < TestMailbox.catalogCount ? TestMailbox.catalog[index].name : "";
}
