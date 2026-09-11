#ifndef SUSAMUNE_LM_GRAIN_STATE_H
#define SUSAMUNE_LM_GRAIN_STATE_H

typedef char LmGrainWordMustBe32Bits[sizeof(unsigned int) == 4 ? 1 : -1];
typedef int (*LmGrainReadWord)(void* context, unsigned int address,
                              unsigned int* value);

#define LM_GRAIN_MANAGER 0x803CBF48u
#define LM_GRAIN_SMALL_MANAGER 0x803CBAF0u
#define LM_GRAIN_SMALL_CONTROLLER_COUNT 15u
#define LM_GRAIN_CONTROLLER_COUNT 80u
#define LM_GRAIN_CONTROLLER_STRIDE 0x1B8u
#define LM_GRAIN_CONTROLLER_POOL_SIZE \
    (LM_GRAIN_CONTROLLER_COUNT * LM_GRAIN_CONTROLLER_STRIDE)
#define LM_GRAIN_SMALL_CONTROLLER_POOL_SIZE \
    ((LM_GRAIN_SMALL_CONTROLLER_COUNT * LM_GRAIN_CONTROLLER_STRIDE + 31u) & ~31u)
#define LM_GRAIN_PARTICLE_COUNT 1500u
#define LM_GRAIN_PARTICLE_STRIDE 0x54u
#define LM_GRAIN_PARTICLE_POOL_SIZE \
    ((LM_GRAIN_PARTICLE_COUNT * LM_GRAIN_PARTICLE_STRIDE + 31u) & ~31u)

typedef struct {
    void* context;
    LmGrainReadWord read;
    unsigned int address, value;
} LmGrainReader;

static inline int LmGrainRead(LmGrainReader* reader, unsigned int address) {
    reader->address = address;
    reader->value = 0;
    return reader->read(reader->context, address, &reader->value);
}

static inline int LmGrainExpected(LmGrainReader* reader, unsigned int address,
                                 unsigned int expected) {
    return LmGrainRead(reader, address) && reader->value == expected;
}

/* A seen bit belongs to the entire pool, not one list: two otherwise-valid
 * lists must never claim the same node. The bound is independent of links. */
static inline int LmGrainList(LmGrainReader* reader, unsigned int sentinel,
                             unsigned int pool, unsigned int count,
                             unsigned int stride, unsigned int nextOffset,
                             unsigned int previousOffset,
                             unsigned int* seen, unsigned int* visited) {
    unsigned int previous = sentinel;
    if (!LmGrainRead(reader, sentinel + nextOffset)) return 0;
    while (reader->value != sentinel) {
        unsigned int node = reader->value;
        unsigned int index, bit;
        if (node < pool || node - pool >= count * stride ||
            (node - pool) % stride != 0u) return 0;
        index = (node - pool) / stride;
        bit = 1u << (index & 31u);
        if (seen[index >> 5] & bit) return 0;
        seen[index >> 5] |= bit;
        ++*visited;
        if (!LmGrainExpected(reader, node + previousOffset, previous)) return 0;
        previous = node;
        if (!LmGrainRead(reader, node + nextOffset)) return 0;
    }
    return LmGrainExpected(reader, sentinel + previousOffset, previous);
}

static inline int LmGrainManagerValid(LmGrainReader* reader,
                                      unsigned int manager,
                                      unsigned int controllers,
                                      unsigned int controllerCount,
                                      unsigned int particles) {
    unsigned int controllerSeen[(LM_GRAIN_CONTROLLER_COUNT + 31u) / 32u];
    unsigned int particleSeen[(LM_GRAIN_PARTICLE_COUNT + 31u) / 32u];
    unsigned int controllerVisited = 0u, particleVisited = 0u, i;
    for (i = 0u; i < (LM_GRAIN_CONTROLLER_COUNT + 31u) / 32u; ++i)
        controllerSeen[i] = 0u;
    for (i = 0u; i < (LM_GRAIN_PARTICLE_COUNT + 31u) / 32u; ++i)
        particleSeen[i] = 0u;

    if (!LmGrainExpected(reader, manager + 0x60u, manager + 0xCu) ||
        !LmGrainExpected(reader, manager + 0x21Cu, manager + 0x64u) ||
        !LmGrainExpected(reader, manager + 0x3D8u, manager + 0x220u)) return 0;
    if (!LmGrainList(reader, manager + 0x64u, controllers, controllerCount,
                     LM_GRAIN_CONTROLLER_STRIDE, 0x140u, 0x144u,
                     controllerSeen, &controllerVisited) ||
        !LmGrainList(reader, manager + 0x220u, controllers, controllerCount,
                     LM_GRAIN_CONTROLLER_STRIDE, 0x140u, 0x144u,
                     controllerSeen, &controllerVisited)) return 0;
    if (controllerVisited != controllerCount) {
        reader->address = manager;
        reader->value = controllerVisited;
        return 0;
    }
    if (!LmGrainList(reader, manager + 0xCu, particles, LM_GRAIN_PARTICLE_COUNT,
                     LM_GRAIN_PARTICLE_STRIDE, 0x4Cu, 0x50u,
                     particleSeen, &particleVisited)) return 0;
    for (i = 0u; i < controllerCount; ++i) {
        unsigned int node = controllers + i * LM_GRAIN_CONTROLLER_STRIDE;
        if (!LmGrainExpected(reader, node + 0x94u, node + 0x40u) ||
            !LmGrainList(reader, node + 0x40u, particles, LM_GRAIN_PARTICLE_COUNT,
                         LM_GRAIN_PARTICLE_STRIDE, 0x4Cu, 0x50u,
                         particleSeen, &particleVisited)) return 0;
    }
    if (particleVisited != LM_GRAIN_PARTICLE_COUNT) {
        reader->address = manager + 4u;
        reader->value = particleVisited;
        return 0;
    }
    return 1;
}

/* Read-only. The callback maps either saved bytes or live MEM1; it must not
 * dereference the native address until its own backing extent is checked. */
static inline int LmGrainValidate(void* context, LmGrainReadWord readWord,
                                  unsigned int gameStart, unsigned int gameEnd,
                                  unsigned int* faultAddress,
                                  unsigned int* faultValue) {
    LmGrainReader reader;
    unsigned int pools[4], sizes[4];
    unsigned int i, j;
    reader.context = context;
    reader.read = readWord;
    reader.address = gameStart;
    reader.value = gameEnd;
    if (!readWord || gameStart < 0x80003100u || gameEnd > 0x81800000u ||
        gameStart >= gameEnd || ((gameStart | gameEnd) & 3u))
        goto failed;

    sizes[0] = LM_GRAIN_SMALL_CONTROLLER_POOL_SIZE;
    sizes[1] = LM_GRAIN_PARTICLE_POOL_SIZE;
    sizes[2] = LM_GRAIN_CONTROLLER_POOL_SIZE;
    sizes[3] = LM_GRAIN_PARTICLE_POOL_SIZE;
    for (i = 0u; i < 4u; ++i) {
        unsigned int manager = i < 2u ? LM_GRAIN_SMALL_MANAGER : LM_GRAIN_MANAGER;
        unsigned int pool;
        if (!LmGrainRead(&reader, manager + (i & 1u) * 4u)) goto failed;
        pool = reader.value;
        if ((pool & 3u) || pool < gameStart || pool > gameEnd ||
            sizes[i] > gameEnd - pool) goto failed;
        if (pool < 0x803CC460u && pool + sizes[i] > LM_GRAIN_SMALL_MANAGER)
            goto failed;
        for (j = 0u; j < i; ++j)
            if (pool < pools[j] + sizes[j] && pools[j] < pool + sizes[i])
                goto failed;
        pools[i] = pool;
    }
    if (!LmGrainManagerValid(&reader, LM_GRAIN_SMALL_MANAGER, pools[0],
                             LM_GRAIN_SMALL_CONTROLLER_COUNT, pools[1]) ||
        !LmGrainManagerValid(&reader, LM_GRAIN_MANAGER, pools[2],
                             LM_GRAIN_CONTROLLER_COUNT, pools[3])) goto failed;
    if (faultAddress) *faultAddress = 0;
    if (faultValue) *faultValue = 0;
    return 1;

failed:
    if (faultAddress) *faultAddress = reader.address;
    if (faultValue) *faultValue = reader.value;
    return 0;
}

#endif
