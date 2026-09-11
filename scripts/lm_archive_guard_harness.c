#include "susamune/lm_archive_guard.h"
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API
#endif
API unsigned int reasons(const struct LmArchiveGuardEntry *entry,
    unsigned int start, unsigned int end, unsigned int vtable) {
    return LmArchiveGuardReasons(entry, start, end, vtable);
}
API unsigned int failure_value(const struct LmArchiveGuardEntry *entry, unsigned int mask) {
    return LmArchiveGuardFailureValue(entry, mask);
}
API int shared_backing(const struct LmArchiveGuardEntry *entry,
    unsigned int objectOwnerHeap, unsigned int archiveHeap, unsigned int type,
    unsigned int mountSource, unsigned int gameHeap,
    const LmSharedArchiveDescriptor *parent) {
    return LmArchiveGuardSharedBackingAllowed(entry, objectOwnerHeap, archiveHeap,
        type, mountSource, gameHeap, parent);
}
