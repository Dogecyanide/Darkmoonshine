#ifndef LM_STATE_STORAGE_KERNEL_H
#define LM_STATE_STORAGE_KERNEL_H
#include "global.h"
void LmStateStorageInit(void);
bool LmStateStoragePending(void);
void LmStateStorageService(void);
#endif
