#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_practice.hxx"

#include "Dolphin/PAD.h"
#include "Dolphin/types.h"
#include "lm_state.hxx"

namespace {

constexpr u32 kMem1Start = 0x80000000u;
constexpr u32 kMem1End = 0x81800000u;
constexpr u32 kPadStatusAddress = 0x80494778u;
constexpr u32 kPadReadAddress = 0x801E48FCu;
constexpr u32 kSceneValueAddress = 0x804A0C20u;
constexpr u32 kMapValueAddress = 0x804A0C48u;
constexpr u32 kMainLoopModeAddress = 0x80398A40u;
constexpr u32 kPendingSceneAddress = 0x80398A44u;
constexpr u32 kMainLoopExitAddress = 0x804A0C28u;
constexpr u32 kMissionModeAddress = 0x804A17C8u;
constexpr u32 kGameHeapAddress = 0x804A0B98u;
constexpr u32 kEventInterpreterAddress = 0x803C7CA0u;
constexpr u32 kMansionModeAddress = 0x804A0C40u;
constexpr u32 kSaveRequestAddress = 0x804A0C44u;
constexpr u32 kSaveModeAddress = 0x804A0C68u;
constexpr u32 kSaveBusyAddress = 0x804A0C6Cu;
constexpr u32 kRoomTableAddress = 0x804A0CF0u;
constexpr u32 kRoomManagerAddress = 0x804A0CF8u;
constexpr u32 kKeyTableCountAddress = 0x804A0D88u;

constexpr u32 kGetFlagAddress = 0x80065320u;
constexpr u32 kSetFlagAddress = 0x80065358u;
constexpr u32 kClearFlagAddress = 0x80065398u;
constexpr u32 kGetPlayerAddress = 0x800E7E7Cu;
constexpr u32 kSetPlayerHpAddress = 0x800B7BFCu;
constexpr u32 kDoorKeyStateAddress = 0x8001B030u;
constexpr u32 kBlackoutAddress = 0x80037498u;
constexpr u32 kRoomLightAddress = 0x800197D8u;
constexpr u32 kBgmStartAddress = 0x801884E8u;
constexpr u32 kBgmStopAddress = 0x801885E8u;
constexpr u32 kActorLookupAddress = 0x800E7F08u;
constexpr u32 kActorKillAddress = 0x800E7EDCu;
constexpr u32 kDirectPrintEraseAddress = 0x801D4294u;
constexpr u32 kDirectPrintDrawStringAddress = 0x801D49F8u;

constexpr u16 kDPadLeft = 0x0001u;
constexpr u16 kDPadRight = 0x0002u;
constexpr u16 kDPadDown = 0x0004u;
constexpr u16 kDPadUp = 0x0008u;
constexpr u16 kButtonR = 0x0020u;
constexpr u16 kButtonL = 0x0040u;
constexpr u16 kButtonA = 0x0100u;
constexpr u16 kButtonB = 0x0200u;

constexpr u16 kMenuTop = 29u;
constexpr u16 kMenuHeight = 92u;
constexpr u32 kPageCount = 4u;
constexpr u32 kConfirmFrames = 180u;
constexpr u32 kNoticeFrames = 180u;
constexpr u32 kRoomEntrySize = 0x14Cu;
constexpr u32 kRoomCountLimit = 72u;

using GetFlagFn = u32 (*)(u32);
using FlagFn = void (*)(u32);
using GetPlayerFn = void *(*)(u32);
using ActorLookupFn = void *(*)(const char *);
using PlayerHpFn = void (*)(void *, s32);
using TwoWordFn = void (*)(u32, u32);
using WordFn = void (*)(u32);
using BoolFn = void (*)(bool);
using VoidFn = void (*)();
using DirectPrintEraseFn = void (*)(void *, u16, u16, u16, u16);
using DirectPrintDrawStringFn = void (*)(void *, u16, u16, const char *, ...);
using PadReadFn = u32 (*)(PADStatus *);

enum class Page : u32 {
    Settings,
    Doors,
    Plant,
    Audio,
};

struct PlantPreset {
    const char *name;
    u8 bits;
};

struct RoomClearRecipe {
    u8 room;
    const char *const *actors;
    u8 actorCount;
    const u8 *flags;
    u8 flagCount;
};

constexpr PlantPreset kArea2Plants[] = {
    {"SEED", 0u},
    {"SPROUT", 2u},
};
constexpr PlantPreset kArea3Plants[] = {
    {"FLOWER", 3u},
    {"BUD", 2u},
    {"SEED", 0u},
    {"SPROUT", 1u},
};
constexpr PlantPreset kArea4Plants[] = {
    {"OPENED", 7u},
    {"CLOSED", 3u},
    {"DEAD", 2u},
    {"SEED", 0u},
    {"SPROUT", 4u},
};

constexpr const char *kBgmNames[] = {
    "Key Ghost",          "E.Gadd Debut",      "SMB1 Water",
    "SMB3 Land",          "Unused Piano",      "Parlor Paintings",
    "Telescope",          "Dark Room",         "Training",
    "Chauncey Door",      "Foyer Debut",       "Toad",
    "Lights On",          "Training Interrupt", "Training Results",
    "Chauncey Intro",     "Chauncey Battle",   "Tombstone Glow",
    "Bogmire Intro",      "Bogmire Battle",    "Boolossus Intro",
    "SMB1 Jingle",        "Boolossus Battle",  "Melody Battle",
    "dummy",              "Portrification",    "Boss Clear",
    "Saving Mario",       "Silence",           "End Game",
    "Portrait Ghost",     "Ghost Minigame",    "Ghost Battle",
    "Chauncey Plays",     "GBH Ringtone",      "GBH",
    "Sliding Wall",       "Water Shutoff",     "Candle Pentagram",
    "Mario Painting",     "Well Cutscene",     "Drum Beat",
    "Well Cutscene 2",    "Sue Pea Debut",     "Boo Warping",
    "Releasing Boos",     "Boo Theme",         "3F Boo Check",
    "B1 Boo Check",       "King Boo",           "Bowser Intro",
    "Bowser Battle",      "Bowser Inhales",    "dummy",
};
static_assert(sizeof(kBgmNames) / sizeof(kBgmNames[0]) == 54u,
              "GaddWarp BGM table must retain every stock ID");

constexpr u8 kAllDoorIds[] = {
    72u, 69u, 71u, 68u, 65u, 63u, 62u, 59u, 56u, 53u,
    51u, 38u, 34u, 33u, 31u, 29u, 28u, 27u, 25u, 20u,
    21u, 42u, 74u, 17u, 16u, 15u, 7u,  14u, 4u,  3u,
};

constexpr const char *kClearRoom0[] = {"demo_situji"};
constexpr const char *kClearRoom3[] = {"dm_uranai"};
constexpr const char *kClearRoom9[] = {
    "iyapoo", "demo_eater", "demo_eater2", "dm_waiter0", "dm_waiter1",
};
constexpr const char *kClearRoom11[] = {"demo_dog"};
constexpr const char *kClearRoom12[] = {"demo_hustler", "iyapoo"};
constexpr const char *kClearRoom21[] = {"demo_piano", "iyapoo"};
constexpr const char *kClearRoom33[] = {"demo_mother"};
constexpr const char *kClearRoom38[] = {"demo_denwa", "iyapoo"};
constexpr const char *kClearRoom40[] = {
    "tony_montana", "tony_montana", "tony_montana",
};
constexpr const char *kClearRoom45[] = {"demo_fat"};
constexpr const char *kClearRoom46[] = {
    "demo_gm", "iyapoo", "kdm1", "kdm2", "kdm3",
};
constexpr const char *kClearRoom56[] = {"dm_doll1", "dm_doll2", "dm_doll3"};
constexpr const char *kClearGeneric235[] = {
    "tony_montana", "tony_montana", "tony_montana", "tony_montana",
    "tony_montana", "tony_montana", "iyapoo",
};
constexpr u8 kClearGeneric235Rooms[] = {
    1u,  4u,  5u,  6u,  7u,  8u,  13u, 14u, 15u, 18u,
    20u, 23u, 26u, 27u, 29u, 31u, 36u, 39u, 42u, 43u,
    44u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 58u,
    60u, 62u, 63u, 64u, 66u, 67u, 68u, 69u, 71u,
};
constexpr u8 kClearGeneric236Rooms[] = {2u, 17u, 19u, 30u, 32u, 37u, 65u};
constexpr u8 kClearFlags35[] = {8u, 14u};
constexpr u8 kClearFlags40[] = {83u, 6u};
constexpr RoomClearRecipe kClearGeneric235Recipe = {
    0xFFu, kClearGeneric235, 7u, nullptr, 0u,
};
constexpr RoomClearRecipe kClearGeneric236Recipe = {
    0xFFu, nullptr, 0u, nullptr, 0u,
};
constexpr RoomClearRecipe kRoomClearRecipes[] = {
    {0u, kClearRoom0, 1u, nullptr, 0u},
    {3u, kClearRoom3, 1u, nullptr, 0u},
    {9u, kClearRoom9, 5u, nullptr, 0u},
    {11u, kClearRoom11, 1u, nullptr, 0u},
    {12u, kClearRoom12, 2u, nullptr, 0u},
    {21u, kClearRoom21, 2u, nullptr, 0u},
    {33u, kClearRoom33, 1u, nullptr, 0u},
    {35u, nullptr, 0u, kClearFlags35, 2u},
    {38u, kClearRoom38, 2u, nullptr, 0u},
    {40u, kClearRoom40, 3u, kClearFlags40, 2u},
    {45u, kClearRoom45, 1u, nullptr, 0u},
    {46u, kClearRoom46, 5u, nullptr, 0u},
    {56u, kClearRoom56, 3u, nullptr, 0u},
};

bool sOpen;
bool sConsumeUntilRelease;
PADStatus sMenuPad;
u16 sPreviousButtons;
Page sPage;
u32 sSelection;
u32 sPlantChoice;
u32 sPlantArea;
u32 sBgmId;
u32 sConfirmAction;
u32 sConfirmTimer;
const char *sNotice;
u32 sNoticeTimer;

inline u32 readWord(u32 address) {
    return *reinterpret_cast<volatile u32 *>(address);
}

inline s32 readSignedWord(u32 address) {
    return *reinterpret_cast<volatile s32 *>(address);
}

inline u16 readHalf(u32 address) {
    return *reinterpret_cast<volatile u16 *>(address);
}

inline s16 readSignedHalf(u32 address) {
    return *reinterpret_cast<volatile s16 *>(address);
}

inline void writeWord(u32 address, u32 value) {
    *reinterpret_cast<volatile u32 *>(address) = value;
}

bool validMem1(u32 address, u32 size) {
    return size <= kMem1End - kMem1Start && address >= kMem1Start &&
           address <= kMem1End - size;
}

bool validGameHeap(u32 address, u32 size) {
    const u32 heap = readWord(kGameHeapAddress);
    if (!validMem1(heap, 0x38u)) return false;
    const u32 start = readWord(heap + 0x30u);
    const u32 end = readWord(heap + 0x34u);
    return start >= kMem1Start && start <= end && end <= kMem1End &&
           size <= end - start && address >= start && address <= end - size;
}

bool missionReady() {
    const u32 scene = readWord(kSceneValueAddress);
    const u32 mission = readWord(kMissionModeAddress);
    return LMState::readyForAction() && scene == 2u &&
           readWord(kMapValueAddress) == 2u &&
           readWord(kMainLoopModeAddress) == 2u &&
           readWord(kPendingSceneAddress) == scene &&
           readWord(kMainLoopExitAddress) == 0u && validMem1(mission, 0x24u);
}

bool noEventRunning() {
    return readWord(kEventInterpreterAddress + 0x0Cu) == 0u;
}

u32 flag(u32 id) {
    return reinterpret_cast<GetFlagFn>(kGetFlagAddress)(id) != 0u;
}

void setFlag(u32 id, bool on) {
    reinterpret_cast<FlagFn>(on ? kSetFlagAddress : kClearFlagAddress)(id);
}

void showNotice(const char *text) {
    sNotice = text;
    sNoticeTimer = kNoticeFrames;
}

bool beginAction() {
    if (!missionReady()) {
        showNotice("WAIT FOR GAME/STREAMING");
        return false;
    }
    if (!noEventRunning()) {
        showNotice("WAIT FOR CUTSCENE/EVENT");
        return false;
    }
    return true;
}

u32 playerAddress() {
    if (!missionReady()) return 0u;
    const u32 player = reinterpret_cast<u32>(
        reinterpret_cast<GetPlayerFn>(kGetPlayerAddress)(0u));
    return validMem1(player, 0x1000u) ? player : 0u;
}

u32 booCount() {
    const u32 player = playerAddress();
    return player ? readWord(player + 0x880u) : 0xFFFFFFFFu;
}

bool roomActionReady() {
    const u32 player = playerAddress();
    const u32 roomTable = readWord(kRoomTableAddress);
    const u32 roomManager = readWord(kRoomManagerAddress);
    if (!player || !validMem1(roomManager, 0x10u) || !noEventRunning()) {
        return false;
    }
    const u32 loadedRoom = readWord(roomManager + 0x0Cu) >> 24;
    if (loadedRoom >= kRoomCountLimit ||
        !validMem1(roomTable, (loadedRoom + 1u) * kRoomEntrySize)) {
        return false;
    }
    const s32 playerRoom = readSignedWord(player + 0xB4u);
    return playerRoom >= 0 && static_cast<u32>(playerRoom) == loadedRoom;
}

u32 currentRoomId() {
    if (!roomActionReady()) return 0xFFFFFFFFu;
    const u32 roomManager = readWord(kRoomManagerAddress);
    return readWord(roomManager + 0x0Cu) >> 24;
}

const RoomClearRecipe *roomClearRecipe(u32 room) {
    for (u32 i = 0u;
         i < sizeof(kClearGeneric235Rooms) / sizeof(kClearGeneric235Rooms[0]);
         ++i) {
        if (kClearGeneric235Rooms[i] == room) return &kClearGeneric235Recipe;
    }
    for (u32 i = 0u;
         i < sizeof(kClearGeneric236Rooms) / sizeof(kClearGeneric236Rooms[0]);
         ++i) {
        if (kClearGeneric236Rooms[i] == room) return &kClearGeneric236Recipe;
    }
    for (u32 i = 0u;
         i < sizeof(kRoomClearRecipes) / sizeof(kRoomClearRecipes[0]); ++i) {
        if (kRoomClearRecipes[i].room == room) return &kRoomClearRecipes[i];
    }
    return nullptr;
}

bool actorRegistryReady() {
    const u32 mission = readWord(kMissionModeAddress);
    if (!validMem1(mission, 0x24u)) return false;
    return validGameHeap(readWord(mission + 8u), 0xE48u);
}

void *lookupActor(const char *name) {
    return reinterpret_cast<ActorLookupFn>(kActorLookupAddress)(name);
}

bool actorLookupSafe(const char *name) {
    const u32 actor = reinterpret_cast<u32>(lookupActor(name));
    return actor == 0u || validGameHeap(actor, 0x3Cu);
}

void killActor(const char *name) {
    const u32 actor = reinterpret_cast<u32>(lookupActor(name));
    if (!actor || !validGameHeap(actor, 0x3Cu)) return;
    reinterpret_cast<WordFn>(kActorKillAddress)(readWord(actor + 0x38u));
}

u32 plantArea() {
    if (flag(82u)) return 4u;
    if (flag(68u)) return 3u;
    if (flag(39u)) return 2u;
    return 1u;
}

const PlantPreset *plantPresets(u32 area, u32 *count) {
    if (area == 2u) {
        *count = sizeof(kArea2Plants) / sizeof(kArea2Plants[0]);
        return kArea2Plants;
    }
    if (area == 3u) {
        *count = sizeof(kArea3Plants) / sizeof(kArea3Plants[0]);
        return kArea3Plants;
    }
    if (area == 4u) {
        *count = sizeof(kArea4Plants) / sizeof(kArea4Plants[0]);
        return kArea4Plants;
    }
    *count = 0u;
    return nullptr;
}

void applyPlant(u8 bits) {
    setFlag(48u, (bits & 4u) != 0u);
    setFlag(78u, (bits & 2u) != 0u);
    setFlag(79u, (bits & 1u) != 0u);
}

bool booRequirementsDisabled(u32 count) {
    if (count <= 4u) return flag(2u) && flag(77u) && flag(56u);
    if (count <= 19u) return flag(2u) && flag(77u);
    if (count <= 39u) return flag(77u);
    return false;
}

void setBooRequirements(u32 count, bool disabled) {
    if (count <= 4u) setFlag(56u, disabled);
    if (count <= 19u) setFlag(2u, disabled);
    if (count <= 39u) setFlag(77u, disabled);
}

bool confirm(u32 action) {
    if (sConfirmAction == action && sConfirmTimer != 0u) {
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
        return true;
    }
    sConfirmAction = action;
    sConfirmTimer = kConfirmFrames;
    showNotice("PRESS A AGAIN TO CONFIRM");
    return false;
}

void toggleSettings(u32 row, s32 direction) {
    if (!beginAction()) return;
    if (row == 0u) {
        const bool hidden = direction < 0 ? false :
                            direction > 0 ? true :
                            readWord(kMansionModeAddress) == 0u;
        setFlag(35u, !hidden);
        writeWord(kMansionModeAddress, hidden ? 1u : 0u);
        showNotice(hidden ? "HIDDEN MANSION SET" : "NORMAL MANSION SET");
    } else if (row == 1u) {
        const bool current = flag(22u) && flag(73u) && flag(75u);
        const bool on = direction < 0 ? false : direction > 0 ? true : !current;
        setFlag(22u, on);
        setFlag(73u, on);
        setFlag(75u, on);
        showNotice(on ? "BOO SPAWNING ON" : "BOO SPAWNING OFF");
    } else if (row == 2u) {
        const u32 count = booCount();
        if (count == 0xFFFFFFFFu) {
            showNotice("PLAYER NOT READY");
        } else if (count >= 40u) {
            showNotice("40+ BOOS: NO GATE PRESET");
        } else {
            const bool current = booRequirementsDisabled(count);
            const bool off = direction < 0 ? false :
                             direction > 0 ? true : !current;
            setBooRequirements(count, off);
            showNotice(off ? "BOO REQUIREMENTS OFF" :
                             "BOO REQUIREMENTS ON");
        }
    } else if (row == 3u) {
        if (!roomActionReady()) {
            showNotice("WAIT: ROOM/EVENT BUSY");
            return;
        }
        const bool current = flag(61u) != 0u;
        const bool on = direction < 0 ? false : direction > 0 ? true : !current;
        reinterpret_cast<BoolFn>(kBlackoutAddress)(on);
        showNotice(on ? "BLACKOUT ON" : "BLACKOUT OFF");
    } else if (row == 4u) {
        const u32 player = playerAddress();
        if (!player) {
            showNotice("PLAYER NOT READY");
            return;
        }
        const s32 hp = readSignedHalf(player + 0xFCu) <= 1 ? 100 : 1;
        reinterpret_cast<PlayerHpFn>(kSetPlayerHpAddress)(
            reinterpret_cast<void *>(player), hp);
        showNotice(hp == 100u ? "HP SET TO 100" : "HP SET TO 1");
    } else if (row == 5u) {
        if (readWord(kSaveRequestAddress) != 0u ||
            readWord(kSaveBusyAddress) != 0u) {
            showNotice("SAVE SYSTEM BUSY");
            return;
        }
        if (!confirm(1u)) return;
        // Prepare the mode, then publish the retail HSAVE request. Publishing
        // the request last keeps the asynchronous main loop from seeing a
        // half-prepared command. This is deliberately never automatic.
        writeWord(kSaveModeAddress, 1u);
        writeWord(kSaveRequestAddress, 5u);
        sOpen = false;
        sConsumeUntilRelease = true;
        showNotice("SAVE REQUESTED");
    }
}

void toggleDoors(u32 row, s32 direction) {
    if (!beginAction()) return;
    if (row < 3u) {
        const u32 ids[] = {54u, 83u, 70u};
        const bool currentOn = flag(ids[row]) == 0u;
        const bool on = direction < 0 ? false :
                        direction > 0 ? true : !currentOn;
        setFlag(ids[row], !on);
        showNotice(on ? "DOOR OPTION ON" : "DOOR OPTION OFF");
        return;
    }
    if (!confirm(2u)) return;
    if (readHalf(kKeyTableCountAddress) == 0u) {
        showNotice("KEY TABLE NOT READY");
        return;
    }
    for (u32 i = 0u; i < sizeof(kAllDoorIds) / sizeof(kAllDoorIds[0]); ++i) {
        reinterpret_cast<TwoWordFn>(kDoorKeyStateAddress)(kAllDoorIds[i], 0u);
    }
    setFlag(17u, true);
    showNotice("ALL DOORS UNLOCKED");
}

void clearCurrentRoom() {
    if (!beginAction()) return;
    const u32 room = currentRoomId();
    const RoomClearRecipe *recipe = roomClearRecipe(room);
    if (room == 0xFFFFFFFFu) {
        showNotice("WAIT: ROOM/EVENT BUSY");
        return;
    }
    if (!recipe) {
        showNotice("THIS ROOM CLEAR NEEDS RELOAD");
        return;
    }
    if (flag(61u)) {
        showNotice("TURN BLACKOUT OFF FIRST");
        return;
    }
    if (!actorRegistryReady()) {
        showNotice("ACTOR REGISTRY NOT READY");
        return;
    }
    if (!confirm(3u)) return;
    for (u32 i = 0u; i < recipe->actorCount; ++i) {
        if (!actorLookupSafe(recipe->actors[i])) {
            showNotice("ACTOR TABLE CHANGED - ABORTED");
            return;
        }
    }
    for (u32 i = 0u; i < recipe->actorCount; ++i) {
        killActor(recipe->actors[i]);
    }
    for (u32 i = 0u; i < recipe->flagCount; ++i) {
        setFlag(recipe->flags[i], true);
    }
    reinterpret_cast<VoidFn>(kRoomLightAddress)();
    sOpen = false;
    sConsumeUntilRelease = true;
    showNotice("CURRENT ROOM CLEARED");
}

u32 rowCount() {
    switch (sPage) {
    case Page::Settings:
        return 6u;
    case Page::Doors:
        return 4u;
    case Page::Plant:
        return 2u;
    case Page::Audio:
        return 2u;
    }
    return 1u;
}

bool edge(u16 buttons, u16 mask) {
    return (buttons & mask) != 0u && (sPreviousButtons & mask) == 0u;
}

u16 inputButtons() {
    if (sOpen || sConsumeUntilRelease) return sMenuPad.mButton;
    return reinterpret_cast<volatile PADStatus *>(kPadStatusAddress)->mButton;
}

void handleAction(s32 direction) {
    if (sPage == Page::Settings) {
        if (sSelection == 5u && direction != 0) return;
        toggleSettings(sSelection, direction);
    } else if (sPage == Page::Doors) {
        if (sSelection == 3u && direction != 0) return;
        toggleDoors(sSelection, direction);
    } else if (sPage == Page::Plant) {
        if (sSelection == 0u) {
            u32 count;
            plantPresets(sPlantArea, &count);
            if (count == 0u) {
                showNotice("PLANT UNAVAILABLE IN AREA 1");
            } else if (direction < 0) {
                sPlantChoice = (sPlantChoice + count - 1u) % count;
            } else if (direction > 0) {
                sPlantChoice = (sPlantChoice + 1u) % count;
            } else if (beginAction()) {
                const PlantPreset *presets = plantPresets(sPlantArea, &count);
                applyPlant(presets[sPlantChoice].bits);
                showNotice("PLANT PRESET APPLIED");
            }
        } else if (direction == 0) {
            clearCurrentRoom();
        }
    } else if (sPage == Page::Audio) {
        if (sSelection == 0u) {
            if (direction < 0) {
                sBgmId = sBgmId == 0u ? 52u : sBgmId - 1u;
                if (sBgmId == 24u) sBgmId = 23u;
            } else if (direction > 0) {
                sBgmId = sBgmId >= 52u ? 0u : sBgmId + 1u;
                if (sBgmId == 24u) sBgmId = 25u;
            } else if (beginAction()) {
                reinterpret_cast<TwoWordFn>(kBgmStartAddress)(sBgmId, 0u);
                showNotice("BGM STARTED");
            }
        } else if (direction == 0 && beginAction()) {
            reinterpret_cast<WordFn>(kBgmStopAddress)(0u);
            showNotice("BGM STOPPED");
        }
    }
}

const char *onOff(bool on) {
    return on ? "ON" : "OFF";
}

const char *selectionMark(u32 row) {
    return sSelection == row ? ">" : " ";
}

}  // namespace

namespace LMPractice {

void filterPadRead(PADStatus *statuses) {
    if (!statuses || (!sOpen && !sConsumeUntilRelease)) return;
    PADStatus *pad = &statuses[0];
    sMenuPad.mButton = pad->mButton;
    sMenuPad.mStickX = pad->mStickX;
    sMenuPad.mStickY = pad->mStickY;
    sMenuPad.mSubStickX = pad->mSubStickX;
    sMenuPad.mSubStickY = pad->mSubStickY;
    sMenuPad.mTriggerLeft = pad->mTriggerLeft;
    sMenuPad.mTriggerRight = pad->mTriggerRight;
    sMenuPad.mAnalogA = pad->mAnalogA;
    sMenuPad.mAnalogB = pad->mAnalogB;
    sMenuPad.mCurError = pad->mCurError;
    pad->mButton = 0u;
    pad->mStickX = 0u;
    pad->mStickY = 0u;
    pad->mSubStickX = 0u;
    pad->mSubStickY = 0u;
    pad->mTriggerLeft = 0u;
    pad->mTriggerRight = 0u;
    pad->mAnalogA = 0u;
    pad->mAnalogB = 0u;
    if (sConsumeUntilRelease && sMenuPad.mButton == 0u) {
        sConsumeUntilRelease = false;
    }
}

void tick() {
    if (sNoticeTimer != 0u) --sNoticeTimer;
    if (sConfirmTimer != 0u && --sConfirmTimer == 0u) sConfirmAction = 0u;

    const u16 buttons = inputButtons();
    if (!sOpen) {
        if (!sConsumeUntilRelease && edge(buttons, kDPadDown) &&
            missionReady()) {
            sOpen = true;
            sSelection = 0u;
            sConfirmAction = 0u;
            sConfirmTimer = 0u;
        }
        sPreviousButtons = buttons;
        return;
    }

    if (edge(buttons, kButtonB)) {
        sOpen = false;
        sConsumeUntilRelease = true;
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (edge(buttons, kButtonL) || edge(buttons, kButtonR)) {
        const u32 page = static_cast<u32>(sPage);
        sPage = static_cast<Page>(edge(buttons, kButtonL) ?
                                     (page + kPageCount - 1u) % kPageCount :
                                     (page + 1u) % kPageCount);
        sSelection = 0u;
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (edge(buttons, kDPadUp)) {
        const u32 count = rowCount();
        sSelection = (sSelection + count - 1u) % count;
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (edge(buttons, kDPadDown)) {
        sSelection = (sSelection + 1u) % rowCount();
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (edge(buttons, kDPadLeft)) {
        handleAction(-1);
    } else if (edge(buttons, kDPadRight)) {
        handleAction(1);
    } else if (edge(buttons, kButtonA)) {
        handleAction(0);
    }
    if (missionReady()) {
        const u32 area = plantArea();
        if (area != sPlantArea) {
            sPlantArea = area;
            sPlantChoice = 0u;
        }
    }
    sPreviousButtons = buttons;
}

void draw(void *directPrint) {
    if (!sOpen) return;
    auto erase = reinterpret_cast<DirectPrintEraseFn>(
        kDirectPrintEraseAddress);
    auto text = reinterpret_cast<DirectPrintDrawStringFn>(
        kDirectPrintDrawStringAddress);
    erase(directPrint, 0u, kMenuTop, 320u, kMenuHeight);

    const char *pageName = sPage == Page::Settings ? "SETTINGS" :
                           sPage == Page::Doors ? "DOORS" :
                           sPage == Page::Plant ? "PLANT/ROOM" : "AUDIO";
    text(directPrint, 2u, kMenuTop + 2u, "PRACTICE %lu/4 %s  L/R PAGE",
         static_cast<u32>(sPage) + 1u, pageName);
    text(directPrint, 2u, kMenuTop + 9u,
         missionReady() ? "GAME READY" : "GAME BUSY - ACTIONS LOCKED");

    if (sPage == Page::Settings && missionReady()) {
        const u32 count = booCount();
        const bool booSpawn = flag(22u) && flag(73u) && flag(75u);
        text(directPrint, 2u, kMenuTop + 23u, "%s MANSION       %s",
             selectionMark(0u),
             readWord(kMansionModeAddress) ? "HIDDEN" : "NORMAL");
        text(directPrint, 2u, kMenuTop + 30u, "%s BOO SPAWNING  %s",
             selectionMark(1u), onOff(booSpawn));
        if (count == 0xFFFFFFFFu) {
            text(directPrint, 2u, kMenuTop + 37u,
                 "%s BOO GATES     NO PLAYER", selectionMark(2u));
        } else if (count >= 40u) {
            text(directPrint, 2u, kMenuTop + 37u,
                 "%s BOO GATES     N/A (%lu BOOS)", selectionMark(2u), count);
        } else {
            text(directPrint, 2u, kMenuTop + 37u,
                 "%s BOO GATES     %s (%lu BOOS)", selectionMark(2u),
                 booRequirementsDisabled(count) ? "OFF" : "ON", count);
        }
        text(directPrint, 2u, kMenuTop + 44u, "%s BLACKOUT      %s",
             selectionMark(3u), onOff(flag(61u) != 0u));
        const u32 player = playerAddress();
        text(directPrint, 2u, kMenuTop + 51u, "%s HP            %ld/%ld",
             selectionMark(4u), player ? readSignedHalf(player + 0xFCu) : 0,
             player ? readSignedHalf(player + 0xFFCu) : 0);
        text(directPrint, 2u, kMenuTop + 58u, "%s SAVE TO CARD",
             selectionMark(5u));
    } else if (sPage == Page::Settings) {
        text(directPrint, 2u, kMenuTop + 23u, "ENTER A STABLE MANSION ROOM");
    } else if (sPage == Page::Doors && missionReady()) {
        text(directPrint, 2u, kMenuTop + 23u, "%s FIRE DOORS       %s",
             selectionMark(0u), onOff(flag(54u) == 0u));
        text(directPrint, 2u, kMenuTop + 30u, "%s OBSERVATORY DOOR %s",
             selectionMark(1u), onOff(flag(83u) == 0u));
        text(directPrint, 2u, kMenuTop + 37u, "%s ROOM TRAPS       %s",
             selectionMark(2u), onOff(flag(70u) == 0u));
        text(directPrint, 2u, kMenuTop + 44u, "%s UNLOCK ALL DOORS",
             selectionMark(3u));
    } else if (sPage == Page::Doors) {
        text(directPrint, 2u, kMenuTop + 23u, "ENTER A STABLE MANSION ROOM");
    } else if (sPage == Page::Plant && missionReady()) {
        u32 count;
        const PlantPreset *presets = plantPresets(sPlantArea, &count);
        if (!presets) {
            text(directPrint, 2u, kMenuTop + 23u,
                 "%s PLANT PRESET   AREA 1 N/A", selectionMark(0u));
        } else {
            text(directPrint, 2u, kMenuTop + 23u,
                 "%s PLANT AREA %lu  %s", selectionMark(0u), sPlantArea,
                 presets[sPlantChoice % count].name);
        }
        const u32 room = currentRoomId();
        text(directPrint, 2u, kMenuTop + 30u,
             "%s CLEAR ROOM %s R%lu", selectionMark(1u),
             roomClearRecipe(room) ? "READY" : "NO-WARP N/A",
             room == 0xFFFFFFFFu ? 999u : room);
        text(directPrint, 2u, kMenuTop + 44u,
             "59/72 ROOMS CLEAR WITHOUT RELOAD");
    } else if (sPage == Page::Plant) {
        text(directPrint, 2u, kMenuTop + 23u, "ENTER A STABLE MANSION ROOM");
    } else {
        text(directPrint, 2u, kMenuTop + 23u, "%s BGM %02lu  %s",
             selectionMark(0u), sBgmId, kBgmNames[sBgmId]);
        text(directPrint, 2u, kMenuTop + 30u, "%s STOP BGM",
             selectionMark(1u));
    }

    if (sNoticeTimer != 0u && sNotice) {
        text(directPrint, 2u, kMenuTop + 72u, "%s", sNotice);
    }
    text(directPrint, 2u, kMenuTop + 81u,
         "UP/DOWN SELECT  LEFT/RIGHT SET  A APPLY  B CLOSE");
}

bool isOpen() {
    return sOpen;
}

}  // namespace LMPractice

extern "C" u32 diagnosticPadRead(PADStatus *statuses) {
    const u32 connected =
        reinterpret_cast<PadReadFn>(kPadReadAddress)(statuses);
    LMPractice::filterPadRead(statuses);
    return connected;
}

#endif  // defined(SUSAMUNE_VERSION_LMJ)
