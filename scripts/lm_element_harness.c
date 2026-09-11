#include "susamune/lm_element_plan.h"
int main(void) {
    struct LmElementPlan p = {99u, 88u};
    unsigned i;
    for (i = 0; i < 4u; ++i) {
        if (LmPlanElement(i, 100u, 0u, &p) || p.type != i + 1u ||
            p.fuel != (i ? 100u : 0u)) return 1;
    }
    for (i = 1; i < 8u; ++i) {
        p.type = 99; p.fuel = 88;
        if (LmPlanElement(1, 100, i, &p) != 1 || p.type != 99 || p.fuel != 88) return 4;
    }
    if (LmPlanElement(4, 100, 0, &p) != 3 ||
        LmPlanElement(0, 0, 0, &p) != 3 ||
        LmPlanElement(0, 10001, 0, &p) != 3 ||
        LmPlanElement(0, 100, 0, 0) != 3) return 5;
    if (LmPlanElement(0, 100, 0, &p) || p.type != 1 || p.fuel != 0) return 6;
    return 0;
}
