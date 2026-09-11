#include "susamune/lm_state_deflate.h"
#include "../lm_diag/vendor/lz4/state_lz4_config.h"
extern "C" void *fast_move(void *destination, const void *source, __SIZE_TYPE__ size) {
    return LZ4_memmove(destination, source, size);
}
extern "C" unsigned int staging_tail(unsigned int oldRaw, unsigned int targetRaw,
    unsigned int trailer, unsigned int limit) {
    return LmStateStagingTail(oldRaw, targetRaw, trailer, limit);
}
