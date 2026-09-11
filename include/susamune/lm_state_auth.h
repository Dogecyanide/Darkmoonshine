#ifndef SUSAMUNE_LM_STATE_AUTH_H
#define SUSAMUNE_LM_STATE_AUTH_H

/* SipHash-2-4 tags file bytes, not object lifetime. Legacy keys are per-process;
 * persistent archives use the separate SD key and a native restore profile. */
static unsigned long long LmStateRotate(unsigned long long x, unsigned int n) {
    return (x << n) | (x >> (64 - n));
}
static void LmStateSipRound(unsigned long long *v) {
    v[0] += v[1]; v[1] = LmStateRotate(v[1], 13); v[1] ^= v[0];
    v[0] = LmStateRotate(v[0], 32);
    v[2] += v[3]; v[3] = LmStateRotate(v[3], 16); v[3] ^= v[2];
    v[0] += v[3]; v[3] = LmStateRotate(v[3], 21); v[3] ^= v[0];
    v[2] += v[1]; v[1] = LmStateRotate(v[1], 17); v[1] ^= v[2];
    v[2] = LmStateRotate(v[2], 32);
}
static unsigned long long LmStateAuthenticate(const unsigned char *p,
    unsigned int size, unsigned long long key0, unsigned long long key1) {
    unsigned long long v[4], word;
    unsigned int i, pos = 0;
    v[0] = 0x736f6d6570736575ULL ^ key0;
    v[1] = 0x646f72616e646f6dULL ^ key1;
    v[2] = 0x6c7967656e657261ULL ^ key0;
    v[3] = 0x7465646279746573ULL ^ key1;
    while (size - pos >= 8) {
        word = 0;
        for (i = 0; i < 8; ++i) word |= (unsigned long long)p[pos++] << (8 * i);
        v[3] ^= word;
        LmStateSipRound(v); LmStateSipRound(v);
        v[0] ^= word;
    }
    word = (unsigned long long)(size & 255u) << 56;
    for (i = 0; pos < size; ++i) word |= (unsigned long long)p[pos++] << (8 * i);
    v[3] ^= word;
    LmStateSipRound(v); LmStateSipRound(v);
    v[0] ^= word;
    v[2] ^= 255;
    for (i = 0; i < 4; ++i) LmStateSipRound(v);
    return v[0] ^ v[1] ^ v[2] ^ v[3];
}
/* Domain-separated header binding for v2's 64-byte envelope. The two tag
 * words are omitted; every other header byte participates in the payload key. */
static unsigned long long LmStateAuthenticateArchive(const unsigned char *header,
    const unsigned char *payload, unsigned int size,
    unsigned long long key0, unsigned long long key1) {
    unsigned char prefix[64];
    unsigned int i;
    unsigned long long derived0, derived1;
    for (i = 0; i < 64u; ++i) prefix[i] = i >= 48u && i < 56u ? 0u : header[i];
    derived0 = LmStateAuthenticate(prefix, 64u, key0 ^ 0x4C4D503248445230ULL, key1);
    derived1 = LmStateAuthenticate(prefix, 64u, key0, key1 ^ 0x4C4D503248445231ULL);
    return LmStateAuthenticate(payload, size, derived0, derived1);
}
#endif
