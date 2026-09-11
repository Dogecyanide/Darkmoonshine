#ifndef LM_PREFERENCES_TEST_SHIM_H
#define LM_PREFERENCES_TEST_SHIM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t WCHAR;
typedef unsigned int UINT;
typedef int FRESULT;
typedef struct { FILE *stream; u32 size; bool writing; } FIL;
enum { FR_OK, FR_DISK_ERR, FR_NO_FILE = 4, FR_INVALID_NAME = 6 };
enum { FA_READ = 1, FA_WRITE = 2, FA_CREATE_ALWAYS = 8 };
struct LmPreferencesBlock;
extern struct LmPreferencesBlock TestBlock;
#define _sprintf sprintf
FRESULT f_open_char(FIL *, const char *, unsigned int);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_close(FIL *);
FRESULT f_sync(FIL *);
FRESULT f_unlink_char(const char *);
FRESULT f_rename(const WCHAR *, const WCHAR *);
static inline u32 f_size(FIL *file) { return file->size; }
void sync_before_read(void *, u32);
void sync_after_write(void *, u32);
const char *SusamuneCfgIniPath(void);
bool SusamuneCfgStorageAvailable(void);
void *TestAllocate(u32 size);
#endif
