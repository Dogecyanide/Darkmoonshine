#include "susamune/lm_warp_transition.h"

static unsigned long globals[3], flags[256], seen[5], queueResult;
static unsigned char eventCounts[LM_RETAIL_EVENT_COUNT];
static unsigned long get_flag(unsigned long id) { return flags[id]; }
static void set_flag(unsigned long id) { flags[id] = 1u; }
static void clear_flag(unsigned long id) { flags[id] = 0u; }
static unsigned long queue_scene(unsigned long scene) {
    seen[0] = scene;
    seen[1] = globals[0];
    seen[2] = globals[1];
    seen[3] = flags[222];
    ++seen[4];
    globals[2] = 1u;
    return queueResult;
}

int run(unsigned long map, unsigned long point, unsigned long success,
        unsigned long initialFlags, unsigned long countOverride) {
    struct LmWarpFlagPlan plan = LmWarpFlagsForDestination(map, point);
    unsigned long i;
    globals[0] = 13u;
    globals[1] = 55u;
    globals[2] = 7u;
    queueResult = success;
    for (i = 0u; i < 256u; ++i) flags[i] = (initialFlags >> (i % 16u)) & 1u;
    for (i = 0u; i < 5u; ++i) seen[i] = 0u;
    for (i = 0u; i < LM_RETAIL_EVENT_COUNT; ++i) eventCounts[i] = (unsigned char)(i + 1u);
    if (countOverride) plan.count = countOverride;
    return LmWarpPublishAndQueue(map, map == 2u ? 240u : 0u, &plan,
        &globals[0], &globals[1], &globals[2], get_flag, set_flag, clear_flag,
        queue_scene, eventCounts);
}

unsigned long global_value(unsigned long index) { return globals[index]; }
unsigned long flag_value(unsigned long index) { return flags[index]; }
unsigned long seen_value(unsigned long index) { return seen[index]; }
unsigned long event_value(unsigned long index) { return eventCounts[index]; }
unsigned long event_plan(unsigned long map) {
    return LmWarpFlagsForDestination(map, 0u).replayEvent;
}
unsigned long plan_value(unsigned long map, unsigned long point, unsigned long index) {
    const struct LmWarpFlagPlan plan = LmWarpFlagsForDestination(map, point);
    if (index == 0u) return plan.count;
    if (index <= 3u) return plan.ids[index - 1u];
    return plan.values[index - 4u];
}
