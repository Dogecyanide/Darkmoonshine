#include <stddef.h>
#include <string.h>
#include "susamune/lm_crash_telemetry.h"

#define CHECK(value) do { if (!(value)) return __LINE__; } while (0)
static struct LmCriticalRing physical, ppc, arm;
static struct LmCriticalRing *cache;
static unsigned int role, faults;

static void invalidate(void *address, unsigned int size) {
    size_t offset = (unsigned char *)address - (unsigned char *)cache;
    if ((offset & 31u) || size != 32u || offset + size > sizeof(*cache)) {
        faults |= 1u; return;
    }
    if ((role == 1u && offset >= 64u) || (role == 2u && offset == 32u))
        faults |= 2u;
    memcpy(address, (unsigned char *)&physical + offset, size);
}

static void publish(void *address, unsigned int size) {
    size_t offset = (unsigned char *)address - (unsigned char *)cache;
    if ((offset & 31u) || size != 32u || offset + size > sizeof(*cache)) {
        faults |= 4u; return;
    }
    if ((role == 1u && offset == 32u) || (role == 2u && offset != 32u))
        faults |= 8u;
    /* An entry must become visible before the published producer advances. */
    if (role == 1u && offset >= 64u &&
        physical.producer.cursor != ppc.producer.cursor) faults |= 16u;
    memcpy((unsigned char *)&physical + offset, address, size);
}

static const struct LmCriticalIo io = {invalidate, publish};
static void select_ppc(void) { role = 1u; cache = &ppc; }
static void select_arm(void) { role = 2u; cache = &arm; }
static void initialize(void) {
    memset(&physical, 0, sizeof(physical));
    memset(&ppc, 0xCD, sizeof(ppc));
    memset(&arm, 0, sizeof(arm));
    faults = 0u; role = 0u; cache = &arm;
    LmCriticalInit(&arm, &io);
    select_ppc();
}

static struct SusamunePhaseTrace trace(unsigned int seq, unsigned int action,
                                        unsigned int phase) {
    struct SusamunePhaseTrace value = {SUSAMUNE_PHASE_TRACE_MAGIC, seq,
        action, phase, ~phase, 0x12345678u, 9u, seq};
    return value;
}

static unsigned char mem1[0x1800000u];
static unsigned int reads, badReads;
static int read_effect(void *out, unsigned int address, unsigned int size) {
    ++reads;
    if (!LmEffectMem1Range(address, size)) { ++badReads; return 0; }
    memcpy(out, mem1 + address - 0x80000000u, size);
    return 1;
}
static void word(unsigned int address, unsigned int value) {
    memcpy(mem1 + address - 0x80000000u, &value, 4u);
}
static void effect_fixture(void) {
    memset(mem1, 0, sizeof(mem1)); reads = badReads = 0u;
    word(0x803CE4A8u, 0x811ECD80u);
    word(0x804A19B0u, 44u);
    word(0x811EB890u, 0x2Au);
    word(0x811EB898u, 0x811EC000u);
    word(0x811EB89Cu, 1u);
    word(0x811EB8B4u, 0x80379C14u);
}

int telemetry_case(unsigned int which) {
    unsigned int i;
    struct SusamunePhaseTrace value, out;
    struct { unsigned int before; struct LmEffectAux aux; unsigned int after; } box;
    initialize();
    if (which == 10u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        value = trace(2u, 3u, 0xF0u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(4u, 3u, 0xF1u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(6u, 3u, 0xF3u);
        CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(8u, 3u, 0xF4u);
        CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        select_arm(); CHECK(LmCriticalPeek(&arm, &out, &io));
        CHECK(out.phase == 0xF3u);
        LmCriticalAcknowledge(&arm, &io);
        CHECK(LmCriticalPeek(&arm, &out, &io));
        CHECK(out.phase == 0xF4u);
    } else if (which == 9u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        for (i = 0u; i < 3u; ++i) {
            value = trace(2u + 2u * i, i ? 2u : 1u,
                          i == 0u ? 0x5Fu : (i == 1u ? 0x07u : 0x6Bu));
            value.arg0 = i == 2u ? 0x8129E524u : 0u;
            value.arg1 = i == 2u ? 0xFFFFFFFFu : 0u;
            CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        }
        for (i = 0u; i < 1000u; ++i) {
            value = trace(8u + 2u * i, 3u, 0xA4u);
            CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        }
        select_arm();
        for (i = 0u; i < 3u; ++i) {
            CHECK(LmCriticalPeek(&arm, &out, &io));
            CHECK(out.sequenceBegin == 2u + 2u * i);
            CHECK(out.arg0 == (i == 2u ? 0x8129E524u : 0u));
            CHECK(out.arg1 == (i == 2u ? 0xFFFFFFFFu : 0u));
            LmCriticalAcknowledge(&arm, &io);
        }
        CHECK(!LmCriticalPeek(&arm, &out, &io));
    } else if (which == 11u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        for (i = 0u; i < 8u; ++i) {
            value = trace(2u + 2u * i, i ? 2u : 1u, i ? 0xF3u + i : 0xF9u);
            value.arg0 = 0x80399BE0u + 4u * i;
            value.arg1 = 0x80C3A2D8u + 4u * i;
            CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        }
        value = trace(20u, 3u, 0xF9u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(22u, 2u, 0x01u);
        CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        select_arm();
        for (i = 0u; i < 8u; ++i) {
            CHECK(LmCriticalPeek(&arm, &out, &io));
            CHECK(out.action == (i ? 2u : 1u));
            CHECK(out.phase == (i ? 0xF3u + i : 0xF9u));
            CHECK(out.arg0 == 0x80399BE0u + 4u * i);
            CHECK(out.arg1 == 0x80C3A2D8u + 4u * i);
            LmCriticalAcknowledge(&arm, &io);
        }
        CHECK(LmCriticalPeek(&arm, &out, &io));
        CHECK(out.action == 2u && out.phase == 0x01u && out.sequenceBegin == 22u);
        LmCriticalAcknowledge(&arm, &io);
        CHECK(!LmCriticalPeek(&arm, &out, &io));
    } else if (which == 12u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        for (i = 0u; i < 2u; ++i) {
            value = trace(2u + 2u * i, 2u, 0xD9u + i);
            value.arg0 = i ? 0x907F0B39u : 0x80000014u;
            value.arg1 = i ? 0x80C399A0u : 0x80C39910u;
            CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        }
        for (i = 0u; i < 1000u; ++i) {
            value = trace(6u + 2u * i, 3u, 0xA4u);
            CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        }
        value = trace(2008u, 1u, 0xD9u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        select_arm();
        for (i = 0u; i < 2u; ++i) {
            CHECK(LmCriticalPeek(&arm, &out, &io));
            CHECK(out.action == 2u && out.phase == 0xD9u + i);
            CHECK(out.arg0 == (i ? 0x907F0B39u : 0x80000014u));
            CHECK(out.arg1 == (i ? 0x80C399A0u : 0x80C39910u));
            LmCriticalAcknowledge(&arm, &io);
        }
        CHECK(!LmCriticalPeek(&arm, &out, &io));
    } else if (which == 0u) {
        /* New kernel / old mod remains unenrolled; old kernel is unsupported. */
        select_arm(); CHECK(!LmCriticalPeek(&arm, &out, &io));
        CHECK(!LmCriticalControlValid(&physical.producer));
        physical.consumer.version = LM_CRITICAL_VERSION - 1u;
        select_ppc(); CHECK(!LmCriticalAttach(&ppc, &io));
        physical.consumer.version = LM_CRITICAL_VERSION;
        CHECK(LmCriticalAttach(&ppc, &io));
        CHECK(LmCriticalControlValid(&physical.producer));
        memset(&physical, 0, sizeof(physical));
        CHECK(!LmCriticalAttach(&ppc, &io));
        value = trace(2u, 1u, 0x7Fu);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
    } else if (which == 1u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        for (i = 0u; i < 32u; ++i) {
            value = trace(2u + i * 2u, (i & 1u) ? 4u : 1u, 0x7Fu);
            CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        }
        value = trace(66u, 4u, 1u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        CHECK(physical.producer.dropped == 1u);
        select_arm();
        for (i = 0u; i < 32u; ++i) {
            CHECK(LmCriticalPeek(&arm, &out, &io));
            CHECK(out.sequenceBegin == 2u + i * 2u);
            LmCriticalAcknowledge(&arm, &io);
        }
        CHECK(!LmCriticalPeek(&arm, &out, &io));
    } else if (which == 2u || which == 3u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        if (which == 3u) {
            physical.producer.cursor = physical.consumer.cursor = 0xFFFFFFF0u;
            physical.producer.cursorInverse = physical.consumer.cursorInverse = 15u;
            ppc.producer = physical.producer;
            arm.consumer = physical.consumer;
        }
        for (i = 0u; i < 100u; ++i) {
            select_ppc(); value = trace(2u + i * 2u, 4u, 0x10u);
            CHECK(LmCriticalEnqueue(&ppc, &value, &io));
            select_arm(); CHECK(LmCriticalPeek(&arm, &out, &io));
            CHECK(out.sequenceBegin == value.sequenceBegin);
            LmCriticalAcknowledge(&arm, &io);
        }
    } else if (which == 4u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        value = trace(2u, 3u, 0x8Du); CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(2u, 5u, 0x7Fu); CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        value = trace(2u, 4u, 1u); CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        physical.producer.cursorInverse ^= 1u;
        select_arm(); CHECK(!LmCriticalPeek(&arm, &out, &io));
        physical.producer.cursorInverse ^= 1u;
        physical.entries[0].sequenceEnd ^= 2u;
        CHECK(!LmCriticalPeek(&arm, &out, &io));
        CHECK(physical.consumer.cursor == 0u);
        physical.entries[0].sequenceEnd ^= 2u;
        CHECK(LmCriticalPeek(&arm, &out, &io));
    } else if (which == 5u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        ppc.producer.cursor = 32u; ppc.producer.cursorInverse = ~32u;
        ppc.producer.dropped = 0xFFFFFFFFu; ppc.producer.droppedInverse = 0u;
        value = trace(2u, 4u, 1u);
        CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        CHECK(physical.producer.dropped == 0xFFFFFFFFu);
    } else if (which == 6u || which == 7u) {
        effect_fixture(); box.before = box.after = 0xA55AA55Au;
        memset(&box.aux, 0xCD, sizeof(box.aux));
        CHECK(!LmCaptureEffectAux(&box.aux, 0x801717E4u, 0x811EB890u, read_effect));
        CHECK(reads == 0u && box.aux.magic == 0xCDCDCDCDu);
        if (which == 7u) {
            word(0x811EB898u, 0x817FFFF0u); /* extent crosses MEM1 end */
            word(0x811EB8B4u, 0xCC000000u); /* I/O must never be read */
        }
        CHECK(LmCaptureEffectAux(&box.aux, 0x801717E0u, 0x811EB890u, read_effect));
        CHECK(box.aux.magic == LM_EFFECT_AUX_MAGIC && box.aux.size == 268u);
        CHECK(box.aux.flags == (which == 6u ? 15u : 5u));
        CHECK(box.aux.owner[0] == 0x2Au && box.aux.managerCount == 44u);
        CHECK(box.aux.slots[1] == 0u); /* null model captured, never followed */
        CHECK(box.before == 0xA55AA55Au && box.after == 0xA55AA55Au && !badReads);
        CHECK(LmCaptureEffectAux(&box.aux, 0x801717E0u, 0u, read_effect));
        CHECK(box.aux.flags == LM_EFFECT_HEAP_VALID && box.aux.slotsBase == 0u);
        CHECK(!LmEffectMem1Range(0xFFFFFFFCu, 8u));
        CHECK(!LmEffectMem1Range(0x80000001u, 4u));
        CHECK(!LmEffectMem1Range(0x80000000u, 0u));
    } else if (which == 8u) {
        CHECK(LmCriticalAttach(&ppc, &io));
        value = trace(2280u, 1u, 0x7Fu);
        CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        /* A later latest-value phase cannot replace this save anchor. */
        for (i = 0u; i < 1000u; ++i) {
            value = trace(2282u + i * 2u, 3u, 0xE6u);
            CHECK(!LmCriticalEnqueue(&ppc, &value, &io));
        }
        value = trace(4282u, 4u, 1u);
        CHECK(LmCriticalEnqueue(&ppc, &value, &io));
        select_arm(); CHECK(LmCriticalPeek(&arm, &out, &io));
        CHECK(out.action == 1u && out.phase == 0x7Fu && out.sequenceBegin == 2280u);
        LmCriticalAcknowledge(&arm, &io);
        CHECK(LmCriticalPeek(&arm, &out, &io));
        CHECK(out.action == 4u && out.sequenceBegin == 4282u);
        CHECK(physical.producer.dropped == 0u);
    } else return -1;
    CHECK(faults == 0u);
    return 0;
}
