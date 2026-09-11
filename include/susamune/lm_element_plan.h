#ifndef LM_ELEMENT_PLAN_H
#define LM_ELEMENT_PLAN_H

struct LmElementPlan {
    unsigned type;
    unsigned fuel;
};

/* UI order NONE/FIRE/WATER/ICE matches the retail tank's types 1..4.
 * This is a practice tank preset, not a medal grant. Acquisition cutscene
 * flags do not reliably describe medal ownership on completed/practice saves. */
static inline int LmPlanElement(unsigned choice, unsigned capacity, unsigned action,
                                struct LmElementPlan *out) {
    if (!out || choice > 3u || capacity == 0u || capacity > 10000u)
        return 3;
    if (action != 0u) return 1;
    out->type = choice + 1u;
    out->fuel = choice ? capacity : 0u;
    return 0;
}

#endif
