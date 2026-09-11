#pragma once

#define LZ4_FREESTANDING 1
#define LZ4_MEMORY_USAGE 15
#define LZ4_FORCE_MEMORY_ACCESS 0
#define LZ4LIB_VISIBILITY static __attribute__((unused))
#define LZ4_memcpy __builtin_memcpy
#define LZ4_memset __builtin_memset
// Volatile byte accesses prevent libc-loop recognition on the freestanding PPC.
static __attribute__((unused)) void *lm_lz4_memmove(void *destination,
    const void *source, __SIZE_TYPE__ size) {
    volatile unsigned char *to = static_cast<volatile unsigned char *>(destination);
    const volatile unsigned char *from = static_cast<const volatile unsigned char *>(source);
    if (reinterpret_cast<__UINTPTR_TYPE__>(destination) < reinterpret_cast<__UINTPTR_TYPE__>(source)) {
        for (__SIZE_TYPE__ i = 0; i < size; ++i) to[i] = from[i];
    } else {
        while (size) { --size; to[size] = from[size]; }
    }
    return destination;
}
#define LZ4_memmove lm_lz4_memmove

// The bundled freestanding compiler has no C library headers.
typedef __UINTPTR_TYPE__ uintptr_t;
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef int int32_t;
#define UINT_MAX 4294967295U
#define INT_MAX 2147483647
#undef _MSC_VER
