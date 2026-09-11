#include <susamune/lm_grain_state.h>

static unsigned int managers[2][0x518u / 4u];
static unsigned int controllers0[LM_GRAIN_SMALL_CONTROLLER_POOL_SIZE / 4u];
static unsigned int particles0[LM_GRAIN_PARTICLE_POOL_SIZE / 4u];
static unsigned int controllers1[80u * 0x1B8u / 4u];
static unsigned int particles1[LM_GRAIN_PARTICLE_POOL_SIZE / 4u];
static unsigned int calls, foreignReads, lastAddress, faultAddress, faultValue;
static unsigned int failAddress;

static unsigned int* mapped(unsigned int address) {
    unsigned int bases[6] = {LM_GRAIN_SMALL_MANAGER, LM_GRAIN_MANAGER,
        0x81000000u, 0x81010000u, 0x81200000u, 0x81300000u};
    unsigned int sizes[6] = {0x458u, 0x518u, sizeof(controllers0),
        sizeof(particles0), sizeof(controllers1), sizeof(particles1)};
    unsigned int* bytes[6] = {managers[0], managers[1], controllers0,
        particles0, controllers1, particles1};
    unsigned int i;
    if (address & 3u) return 0;
    for (i = 0u; i < 6u; ++i)
        if (address >= bases[i] && address - bases[i] < sizes[i])
            return bytes[i] + (address - bases[i]) / 4u;
    return 0;
}

int setWord(unsigned int address, unsigned int value) {
    unsigned int* p = mapped(address);
    if (!p) return 0;
    *p = value;
    return 1;
}

static void chain(unsigned int sentinel, unsigned int pool, unsigned int count,
                  unsigned int stride, unsigned int next, unsigned int prev) {
    unsigned int i;
    setWord(sentinel + next, count ? pool : sentinel);
    setWord(sentinel + prev, count ? pool + (count - 1u) * stride : sentinel);
    for (i = 0u; i < count; ++i) {
        unsigned int node = pool + i * stride;
        setWord(node + next, i + 1u < count ? node + stride : sentinel);
        setWord(node + prev, i ? node - stride : sentinel);
    }
}

void reset(void) {
    unsigned int i, manager, count, pool, particles, m;
    for (m = 0u; m < 2u; ++m) {
        manager = m ? LM_GRAIN_MANAGER : LM_GRAIN_SMALL_MANAGER;
        count = m ? 80u : 15u;
        pool = m ? 0x81200000u : 0x81000000u;
        particles = m ? 0x81300000u : 0x81010000u;
        setWord(manager, pool);
        setWord(manager + 4u, particles);
        setWord(manager + 0x60u, manager + 0xCu);
        setWord(manager + 0x21Cu, manager + 0x64u);
        setWord(manager + 0x3D8u, manager + 0x220u);
        chain(manager + 0x64u, pool, count, 0x1B8u, 0x140u, 0x144u);
        chain(manager + 0x220u, pool, 0u, 0x1B8u, 0x140u, 0x144u);
        chain(manager + 0xCu, particles, 1500u, 0x54u, 0x4Cu, 0x50u);
        for (i = 0u; i < count; ++i) {
            unsigned int node = pool + i * 0x1B8u;
            setWord(node + 0x94u, node + 0x40u);
            chain(node + 0x40u, particles, 0u, 0x54u, 0x4Cu, 0x50u);
        }
    }
    failAddress = 0u;
}

static int readWord(void* context, unsigned int address, unsigned int* value) {
    unsigned int* p;
    (void)context;
    ++calls;
    lastAddress = address;
    if (address == failAddress) return 0;
    p = mapped(address);
    if (!p) { ++foreignReads; return 0; }
    *value = *p;
    return 1;
}

static void clearMetrics(void) {
    calls = foreignReads = lastAddress = 0;
    faultAddress = faultValue = 0xAAAAAAAAu;
}

int run(unsigned int gameStart, unsigned int gameEnd, unsigned int fail) {
    failAddress = fail;
    clearMetrics();
    return LmGrainValidate(0, readWord, gameStart, gameEnd,
                           &faultAddress, &faultValue);
}

unsigned int metric(unsigned int index) {
    switch (index) {
        case 0: return calls;
        case 1: return foreignReads;
        case 2: return lastAddress;
        case 3: return faultAddress;
        case 4: return faultValue;
        default: return 0;
    }
}

int nullReader(void) {
    return LmGrainValidate(0, 0, 0x80BE4560u, 0x817FB140u, 0, 0);
}

typedef struct {
    const unsigned char* bytes;
    unsigned int size;
} MemoryContext;

static int readMemory(void* context, unsigned int address, unsigned int* value) {
    const MemoryContext* memory = (const MemoryContext*)context;
    unsigned int offset;
    ++calls;
    lastAddress = address;
    if (address < 0x80000000u || address > 0x817FFFFCu || (address & 3u)) {
        ++foreignReads;
        return 0;
    }
    offset = address - 0x80000000u;
    if (offset > memory->size || 4u > memory->size - offset) return 0;
    *value = ((unsigned int)memory->bytes[offset] << 24) |
             ((unsigned int)memory->bytes[offset + 1u] << 16) |
             ((unsigned int)memory->bytes[offset + 2u] << 8) |
             (unsigned int)memory->bytes[offset + 3u];
    return 1;
}

int runMemory(const unsigned char* bytes, unsigned int size,
              unsigned int gameStart, unsigned int gameEnd) {
    MemoryContext memory;
    memory.bytes = bytes;
    memory.size = size;
    clearMetrics();
    return LmGrainValidate(&memory, readMemory, gameStart, gameEnd,
                           &faultAddress, &faultValue);
}
