#define LM_PREFERENCES_HOST_TEST 1
#include "lm_preferences_test_shim.h"
#include "susamune/lm_preferences.h"

struct LmPreferencesBlock TestBlock;
u32 GAME_ID;
static char TestPath[128];
static bool StorageReady;
static u32 Fail, RenameCount, OpenCount, FlushOffset, FlushSize;

FRESULT f_open_char(FIL *file, const char *path, unsigned int flags) {
    ++OpenCount;
    file->writing = (flags & FA_WRITE) != 0;
    file->stream = fopen(path, file->writing ? "wb" : "rb");
    if (!file->stream) return errno == ENOENT ? FR_NO_FILE : FR_DISK_ERR;
    if (fseek(file->stream, 0, SEEK_END)) return FR_DISK_ERR;
    file->size = (u32)ftell(file->stream);
    rewind(file->stream);
    return FR_OK;
}
FRESULT f_read(FIL *file, void *buffer, UINT count, UINT *got) {
    if (Fail == 1 && count > file->size && file->size) count = file->size - 1;
    *got = (UINT)fread(buffer, 1, count, file->stream);
    return ferror(file->stream) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *file, const void *buffer, UINT count, UINT *written) {
    if (Fail == 2 && count) --count;
    *written = (UINT)fwrite(buffer, 1, count, file->stream);
    return ferror(file->stream) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *file) {
    int result = fclose(file->stream);
    return result || (Fail == 4 && file->writing) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_sync(FIL *file) {
    return fflush(file->stream) || Fail == 3 ? FR_DISK_ERR : FR_OK;
}
FRESULT f_unlink_char(const char *path) {
    if (Fail == 5) return FR_DISK_ERR;
    return remove(path) == 0 ? FR_OK : errno == ENOENT ? FR_NO_FILE : FR_DISK_ERR;
}
FRESULT f_rename(const WCHAR *source, const WCHAR *destination) {
    char a[128], b[128];
    u32 i;
    ++RenameCount;
    if ((Fail == 6 && RenameCount == 1) ||
        (Fail == 7 && RenameCount == 2) ||
        (Fail == 8 && RenameCount >= 2)) return FR_DISK_ERR;
    for (i = 0; source[i]; ++i) a[i] = (char)source[i];
    a[i] = 0;
    for (i = 0; destination[i]; ++i) b[i] = (char)destination[i];
    b[i] = 0;
    return rename(a, b) ? FR_DISK_ERR : FR_OK;
}
void sync_before_read(void *ptr, u32 size) { (void)ptr; (void)size; }
void sync_after_write(void *ptr, u32 size) {
    FlushOffset = (u32)((char *)ptr - (char *)&TestBlock);
    FlushSize = size;
}
const char *SusamuneCfgIniPath(void) { return TestPath; }
bool SusamuneCfgStorageAvailable(void) { return StorageReady; }
void *TestAllocate(u32 size) { return Fail == 9 ? NULL : malloc(size); }

#include "../launcher/kernel/LmPreferences.c"

void setup(const char *path, u32 game, u32 ready) {
    strncpy(TestPath, path, sizeof(TestPath) - 1u);
    GAME_ID = game; StorageReady = ready != 0;
    Fail = RenameCount = OpenCount = FlushOffset = FlushSize = 0;
    memset(&TestBlock, 0, sizeof(TestBlock));
}
void failure(u32 mode) { Fail = mode; RenameCount = 0; }
void *block(void) { return &TestBlock; }
u32 valid(void) { return LmPreferencesValid(&TestBlock); }
void checksum(void) {
    TestBlock.checksum = LmPreferencesChecksum(TestBlock.values,
                                             TestBlock.presentLo, TestBlock.presentHi);
}
u32 parse(const char *text, u32 size) { return Parse(text, size, &TestBlock); }
u32 metric(u32 id) {
    return id == 0 ? OpenCount : id == 1 ? FlushOffset : FlushSize;
}
