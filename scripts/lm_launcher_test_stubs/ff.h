#ifndef LM_LAUNCHER_TEST_FF_H
#define LM_LAUNCHER_TEST_FF_H
typedef int FRESULT;
typedef struct { unsigned int fsize; unsigned char fattrib; } FILINFO;
enum { FR_OK, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE, FR_NO_PATH,
       FR_INVALID_NAME, FR_DENIED, FR_EXIST };
#define AM_DIR 0x10
#endif
