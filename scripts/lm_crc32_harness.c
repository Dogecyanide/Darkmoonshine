#include "susamune/lm_crc32.h"

unsigned int lm_crc_step(unsigned int crc, unsigned char value) {
    return LmCrc32Byte(crc, value);
}

unsigned int lm_crc_buffer(const unsigned char *data, unsigned int size,
                           unsigned int initial) {
    unsigned int crc = initial ^ 0xFFFFFFFFu, i;
    for (i = 0; i < size; ++i) crc = LmCrc32Byte(crc, data[i]);
    return crc ^ 0xFFFFFFFFu;
}

/* Independent copy of the previous production bitwise algorithm, retained
 * only in this host test for equivalence and optional throughput comparison. */
unsigned int lm_crc_bitwise(const unsigned char *data, unsigned int size,
                            unsigned int initial) {
    unsigned int crc = initial ^ 0xFFFFFFFFu, i, bit;
    for (i = 0; i < size; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}
