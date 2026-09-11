/* Root theme-file handling adapted from Moonshine V2.3.0. */
#include <stdio.h>
#include <string.h>

#include "ff_utf8.h"
#include "SusamuneThemeFiles.h"

FRESULT SusamuneThemeFindFile(char *out, size_t outSize, const char *device,
    const char *leaf, FILINFO *info)
{
    int written;
    if (!out || !outSize || !device || !leaf || !info)
        return FR_INVALID_NAME;
    if ((strcmp(device, "sd") != 0 && strcmp(device, "usb") != 0) ||
        (strcmp(leaf, "background.png") != 0 && strcmp(leaf, "bgm.mp3") != 0))
        return FR_INVALID_NAME;
    written = snprintf(out, outSize, "%s:/" DARKMOONSHINE_THEME_DIRECTORY "/%s", device, leaf);
    if (written <= 0 || (size_t)written >= outSize)
        return FR_INVALID_NAME;
    return f_stat_char(out, info);
}

FRESULT SusamuneThemeEnsureDirectory(const char *device)
{
    char path[40];
    FILINFO info;
    FRESULT result;
    if (!device || (strcmp(device, "sd") != 0 && strcmp(device, "usb") != 0))
        return FR_INVALID_NAME;
    snprintf(path, sizeof(path), "%s:/" DARKMOONSHINE_THEME_DIRECTORY, device);
    result = f_stat_char(path, &info);
    if (result == FR_OK)
        return (info.fattrib & AM_DIR) ? FR_OK : FR_EXIST;
    if (result != FR_NO_FILE && result != FR_NO_PATH)
        return result;
    result = f_mkdir_char(path);
    if (result == FR_EXIST)
    {
        result = f_stat_char(path, &info);
        if (result == FR_OK && !(info.fattrib & AM_DIR))
            return FR_EXIST;
    }
    return result;
}
