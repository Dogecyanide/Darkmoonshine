#include "susamune/lm_state_codec.h"
#include "susamune/lm_state_auth.h"
#if defined(_WIN32)
#define API __declspec(dllexport)
#else
#define API
#endif
API unsigned int pack(const unsigned char *src, unsigned int size,
    unsigned char *dst, unsigned int capacity) {
    unsigned int table[4096];
    return LmStatePack(src, size, dst, capacity, table);
}
API int unpack(const unsigned char *src, unsigned int size,
    unsigned char *dst, unsigned int expected) {
    return LmStateUnpack(src, size, dst, expected);
}
API unsigned long long authenticate(const unsigned char *src,
    unsigned int size, unsigned long long k0, unsigned long long k1) {
    return LmStateAuthenticate(src, size, k0, k1);
}
