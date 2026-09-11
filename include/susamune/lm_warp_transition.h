#ifndef SUSAMUNE_LM_WARP_TRANSITION_H
#define SUSAMUNE_LM_WARP_TRANSITION_H

typedef unsigned long (*LmWarpWordFn)(unsigned long);
typedef void (*LmWarpFlagFn)(unsigned long);

struct LmWarpFlagPlan {
    unsigned long count, ids[3], values[3];
    unsigned long replayEvent;
};

#define LM_WARP_NO_REPLAY_EVENT 0xFFFFFFFFu
#define LM_RETAIL_EVENT_COUNT 109u

static inline struct LmWarpFlagPlan LmWarpFlagsForDestination(
    unsigned long map, unsigned long point) {
    struct LmWarpFlagPlan plan = {0u, {0u, 0u, 0u}, {0u, 0u, 0u},
                                 LM_WARP_NO_REPLAY_EVENT};
    switch (map) {
    case 10u: plan.ids[0] = 39u; plan.ids[1] = 46u; plan.count = 2u; break;
    case 13u: plan.ids[0] = 67u; plan.ids[1] = 68u; plan.count = 2u; break;
    case 11u: plan.ids[0] = 81u; plan.ids[1] = 82u; plan.count = 2u; break;
    case 9u: plan.ids[0] = 66u; plan.count = 1u; break;
    default:
        if (point == 55u || point == 56u) {
            plan.ids[0] = point == 55u ? 34u : 46u;
            plan.values[0] = 1u;
            plan.count = 1u;
        }
        return plan;
    }
    /* Clean map9/10/11/13 intro records have EventLoad=1: their persistent
     * play counters must be rearmed before native EventInfo construction. */
    plan.replayEvent = map == 9u ? 75u : map == 10u ? 64u :
                       map == 11u ? 72u : 66u;
    plan.ids[plan.count++] = 222u;
    return plan;
}

/* The native sender is nonblocking and may fail after setting pause-request.
 * Publish exactly as retail callers do, but undo every change on failure. */
static inline int LmWarpPublishAndQueue(
    unsigned long map, unsigned long point, const struct LmWarpFlagPlan *plan,
    volatile unsigned long *requestedMap, volatile unsigned long *appearance,
    volatile unsigned long *pauseRequest, LmWarpWordFn getFlag,
    LmWarpFlagFn setFlag, LmWarpFlagFn clearFlag, LmWarpWordFn queueScene,
    volatile unsigned char *eventPlayCounts) {
    unsigned long oldFlags[3], i;
    unsigned char oldEventCount = 0u;
    const unsigned long oldMap = *requestedMap;
    const unsigned long oldPoint = *appearance;
    const unsigned long oldPause = *pauseRequest;
    if (plan->count > 3u || (plan->replayEvent != LM_WARP_NO_REPLAY_EVENT &&
        (plan->replayEvent >= LM_RETAIL_EVENT_COUNT || !eventPlayCounts))) return 0;
    for (i = 0u; i < plan->count; ++i) oldFlags[i] = getFlag(plan->ids[i]);
    *requestedMap = map;
    *appearance = point;
    for (i = 0u; i < plan->count; ++i) {
        (plan->values[i] ? setFlag : clearFlag)(plan->ids[i]);
    }
    if (plan->replayEvent != LM_WARP_NO_REPLAY_EVENT) {
        oldEventCount = eventPlayCounts[plan->replayEvent];
        eventPlayCounts[plan->replayEvent] = 0u;
    }
    if (queueScene(2u) != 0u) return 1;
    if (plan->replayEvent != LM_WARP_NO_REPLAY_EVENT)
        eventPlayCounts[plan->replayEvent] = oldEventCount;
    for (i = 0u; i < plan->count; ++i) {
        (oldFlags[i] ? setFlag : clearFlag)(plan->ids[i]);
    }
    *requestedMap = oldMap;
    *appearance = oldPoint;
    *pauseRequest = oldPause;
    return 0;
}

#endif
