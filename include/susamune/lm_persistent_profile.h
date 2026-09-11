#ifndef SUSAMUNE_LM_PERSISTENT_PROFILE_H
#define SUSAMUNE_LM_PERSISTENT_PROFILE_H

#define LM_PERSISTENT_MAGIC 0x4C4D5052u
#define LM_PERSISTENT_RECORDS 48u
#define LM_PERSISTENT_ANCHORS 64u
typedef int (*LmPersistentReadWord)(void*, unsigned int, unsigned int*);
typedef struct {
    unsigned int heap, node, tag, bytes, previous, next;
} LmPersistentAllocation;
typedef struct {
    unsigned int magic, version, generation, checksum;
    unsigned int configId, rootHeap, systemHeap, count;
    unsigned int anchors[LM_PERSISTENT_ANCHORS];
    LmPersistentAllocation allocations[LM_PERSISTENT_RECORDS];
} LmPersistentProfile;
typedef char lm_persistent_profile_size[sizeof(LmPersistentProfile) == 1440 ? 1 : -1];
typedef struct {
    void* context;
    LmPersistentReadWord read;
    unsigned int fault, value;
} LmPersistentReader;

static inline unsigned int LmPersistentChecksum(const LmPersistentProfile* p) {
    const unsigned char* data = (const unsigned char*)p;
    unsigned int crc = ~0u, i, bit;
    for (i = 0; i < sizeof(*p); ++i) {
        crc ^= i >= 12u && i < 16u ? 0u : data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xEDB88320u);
    }
    return ~crc;
}

static inline int LmPersistentProfileValid(const LmPersistentProfile* p,
    unsigned int generation) {
    return p->magic == LM_PERSISTENT_MAGIC && p->version == 1u &&
        p->generation == generation && p->count && p->count <= LM_PERSISTENT_RECORDS &&
        p->checksum == LmPersistentChecksum(p);
}

static inline int LmPersistentWord(LmPersistentReader* r, unsigned int address) {
    r->fault = address;
    r->value = 0u;
    return !(address & 3u) && address >= 0x80000000u && address <= 0x817FFFFCu &&
        r->read(r->context, address, &r->value);
}

static inline int LmPersistentList(LmPersistentReader* r, LmPersistentProfile* p,
    unsigned int heap) {
    unsigned int start, end, node, tail, previous = 0u;
    if (!LmPersistentWord(r, heap + 0x30u)) return 0;
    start = r->value;
    if (!LmPersistentWord(r, heap + 0x34u)) return 0;
    end = r->value;
    if (start >= end || start < 0x80000000u || end > 0x81800000u ||
        ((start | end) & 3u) || !LmPersistentWord(r, heap + 0x7Cu)) return 0;
    node = r->value;
    if (!LmPersistentWord(r, heap + 0x80u)) return 0;
    tail = r->value;
    while (node) {
        LmPersistentAllocation* a;
        if (p->count == LM_PERSISTENT_RECORDS || node < start || node > end - 16u ||
            !LmPersistentWord(r, node)) return 0;
        a = &p->allocations[p->count++];
        a->heap = heap; a->node = node; a->tag = r->value;
        if ((a->tag >> 16) != 0x484Du || !LmPersistentWord(r, node + 4u)) return 0;
        a->bytes = r->value;
        if ((a->bytes & 3u) || a->bytes > end - node - 16u ||
            !LmPersistentWord(r, node + 8u)) return 0;
        a->previous = r->value;
        if (a->previous != previous || !LmPersistentWord(r, node + 12u)) return 0;
        a->next = r->value;
        previous = node;
        node = a->next;
    }
    return previous == tail;
}

static inline int LmPersistentBacking(const LmPersistentProfile* p,
    unsigned int address, unsigned int bytes) {
    unsigned int i;
    if (!bytes || address < 0x80000000u || address > 0x81800000u - bytes) return 0;
    for (i = 0; i < p->count; ++i) {
        const LmPersistentAllocation* a = &p->allocations[i];
        if (a->heap == p->systemHeap && address >= a->node + 16u &&
            address - (a->node + 16u) <= a->bytes &&
            bytes <= a->bytes - (address - (a->node + 16u))) return 1;
    }
    return 0;
}

/* Only typed boot-service identities are recorded. Mutable FIFO positions,
 * frame alternation, rumble patterns and completed ARAM commands stay live. */
static inline int LmPersistentAnchors(LmPersistentReader* r, LmPersistentProfile* p) {
    static const unsigned int globals[8] = {0x804A0BA0u, 0x804A0BA4u,
        0x804A0BBCu, 0x804A0BC0u, 0x804A0BCCu, 0x804A0BD0u,
        0x804A0BD4u, 0x804A0BF8u};
    unsigned int i, n = 0u, pad, mode;
    for (i = 0; i < 8u; ++i) {
        if (!LmPersistentWord(r, globals[i])) return 0;
        p->anchors[n++] = r->value;
    }
    if (!LmPersistentBacking(p, p->anchors[0], 0x80000u) ||
        !LmPersistentBacking(p, p->anchors[2], 0x96000u) ||
        !LmPersistentBacking(p, p->anchors[3], 0x96000u) ||
        p->anchors[2] == p->anchors[3] || p->anchors[2] != p->anchors[4] ||
        p->anchors[3] != p->anchors[5]) return 0;
    for (i = 0; i < 3u; ++i) {
        const unsigned int expected = i == 0u ? p->anchors[0] :
            i == 1u ? p->anchors[0] + 0x80000u - 4u : 0x80000u;
        if (!LmPersistentWord(r, p->anchors[1] + i * 4u) || r->value != expected)
            return 0;
        p->anchors[n++] = r->value;
    }
    pad = p->anchors[7]; mode = p->anchors[6];
    if (!LmPersistentBacking(p, pad, 0x98u) || !LmPersistentWord(r, pad) ||
        r->value != 0x8038925Cu) return 0;
    p->anchors[n++] = r->value;
    if (!LmPersistentWord(r, pad + 4u) || r->value != p->systemHeap) return 0;
    p->anchors[n++] = r->value;
    if (!LmPersistentWord(r, pad + 0x74u)) return 0;
    p->anchors[n++] = r->value >> 16u;
    if (!LmPersistentWord(r, pad + 0x68u) || r->value ||
        !LmPersistentWord(r, pad + 0x78u) || r->value ||
        !LmPersistentWord(r, pad + 0x7Cu) || r->value) return 0;
    if (mode < 0x80000000u || mode > 0x81800000u - 0x3Cu) return 0;
    for (i = 0; i < 15u; ++i) {
        if (!LmPersistentWord(r, mode + i * 4u)) return 0;
        p->anchors[n++] = r->value;
    }
    for (i = 0; i < 5u; ++i) {
        const unsigned int descriptor = 0x803C8428u + i * 0x14u;
        if (!LmPersistentWord(r, descriptor)) return 0;
        p->anchors[n++] = r->value;
        if (!LmPersistentWord(r, descriptor + 4u)) return 0;
        p->anchors[n++] = r->value;
        if (!LmPersistentWord(r, descriptor + 8u) || r->value) return 0;
    }
    if (!LmPersistentWord(r, 0x803E3CF8u)) return 0;
    {
        const unsigned int data = r->value;
        const unsigned int offsets[3] = {0x180u, 0x184u, 0x1E8u};
        const unsigned int counts[3] = {0x804A042Cu, 0x804A0444u, 0x804A045Cu};
        if (!LmPersistentBacking(p, data, 0x1ECu)) return 0;
        p->anchors[n++] = data;
        for (i = 0; i < 3u; ++i) {
            if (!LmPersistentWord(r, data + offsets[i])) return 0;
            p->anchors[n++] = r->value;
            if (!LmPersistentWord(r, counts[i])) return 0;
            p->anchors[n++] = r->value;
        }
    }
    /* The audio camera targets the captured fixed renderer, not GAME camera
     * allocations. JP 800093B4 supplies these to setter 8018B3B8. */
    for (i = 0; i < 3u; ++i) {
        const unsigned int expected = i == 0u ? 0x80398780u :
            i == 1u ? 0u : 0x8039890Cu;
        if (!LmPersistentWord(r, 0x803E3D04u + i * 4u) || r->value != expected)
            return 0;
        p->anchors[n++] = r->value;
    }
    return 1;
}

static inline int LmPersistentCapture(void* context, LmPersistentReadWord read,
    unsigned int rootHeap, unsigned int systemHeap, unsigned int configId,
    unsigned int generation, LmPersistentProfile* out,
    unsigned int* fault, unsigned int* value) {
    LmPersistentReader r;
    unsigned int i;
    int valid;
    if (!out) return 0;
    for (i = 0; i < sizeof(*out) / 4u; ++i) ((unsigned int*)out)[i] = 0u;
    out->rootHeap = rootHeap; out->systemHeap = systemHeap;
    out->configId = configId; out->generation = generation;
    r.context = context; r.read = read; r.fault = r.value = 0u;
    valid = read && LmPersistentList(&r, out, rootHeap) &&
        LmPersistentList(&r, out, systemHeap) && LmPersistentAnchors(&r, out);
    if (fault) *fault = valid ? 0u : r.fault;
    if (value) *value = valid ? 0u : r.value;
    if (valid) {
        out->magic = LM_PERSISTENT_MAGIC; out->version = 1u;
        out->checksum = LmPersistentChecksum(out);
    } else {
        for (i = 0; i < sizeof(*out) / 4u; ++i) ((unsigned int*)out)[i] = 0u;
    }
    return valid;
}

static inline int LmPersistentMatch(const LmPersistentProfile* saved,
    const LmPersistentProfile* live, unsigned int* differingWord) {
    unsigned int i;
    if (!LmPersistentProfileValid(saved, live->generation) ||
        !LmPersistentProfileValid(live, live->generation)) return 0;
    for (i = 0u; i < sizeof(*saved) / 4u; ++i) {
        if (i == 3u) continue;
        if (((const unsigned int*)saved)[i] != ((const unsigned int*)live)[i]) {
            if (differingWord) *differingWord = i;
            return 0;
        }
    }
    if (differingWord) *differingWord = 0u;
    return 1;
}
#endif
