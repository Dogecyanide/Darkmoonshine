#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_elements.hxx"
#include "lm_state.hxx"
#include "lm_warp.hxx"
#include "susamune/lm_door_state.h"
#include "susamune/lm_element_plan.h"

namespace {
u32 word(u32 a) { return *reinterpret_cast<volatile const u32 *>(a); }
bool inside(u32 address, u32 bytes, u32 start, u32 end) {
    return LmDoorRangeInside(address, bytes, start, end) != 0;
}
}

namespace LMElements {
const char *name(u32 choice) {
    const char *const names[] = {"NONE", "FIRE", "WATER", "ICE"};
    return choice < kChoiceCount ? names[choice] : "INVALID";
}
const char *statusText(Result result) {
    switch (result) {
    case Result::Applied: return "POLTERGUST TANK UPDATED";
    case Result::Busy: return "WAIT: SUCTION / SPRAY ACTIVE";
    case Result::Warping: return "WAIT FOR ROOM TRANSITION";
    case Result::NotReady: return "WAIT FOR GAME / STREAMING";
    case Result::Event: return "WAIT FOR EVENT / CUTSCENE";
    case Result::PlayerInactive: return "WAIT FOR LUIGI TO RECOVER";
    default: return "ELEMENT DATA UNAVAILABLE";
    }
}
Result apply(u32 choice, u32 player) {
    if (LMWarp::active()) return Result::Warping;
    if (!LMState::readyForActionNow()) return Result::NotReady;
    if (word(0x803C7CACu) != 0u) return Result::Event;
    const u32 heap = word(0x804A0B98u);
    if (!inside(heap, 0x38u, 0x80003100u, 0x81800000u)) return Result::Invalid;
    const u32 start = word(heap + 0x30u), end = word(heap + 0x34u);
    if (!inside(player, 0x1190u, start, end) || word(player) != 0x8034EE50u)
        return Result::Invalid;
    if (*reinterpret_cast<volatile const s16 *>(player + 0xFCu) <= 0)
        return Result::PlayerInactive;
    const u32 current = word(player + 0x1184u);
    if (current < 1u || current > 4u) return Result::Invalid;
    LmElementPlan plan;
    const int result = LmPlanElement(choice, word(0x8049B7ACu),
                                     word(player + 0x1180u), &plan);
    if (result != 0) return static_cast<Result>(result);
    // Same pair as elemental pickup; leave action, effects and story flags alone.
    *reinterpret_cast<volatile f32 *>(player + 0x1188u) = static_cast<f32>(plan.fuel);
    *reinterpret_cast<volatile u32 *>(player + 0x1184u) = plan.type;
    return Result::Applied;
}
}
#endif
