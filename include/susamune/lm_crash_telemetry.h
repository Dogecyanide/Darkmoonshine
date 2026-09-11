#ifndef SUSAMUNE_LM_CRASH_TELEMETRY_H
#define SUSAMUNE_LM_CRASH_TELEMETRY_H

#include "susamune/crash_report.h"

#define LM_CRITICAL_MAGIC 0x4C4D4351u /* LMCQ */
#define LM_CRITICAL_VERSION 2u
#define LM_CRITICAL_OFFSET 0x820u
#define LM_CRITICAL_COUNT 32u

struct LmCriticalControl {
    unsigned int magic, version, cursor, cursorInverse;
    unsigned int dropped, droppedInverse, reserved[2];
};

struct LmCriticalRing {
    struct LmCriticalControl producer; /* PPC-owned cache line */
    struct LmCriticalControl consumer; /* ARM-owned cache line/capability */
    struct SusamunePhaseTrace entries[LM_CRITICAL_COUNT];
};

struct LmCriticalIo {
    void (*invalidate)(void *, unsigned int);
    void (*publish)(void *, unsigned int);
};

static inline int LmPhaseValid(const struct SusamunePhaseTrace *trace) {
    return trace->magic == SUSAMUNE_PHASE_TRACE_MAGIC &&
        trace->sequenceBegin != 0u && !(trace->sequenceBegin & 1u) &&
        trace->sequenceBegin == trace->sequenceEnd &&
        (trace->phase ^ trace->phaseInverse) == 0xFFFFFFFFu &&
        trace->action >= SUSAMUNE_PHASE_ACTION_SAVE &&
        trace->action <= SUSAMUNE_PHASE_ACTION_WARP;
}

static inline int LmPhaseCritical(const struct SusamunePhaseTrace *trace) {
    return trace->action == SUSAMUNE_PHASE_ACTION_WARP ||
        (trace->action == SUSAMUNE_PHASE_ACTION_POST_LOAD &&
         (trace->phase == 0xF4u || trace->phase == 0xF3u)) ||
        (trace->action == SUSAMUNE_PHASE_ACTION_SAVE &&
         (trace->phase == 0x7Fu || trace->phase == 0x5Fu || trace->phase == 0xF9u)) ||
        (trace->action == SUSAMUNE_PHASE_ACTION_LOAD &&
         (trace->phase == 0x01u || trace->phase == 0x07u || trace->phase == 0x6Bu ||
          trace->phase == 0xD9u || trace->phase == 0xDAu ||
          (trace->phase >= 0xF4u && trace->phase <= 0xFAu)));
}

static inline int LmCriticalControlValid(const struct LmCriticalControl *control) {
    return control->magic == LM_CRITICAL_MAGIC &&
        control->version == LM_CRITICAL_VERSION &&
        (control->cursor ^ control->cursorInverse) == 0xFFFFFFFFu &&
        (control->dropped ^ control->droppedInverse) == 0xFFFFFFFFu;
}

static inline void LmCriticalControlInit(struct LmCriticalControl *control) {
    control->magic = LM_CRITICAL_MAGIC;
    control->version = LM_CRITICAL_VERSION;
    control->cursor = control->dropped = 0u;
    control->cursorInverse = control->droppedInverse = 0xFFFFFFFFu;
    control->reserved[0] = control->reserved[1] = 0u;
}

/* The initializer owns the whole reservation before publishing capability. */
static inline void LmCriticalInit(struct LmCriticalRing *ring,
                                  const struct LmCriticalIo *io) {
    LmCriticalControlInit(&ring->producer);
    ring->producer.magic = 0u; /* Older PPC builds never enroll. */
    LmCriticalControlInit(&ring->consumer);
    io->publish(&ring->producer, sizeof(ring->producer));
    io->publish(&ring->consumer, sizeof(ring->consumer));
}

static inline int LmCriticalAttach(struct LmCriticalRing *ring,
                                   const struct LmCriticalIo *io) {
    io->invalidate(&ring->consumer, sizeof(ring->consumer));
    io->invalidate(&ring->producer, sizeof(ring->producer));
    if (!LmCriticalControlValid(&ring->consumer)) return 0;
    if (ring->producer.magic == 0u && ring->consumer.cursor == 0u) {
        LmCriticalControlInit(&ring->producer);
        io->publish(&ring->producer, sizeof(ring->producer));
    }
    return LmCriticalControlValid(&ring->producer);
}

/* No waits or overwrites of unacknowledged records. Zero means legacy or full. */
static inline int LmCriticalEnqueue(struct LmCriticalRing *ring,
                                    const struct SusamunePhaseTrace *trace,
                                    const struct LmCriticalIo *io) {
    unsigned int pending;
    io->invalidate(&ring->consumer, sizeof(ring->consumer));
    if (!LmCriticalControlValid(&ring->consumer) ||
        !LmCriticalControlValid(&ring->producer) || !LmPhaseValid(trace) ||
        !LmPhaseCritical(trace)) return 0;
    pending = ring->producer.cursor - ring->consumer.cursor;
    if (pending >= LM_CRITICAL_COUNT) {
        if (ring->producer.dropped != 0xFFFFFFFFu) ++ring->producer.dropped;
        ring->producer.droppedInverse = ~ring->producer.dropped;
        io->publish(&ring->producer, sizeof(ring->producer));
        return 0;
    }
    ring->entries[ring->producer.cursor % LM_CRITICAL_COUNT] = *trace;
    io->publish(&ring->entries[ring->producer.cursor % LM_CRITICAL_COUNT],
                sizeof(*trace));
    ++ring->producer.cursor;
    ring->producer.cursorInverse = ~ring->producer.cursor;
    io->publish(&ring->producer, sizeof(ring->producer));
    return 1;
}

/* ARM peeks without acknowledging; it releases only after record handling. */
static inline int LmCriticalPeek(struct LmCriticalRing *ring,
                                 struct SusamunePhaseTrace *trace,
                                 const struct LmCriticalIo *io) {
    unsigned int pending;
    io->invalidate(&ring->producer, sizeof(ring->producer));
    if (!LmCriticalControlValid(&ring->producer) ||
        !LmCriticalControlValid(&ring->consumer)) return 0;
    pending = ring->producer.cursor - ring->consumer.cursor;
    if (!pending || pending > LM_CRITICAL_COUNT) return 0;
    io->invalidate(&ring->entries[ring->consumer.cursor % LM_CRITICAL_COUNT],
                   sizeof(*trace));
    *trace = ring->entries[ring->consumer.cursor % LM_CRITICAL_COUNT];
    return LmPhaseValid(trace) && LmPhaseCritical(trace);
}

static inline void LmCriticalAcknowledge(struct LmCriticalRing *ring,
                                        const struct LmCriticalIo *io) {
    ++ring->consumer.cursor;
    ring->consumer.cursorInverse = ~ring->consumer.cursor;
    io->publish(&ring->consumer, sizeof(ring->consumer));
}

#define LM_CRITICAL_PPC_PTR ((struct LmCriticalRing *)(SUSAMUNE_MEM2_CRASH_PPC_BASE + LM_CRITICAL_OFFSET))
#define LM_CRITICAL_PHYS_PTR ((struct LmCriticalRing *)(SUSAMUNE_MEM2_CRASH_PHYS_BASE + LM_CRITICAL_OFFSET))

/* DVD/ARAM prefixes and the outer v1 report remain unchanged for old readers. */
#define LM_EFFECT_AUX_OFFSET 0x28u
#define LM_EFFECT_AUX_MAGIC 0x4C4D4558u /* LMEX */
#define LM_EFFECT_AUX_VERSION 1u
#define LM_EFFECT_OWNER_VALID (1u << 0)
#define LM_EFFECT_SLOTS_VALID (1u << 1)
#define LM_EFFECT_HEAP_VALID (1u << 2)
#define LM_EFFECT_CONFIG_VALID (1u << 3)

struct LmEffectAux {
    unsigned int magic;
    unsigned short version, size;
    unsigned int flags, ownerBase, slotsBase, heapBase, configBase, managerCount;
    unsigned int owner[10];
    unsigned int slots[9];
    unsigned int heap[32];
    unsigned int config[8];
};

static inline int LmEffectMem1Range(unsigned int address, unsigned int size) {
    return size && !(address & 3u) && size <= 0x01800000u &&
        address >= 0x80000000u && address <= 0x81800000u - size;
}

typedef int (*LmEffectRead)(void *, unsigned int, unsigned int);

static inline int LmEffectCopy(void *out, unsigned int address,
                               unsigned int size, LmEffectRead read) {
    return LmEffectMem1Range(address, size) && read(out, address, size);
}

static inline int LmCaptureEffectAux(struct LmEffectAux *aux, unsigned int pc,
                                     unsigned int owner, LmEffectRead read) {
    unsigned int i;
    unsigned char *bytes = (unsigned char *)aux;
    /* The register meaning is proven only at this GLMJ01 instruction. */
    if (pc != 0x801717E0u) return 0;
    for (i = 0u; i < sizeof(*aux); ++i) bytes[i] = 0u;
    aux->magic = LM_EFFECT_AUX_MAGIC;
    aux->version = LM_EFFECT_AUX_VERSION;
    aux->size = sizeof(*aux);
    aux->ownerBase = owner;
    LmEffectCopy(&aux->heapBase, 0x803CE4A8u, 4u, read);
    LmEffectCopy(&aux->managerCount, 0x804A19B0u, 4u, read);
    if (LmEffectCopy(aux->owner, owner, sizeof(aux->owner), read)) {
        aux->flags |= LM_EFFECT_OWNER_VALID;
        aux->slotsBase = aux->owner[2];
        aux->configBase = aux->owner[9];
        if (aux->owner[3] && LmEffectCopy(aux->slots, aux->slotsBase,
                                         sizeof(aux->slots), read))
            aux->flags |= LM_EFFECT_SLOTS_VALID;
        if (LmEffectCopy(aux->config, aux->configBase, sizeof(aux->config), read))
            aux->flags |= LM_EFFECT_CONFIG_VALID;
    }
    if (LmEffectCopy(aux->heap, aux->heapBase, sizeof(aux->heap), read))
        aux->flags |= LM_EFFECT_HEAP_VALID;
    return 1;
}

typedef char lm_critical_control_size_check[(sizeof(struct LmCriticalControl) == 32) ? 1 : -1];
typedef char lm_critical_end_check[(LM_CRITICAL_OFFSET + sizeof(struct LmCriticalRing) <= SUSAMUNE_MEM2_CRASH_SIZE) ? 1 : -1];
typedef char lm_critical_no_trace_overlap_check[(LM_CRITICAL_OFFSET >= SUSAMUNE_PHASE_TRACE_OFFSET + SUSAMUNE_PHASE_TRACE_SIZE) ? 1 : -1];
typedef char lm_effect_aux_size_check[(sizeof(struct LmEffectAux) == 268 && LM_EFFECT_AUX_OFFSET + sizeof(struct LmEffectAux) <= SUSAMUNE_CRASH_DIRECTOR_SIZE) ? 1 : -1];

#endif
