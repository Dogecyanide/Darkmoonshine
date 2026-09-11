#include <string.h>
#include "lm_launcher_test_stubs/ff.h"
static int StatResult, LaterResult, MkdirResult, Attribute, LaterAttribute;
static unsigned int Stats, Mkdirs;
static char LastPath[128];
FRESULT f_stat_char(const char *path, FILINFO *info) {
    strncpy(LastPath, path, sizeof(LastPath)-1);
    info->fattrib = Stats ? LaterAttribute : Attribute;
    return Stats++ ? LaterResult : StatResult;
}
FRESULT f_mkdir_char(const char *path) {
    strncpy(LastPath, path, sizeof(LastPath)-1);
    ++Mkdirs; return MkdirResult;
}
#include "../launcher/loader/source/SusamuneThemeFiles.c"
void setup(int initial, int attribute, int mkdir_result, int later, int later_attribute) {
    StatResult=initial; Attribute=attribute; MkdirResult=mkdir_result;
    LaterResult=later; LaterAttribute=later_attribute; Stats=Mkdirs=0; LastPath[0]=0;
}
unsigned int metric(unsigned int which) { return which ? Mkdirs : Stats; }
const char *last_path(void) { return LastPath; }
int find_file(const char *device,const char *leaf,unsigned int size) {
    char path[128]; FILINFO info;
    return SusamuneThemeFindFile(path,size>sizeof(path)?sizeof(path):size,device,leaf,&info);
}
