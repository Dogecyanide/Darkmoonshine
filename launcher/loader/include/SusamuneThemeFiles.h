#ifndef SUSAMUNE_THEME_FILES_H
#define SUSAMUNE_THEME_FILES_H

#include <stddef.h>
#include "ff.h"

#define DARKMOONSHINE_THEME_DIRECTORY "Darkmoonshine_Theme"

FRESULT SusamuneThemeEnsureDirectory(const char *device);
FRESULT SusamuneThemeFindFile(char *out, size_t outSize, const char *device,
    const char *leaf, FILINFO *info);

#endif
