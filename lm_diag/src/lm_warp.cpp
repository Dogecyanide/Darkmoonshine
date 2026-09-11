#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_warp.hxx"
#include "lm_state.hxx"
#include "lm_notice.hxx"
#include "lm_crash.hxx"
#include "lm_practice.hxx"
#include "susamune/crash_report.h"
#include "susamune/lm_warp_transition.h"
#include "susamune/lm_room_id.h"

namespace {

constexpr u32 kScene = 0x804A0C20u;
constexpr u32 kRequestedMap = 0x804A0C24u;
constexpr u32 kExit = 0x804A0C28u;
constexpr u32 kMap = 0x804A0C48u;
constexpr u32 kMainMode = 0x80398A40u;
constexpr u32 kPendingScene = 0x80398A44u;
constexpr u32 kPauseRequest = 0x804A0C08u;
constexpr u32 kQueueScene = 0x8000B20Cu;
constexpr u32 kMission = 0x804A17C8u;
constexpr u32 kGameHeap = 0x804A0B98u;
constexpr u32 kAppearancePoint = 0x804A12A4u;
constexpr u32 kEventCursor = 0x803C7CACu;
constexpr u32 kSaveRequest = 0x804A0C44u;
constexpr u32 kSaveBusy = 0x804A0C6Cu;
constexpr u32 kGetPlayer = 0x800E7E7Cu;
constexpr u32 kHeapAlloc = 0x801C8EA4u;
constexpr u32 kRetailAppearanceSetup = 0x800E3A1Cu;
constexpr u32 kGetFlag = 0x80065320u;
constexpr u32 kSetFlag = 0x80065358u;
constexpr u32 kClearFlag = 0x80065398u;
constexpr u32 kEventPlayCounts = 0x803C2E30u;

constexpr u32 kShadowPoint = 0xF0u;
constexpr u32 kJMapHeaderSize = 0x124u;
constexpr u32 kJMapRowSize = 0xB8u;
constexpr u32 kJMapRowCount = 131u;
constexpr u32 kJMapFieldCount = 23u;
constexpr u32 kTemplateRow = 39u;
constexpr u32 kTimeoutFrames = 1800u;
constexpr u32 kSettleFrames = 12u;
constexpr u32 kNoRoom = 0xFFFFFFFFu;

struct Point {
    u16 id;
    u16 room;
    f32 x, y, z, yaw;
};

constexpr Point kPoints[] = {
#include "lm_warp_points.inc"
};

struct Destination {
    const char *name;
    u8 map;
    u8 point;
    u8 alternate;
    u8 room;
};

// Room IDs are the game's IDs. Runners can refine these labels without
// changing the spawn data or the persistent destination ordering.
constexpr Destination kDestinations[] = {
    {"Foyer", 2u, 0u, 0u, 2u},
    {"Parlor", 2u, 10u, 76u, 35u},
    {"Anteroom", 2u, 11u, 77u, 39u},
    {"Wardrobe", 2u, 12u, 78u, 38u},
    {"2F Balcony", 2u, 58u, 58u, 37u},
    {"Study", 2u, 13u, 79u, 34u},
    {"Master Bedroom", 2u, 14u, 80u, 33u},
    {"Nursery", 2u, 15u, 81u, 24u},
    {"1F Bathroom", 2u, 16u, 16u, 20u},
    {"Ballroom", 2u, 17u, 122u, 10u},
    {"Storage", 2u, 18u, 83u, 14u},
    {"1F Washroom", 2u, 19u, 19u, 17u},
    {"Fortune-teller", 2u, 20u, 85u, 3u},
    {"Mirror Room", 2u, 21u, 86u, 4u},
    {"Laundry Room", 2u, 22u, 87u, 5u},
    {"Butler's Room", 2u, 23u, 88u, 0u},
    {"Hidden Room", 2u, 24u, 89u, 1u},
    {"Conservatory", 2u, 25u, 90u, 21u},
    {"Dining Room", 2u, 26u, 91u, 9u},
    {"Kitchen", 2u, 27u, 92u, 8u},
    {"Boneyard", 2u, 28u, 28u, 11u},
    {"Graveyard", 2u, 29u, 29u, 16u},
    {"Courtyard", 2u, 30u, 30u, 23u},
    {"Bottom of Well", 2u, 31u, 31u, 69u},
    {"Rec Room", 2u, 32u, 97u, 22u},
    {"Tea Room", 2u, 33u, 98u, 47u},
    {"Astral Hall", 2u, 43u, 109u, 40u},
    {"Observatory", 2u, 34u, 34u, 41u},
    {"2F Bathroom", 2u, 35u, 35u, 45u},
    {"2F Washroom", 2u, 36u, 36u, 42u},
    {"Nana's Room", 2u, 37u, 102u, 46u},
    {"Twins' Room", 2u, 38u, 103u, 25u},
    {"Billiards Room", 2u, 39u, 104u, 12u},
    {"Projection Room", 2u, 40u, 106u, 13u},
    {"Safari Room", 2u, 41u, 107u, 52u},
    {"3F Balcony", 2u, 42u, 42u, 59u},
    {"Telephone Room", 2u, 44u, 110u, 50u},
    {"Breaker Room", 2u, 45u, 111u, 67u},
    {"Cellar", 2u, 46u, 112u, 63u},
    {"Clockwork Room", 2u, 47u, 113u, 56u},
    {"Roof Lift", 2u, 123u, 123u, 56u},
    {"Armory", 2u, 48u, 114u, 48u},
    {"Ceramics Studio", 2u, 52u, 118u, 55u},
    {"Sealed Room", 2u, 7u, 7u, 36u},
    {"Sitting Room", 2u, 53u, 119u, 27u},
    {"Guest Room", 2u, 54u, 120u, 28u},
    {"Pipe Room", 2u, 49u, 115u, 66u},
    {"Cold Storage", 2u, 50u, 116u, 61u},
    {"Artist's Studio", 2u, 51u, 117u, 57u},
    {"Secret Altar", 2u, 55u, 55u, 70u},
    {"2F Foyer Hall", 2u, 64u, 64u, 30u},
    {"1F Foyer Hall", 2u, 65u, 65u, 6u},
    {"Laundry Hall", 2u, 66u, 66u, 6u},
    {"Ballroom Hall", 2u, 67u, 67u, 7u},
    {"Rec Room Hall", 2u, 68u, 68u, 53u},
    {"Tea Room Hall", 2u, 69u, 69u, 19u},
    {"Nana's Hall", 2u, 70u, 70u, 43u},
    {"Safari Hall", 2u, 71u, 71u, 44u},
    {"Telephone Hall", 2u, 72u, 72u, 51u},
    {"Cellar Hall", 2u, 73u, 73u, 49u},
    {"Bathroom Hall", 2u, 75u, 75u, 62u},
    {"Roof", 2u, 3u, 3u, 60u},
    {"Nursery Chest Clip", 2u, 56u, 56u, 24u},
    {"Basement Walk", 2u, 57u, 57u, 6u},
    {"Chauncey", 10u, 0u, 0u, 0xFFu},
    {"Bogmire", 13u, 0u, 0u, 0xFFu},
    {"Boolossus", 11u, 0u, 0u, 0xFFu},
    {"King Boo", 9u, 0u, 0u, 0xFFu},
};

enum class Phase : u8 { Idle, Armed, Queued, Loading, Settling };
Phase sPhase;
u32 sDestination;
u32 sPoint;
u32 sAge;
u32 sStable;
u32 sRoom = kNoRoom;
bool sPrepared;
bool sAllocationFailed;
bool sStallReported;
bool sRoomReload;
bool sClearReload;
const char *sStatus = "READY";

using WordFn = void (*)(u32);
using GetWordFn = u32 (*)(u32);
using PlayerFn = void *(*)(u32);
using AllocFn = void *(*)(u32, s32, void *);
using SetupFn = void (*)(void *, void *);

u32 read(u32 address) { return *reinterpret_cast<volatile const u32 *>(address); }
void write(u32 address, u32 value) { *reinterpret_cast<volatile u32 *>(address) = value; }

bool valid(u32 address, u32 size) {
    return (address & 3u) == 0u && address >= 0x80000000u &&
           size <= 0x01800000u && address <= 0x81800000u - size;
}

bool playableMap(u32 value) {
    return value == 2u || value == 9u || value == 10u || value == 11u || value == 13u;
}

bool stableMission() {
    return read(kScene) == 2u && read(kMainMode) == 2u &&
           read(kPendingScene) == 2u && read(kExit) == 0u &&
           playableMap(read(kMap)) && valid(read(kMission), 0x24u) &&
           read(kSaveRequest) == 0u && read(kSaveBusy) == 0u;
}

const Point *findPoint(u32 id) {
    for (u32 i = 0u; i < sizeof(kPoints) / sizeof(kPoints[0]); ++i) {
        if (kPoints[i].id == id) return &kPoints[i];
    }
    return nullptr;
}

void trace(u32 phase, u32 arg0 = 0u, u32 arg1 = 0u) {
    LMCrash::phase(SUSAMUNE_PHASE_ACTION_WARP, phase, arg0, arg1);
    LMCrash::note(0x120u, phase, arg0);
}

void fail(const char *message, u32 reason, u32 detail = 0u) {
    LMNotice::show(reason == 2u || reason == 3u || reason == 5u ||
        reason == 6u || reason == 11u || reason == 13u ? LM_POPUP_BUSY : LM_POPUP_REJECTED);
    trace(SUSAMUNE_LM_WARP_REJECT, reason, detail);
    sPhase = Phase::Idle;
    sStatus = message;
    sStable = 0u;
}

bool hasField(u32 data, u32 hash, u32 mask, u32 offset, u32 type) {
    for (u32 i = 0u; i < kJMapFieldCount; ++i) {
        const u32 entry = data + 0x10u + 12u * i;
        if (read(entry) == hash) {
            return read(entry + 4u) == mask &&
                   read(entry + 8u) == (offset << 16u | type);
        }
    }
    return false;
}

void *makeShadow(void *table, const Point &point) {
    sAllocationFailed = false;
    const u32 source = reinterpret_cast<u32>(table);
    if (!valid(source, 8u)) return nullptr;
    const u32 data = read(source + 4u);
    if (!valid(data, kJMapHeaderSize + kJMapRowCount * kJMapRowSize) ||
        read(data) != kJMapRowCount || read(data + 4u) != kJMapFieldCount ||
        read(data + 8u) != kJMapHeaderSize || read(data + 12u) != kJMapRowSize) {
        return nullptr;
    }
    if (!hasField(data, 0x006175C6u, 0u, 0x18u, 1u) ||
        !hasField(data, 0x017BEFD9u, 0u, 0x00u, 2u) ||
        !hasField(data, 0x017BEFDAu, 0u, 0x04u, 2u) ||
        !hasField(data, 0x017BEFDBu, 0u, 0x08u, 2u) ||
        !hasField(data, 0x017A0565u, 0u, 0x10u, 2u) ||
        !hasField(data, 0x00C9952Du, 0xFFFFFFFFu, 0x98u, 0u) ||
        !hasField(data, 0x00422CBCu, 0xFFFFFFFFu, 0xACu, 0u)) {
        return nullptr;
    }
    const u32 row = data + kJMapHeaderSize + kTemplateRow * kJMapRowSize;
    if (read(row + 0x18u) != 0x6C756967u ||
        (*reinterpret_cast<const u16 *>(row + 0x1Cu)) != 0x6500u ||
        read(row + 0xACu) != 0u) {
        return nullptr;
    }
    const u32 heap = read(kGameHeap);
    if (!valid(heap, 0x38u)) return nullptr;
    const u32 start = read(heap + 0x30u);
    const u32 end = read(heap + 0x34u);
    if (!valid(start, 4u) || end > 0x81800000u || start >= end) return nullptr;

    // The retained ToolDataRef must rewind with the scene, so the tiny shadow
    // belongs to the captured game heap and dies with the retail map teardown.
    constexpr u32 bytes = 8u + kJMapHeaderSize + kJMapRowSize;
    const u32 shadow = reinterpret_cast<u32>(
        reinterpret_cast<AllocFn>(kHeapAlloc)(bytes, 32, reinterpret_cast<void *>(heap)));
    if (!shadow) {
        sAllocationFailed = true;
        return nullptr;
    }
    if (!valid(shadow, bytes) || shadow < start || shadow > end - bytes) return nullptr;
    write(shadow, read(source));
    write(shadow + 4u, shadow + 8u);
    for (u32 i = 0u; i < kJMapHeaderSize; i += 4u) write(shadow + 8u + i, read(data + i));
    write(shadow + 8u, 1u);
    const u32 target = shadow + 8u + kJMapHeaderSize;
    for (u32 i = 0u; i < kJMapRowSize; i += 4u) write(target + i, read(row + i));
    *reinterpret_cast<f32 *>(target) = point.x;
    *reinterpret_cast<f32 *>(target + 4u) = point.y;
    *reinterpret_cast<f32 *>(target + 8u) = point.z;
    *reinterpret_cast<f32 *>(target + 0x10u) = point.yaw;
    write(target + 0x98u, point.room);
    write(target + 0xACu, kShadowPoint);
    return reinterpret_cast<void *>(shadow);
}

}  // namespace

extern "C" void lmWarpPrepareAppearance(void *manager, void *table) {
    void *selected = table;
    if ((sPhase == Phase::Queued || sPhase == Phase::Loading) &&
        read(kMap) == kDestinations[sDestination].map) {
        sPhase = Phase::Loading;
        sAge = 0u;
        trace(SUSAMUNE_LM_WARP_APPEARANCE,
              reinterpret_cast<u32>(manager), reinterpret_cast<u32>(table));
        if (read(kMap) == 2u) {
            const Point *point = findPoint(sPoint);
            selected = point ? makeShadow(table, *point) : nullptr;
            if (!selected) {
                // Always leave the retail loader with a valid appearance row.
                write(kAppearancePoint, 0u);
                selected = table;
                fail(sAllocationFailed ? "WARP: NO MEMORY" : "WARP: BAD SPAWN TABLE",
                     sAllocationFailed ? 7u : 8u, sDestination);
            }
        }
        if (sPhase == Phase::Loading) sPrepared = true;
    }
    reinterpret_cast<SetupFn>(kRetailAppearanceSetup)(manager, selected);
    if (sPhase == Phase::Loading && sPrepared) {
        trace(SUSAMUNE_LM_WARP_PREPARED,
              reinterpret_cast<u32>(manager), reinterpret_cast<u32>(selected));
    }
}

namespace LMWarp {

u32 count() { return sizeof(kDestinations) / sizeof(kDestinations[0]); }
const char *name(u32 index) { return index < count() ? kDestinations[index].name : "INVALID"; }
u32 map(u32 index) { return index < count() ? kDestinations[index].map : kNoRoom; }
u32 room(u32 index) {
    return index < count() && kDestinations[index].room != 0xFFu
               ? kDestinations[index].room : kNoRoom;
}
bool hasBooSafePoint(u32 index) {
    return index < count() && kDestinations[index].point != kDestinations[index].alternate;
}
bool active() { return sPhase != Phase::Idle; }
const char *statusText() { return sStatus; }
u32 actualRoom() { return sRoom; }

bool request(u32 index, bool booSafe) {
    trace(SUSAMUNE_LM_WARP_REQUEST, index, read(kMap));
    if (active()) { LMNotice::show(LM_POPUP_BUSY); return false; }
    if (index >= count()) { fail("WARP: INVALID DESTINATION", 1u, index); return false; }
    if (!stableMission() || !LMState::readyForActionNow()) {
        fail("WARP: WAIT FOR STREAMING", 2u, LMState::gateValue()); return false;
    }
    if (read(kEventCursor) != 0u) { fail("WARP: WAIT FOR EVENT", 3u, read(kEventCursor)); return false; }
    const Destination &destination = kDestinations[index];
    if (destination.map == 11u && reinterpret_cast<GetWordFn>(kGetFlag)(45u) == 0u) {
        fail("WARP: NEED ICE MEDAL", 4u); return false;
    }
    sDestination = index;
    sRoomReload = false;
    sClearReload = false;
    sPoint = booSafe ? destination.alternate : destination.point;
    sAge = 0u;
    sStable = 0u;
    sRoom = kNoRoom;
    sPrepared = false;
    sStallReported = false;
    sStatus = "WARP: PREPARING";
    LMNotice::show(LM_POPUP_LOADING);
    sPhase = Phase::Armed;
    trace(SUSAMUNE_LM_WARP_ACCEPT, index, destination.map << 16u | sPoint);
    return true;
}

bool roomReloadAvailable(u32 roomId) {
    for (u32 i = 0u; i < count(); ++i) {
        if (kDestinations[i].map == 2u && kDestinations[i].room == roomId)
            return true;
    }
    return false;
}

bool requestRoomReload(bool clear, bool booSafe) {
    if (active()) { LMNotice::show(LM_POPUP_BUSY); return false; }
    if (!stableMission() || !LMState::readyForActionNow()) {
        fail("RELOAD: WAIT FOR GAME", 11u, LMState::gateValue()); return false;
    }
    const u32 currentMap = read(kMap);
    const u32 player = reinterpret_cast<u32>(reinterpret_cast<PlayerFn>(kGetPlayer)(0u));
    if (!valid(player, 0xB8u)) { LMNotice::show(LM_POPUP_REJECTED); return false; }
    const u32 roomId = static_cast<u32>(LmPlayerRoomId(read(player + 0xB4u)));
    for (u32 i = 0u; i < count(); ++i) {
        const Destination &destination = kDestinations[i];
        if (destination.map != currentMap ||
            (currentMap == 2u && destination.room != roomId)) continue;
        if (currentMap != 2u && clear) { LMNotice::show(LM_POPUP_REJECTED); return false; }
        if (!request(i, booSafe)) return false;
        sRoomReload = currentMap == 2u;
        sClearReload = clear;
        sStatus = clear ? "CLEAR: PREPARING RELOAD" : "RESET: PREPARING RELOAD";
        return true;
    }
    fail("RELOAD: ROOM NOT SUPPORTED", 12u, roomId);
    return false;
}

void tick() {
    if (!active()) return;
    if (sAge <= kTimeoutFrames) ++sAge;
    const Destination &destination = kDestinations[sDestination];
    if (sPhase == Phase::Armed) {
        if (!stableMission() || !LMState::readyForActionNow() || read(kEventCursor) != 0u) {
            fail("WARP: GAME BECAME BUSY", 5u, LMState::gateValue()); return;
        }
        const LmWarpFlagPlan flags =
            LmWarpFlagsForDestination(destination.map, destination.point);
        if (sRoomReload && !LMPractice::prepareRoomReload(destination.room, sClearReload)) {
            fail("RELOAD: ROOM BECAME BUSY", 13u, destination.room); return;
        }
        sPhase = Phase::Queued;
        if (!LmWarpPublishAndQueue(
                destination.map, destination.map == 2u ? kShadowPoint : 0u,
                &flags, reinterpret_cast<volatile u32 *>(kRequestedMap),
                reinterpret_cast<volatile u32 *>(kAppearancePoint),
                reinterpret_cast<volatile u32 *>(kPauseRequest),
                reinterpret_cast<LmWarpWordFn>(kGetFlag),
                reinterpret_cast<LmWarpFlagFn>(kSetFlag),
                reinterpret_cast<LmWarpFlagFn>(kClearFlag),
                reinterpret_cast<LmWarpWordFn>(kQueueScene),
                reinterpret_cast<volatile u8 *>(kEventPlayCounts))) {
            fail("WARP: SCENE QUEUE BUSY", 6u, sDestination);
            return;
        }
        // The nonblocking sender has accepted the transition. Its consumer
        // runs later on this same game thread; a failed queue changes no room.
        if (sRoomReload) LMPractice::commitRoomReload(destination.room, sClearReload);
        sStatus = "WARP: QUEUED";
        trace(SUSAMUNE_LM_WARP_DISPATCH, sDestination,
              destination.map << 16u | sPoint);
        return;
    }
    if (sPhase == Phase::Queued && read(kExit) != 0u) {
        sPhase = Phase::Loading;
        sAge = 0u;
        sStatus = "WARP: LOADING";
        LMNotice::show(LM_POPUP_LOADING);
    }
    if (sAge > kTimeoutFrames && !sStallReported) {
        // A queued native request may still be consumed. Do not abandon its
        // appearance point or overwrite somebody else's queue to time out.
        sStallReported = true;
        sStatus = "WARP: LOAD STALLED";
        LMNotice::show(LM_POPUP_BUSY);
        trace(SUSAMUNE_LM_WARP_REJECT, 9u, static_cast<u32>(sPhase));
    }
    if (!sPrepared || !stableMission() || read(kMap) != destination.map ||
        !LMState::readyForAction()) {
        sStable = 0u;
        return;
    }
    const u32 player = reinterpret_cast<u32>(reinterpret_cast<PlayerFn>(kGetPlayer)(0u));
    if (!valid(player, 0xB8u)) { sStable = 0u; return; }
    const u32 packedRoom = read(player + 0xB4u);
    sRoom = static_cast<u32>(LmPlayerRoomId(packedRoom));
    if (sRoom == kNoRoom) { sStable = 0u; return; }
    if (sPhase != Phase::Settling) {
        LMNotice::show(LM_POPUP_LOADING);
        trace(SUSAMUNE_LM_WARP_SETTLING, sDestination, packedRoom);
    }
    sPhase = Phase::Settling;
    sStatus = "WARP: SETTLING";
    if (++sStable < kSettleFrames) return;
    if (destination.map == 2u && sRoom != destination.room) {
        fail("WARP: ROOM MISMATCH", 10u, packedRoom); return;
    }
    sPhase = Phase::Idle;
    sStatus = "WARP: ARRIVED";
    LMNotice::show(LM_POPUP_LOADED);
    trace(SUSAMUNE_LM_WARP_ARRIVED, sDestination, sRoom);
}

}  // namespace LMWarp

#endif
