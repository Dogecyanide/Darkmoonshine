#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_practice.hxx"

#include "Dolphin/PAD.h"
#include "Dolphin/types.h"
#include "lm_state.hxx"
#include "lm_tools.hxx"
#include "lm_timer.hxx"
#include "lm_preferences.hxx"
#include "lm_warp.hxx"
#include "lm_colour.hxx"
#include "lm_draw.hxx"
#include "lm_menu_navigation.hxx"
#include "lm_elements.hxx"
#include "lm_notice.hxx"
#include "susamune/lm_room_id.h"
#include "susamune/lm_room_tools.h"
#include "susamune/lm_door_state.h"
#include "susamune/lm_name_keyboard.h"
#include "susamune/lm_archive_delete.h"
#include "susamune/lm_branding.h"
#include "susamune/lm_reset_bind.h"

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
constexpr u32 kRoomCountAddress = 0x804A0CF4u;
constexpr u32 kRoomPersistenceAddress = 0x803C2EB0u;
constexpr u32 kEventPlayCountsAddress = 0x803C2E30u;
constexpr u32 kFoyerPartnerAddress = 0x80398C8Cu;
constexpr u32 kActorTableAddress = 0x803C8490u;
constexpr u32 kActorCountAddress = 0x804A12B8u;
constexpr u32 kPlayerVtable = 0x8034EE50u;
constexpr u32 kKeyTableCountAddress = 0x804A0D88u;

constexpr u32 kGetFlagAddress = 0x80065320u;
constexpr u32 kSetFlagAddress = 0x80065358u;
constexpr u32 kClearFlagAddress = 0x80065398u;
constexpr u32 kSetPlayerHpAddress = 0x800B7BFCu;
constexpr u32 kDoorKeyStateAddress = 0x8001B030u;
constexpr u32 kBlackoutAddress = 0x80037498u;
constexpr u32 kRoomLightAddress = 0x800197D8u;
constexpr u32 kRoomEntryLightAddress = 0x80018748u;
constexpr u32 kBgmStartAddress = 0x801884E8u;
constexpr u32 kBgmStopAddress = 0x801885E8u;
constexpr u32 kActorLookupAddress = 0x800E7F08u;
constexpr u32 kActorKillAddress = 0x800E7EDCu;
constexpr u32 kDirectPrintDrawStringAddress = 0x801D49F8u;

constexpr u16 kDPadLeft = 0x0001u;
constexpr u16 kDPadRight = 0x0002u;
constexpr u16 kDPadDown = 0x0004u;
constexpr u16 kDPadUp = 0x0008u;
constexpr u16 kButtonZ = 0x0010u;
constexpr u16 kButtonR = 0x0020u;
constexpr u16 kButtonL = 0x0040u;
constexpr u16 kButtonA = 0x0100u;
constexpr u16 kButtonB = 0x0200u;
constexpr u16 kButtonX = 0x0400u;
constexpr u16 kButtonY = 0x0800u;
constexpr u16 kButtonStart = 0x1000u;

constexpr u16 kMenuTop = 31u;
constexpr u16 kMenuHeight = 176u;
constexpr u16 kArchivePanelHeight = 202u;
constexpr u32 kPageCount = 9u;
constexpr u32 kConfirmFrames = 180u;
constexpr u32 kNoticeFrames = 180u;
constexpr u32 kRoomEntrySize = 0x14Cu;
constexpr u32 kRoomCountLimit = LM_ROOM_TABLE_LIMIT;

using GetFlagFn = u32 (*)(u32);
using FlagFn = void (*)(u32);
using ActorLookupFn = void *(*)(const char *);
using PlayerHpFn = void (*)(void *, s32);
using TwoWordFn = void (*)(u32, u32);
using WordFn = void (*)(u32);
using VoidFn = void (*)();
using DirectPrintDrawStringFn = void (*)(void *, u16, u16, const char *, ...);
using PadReadFn = u32 (*)(PADStatus *);

enum class Page : u32 {
    States,
    Warps,
    Display,
    Timing,
    Colours,
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
    bool reload = false;
    bool noLight = false;
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
constexpr const char *kClearRoom22[] = {"demo_builder", "iyapoo"};
constexpr const char *kClearRoom24[] = {"iyapoo"};
constexpr const char *kClearRoom25[] = {"iyapoo"};
constexpr const char *kClearRoom28[] = {"demo_girl"};
constexpr const char *kClearRoom34[] = {"demo_father", "iyapoo"};
constexpr const char *kClearRoom57[] = {"dm_gaka", "tony_montana", "tony_montana", "tony_montana"};
constexpr const char *kClearRoom61[] = {"dm_snowman"};
constexpr u8 kClearFlags16[] = {67u};
constexpr u8 kClearFlags24[] = {46u};
constexpr u8 kClearFlags25[] = {51u};
constexpr u8 kClearFlags28[] = {18u, 38u, 36u, 25u};
constexpr u8 kClearFlags41[] = {49u, 50u, 52u};
constexpr u8 kClearFlags55[] = {40u};
constexpr u8 kClearFlags57[] = {31u, 177u, 178u, 179u, 180u, 181u, 182u, 183u, 55u};
constexpr u8 kClearFlags59[] = {81u};
constexpr u8 kClearFlags61[] = {28u};
constexpr u8 kClearFlags70[] = {66u};
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
    {10u, nullptr, 0u, nullptr, 0u, true},
    {16u, nullptr, 0u, kClearFlags16, 1u, true, true},
    {22u, kClearRoom22, 2u, nullptr, 0u, true},
    {24u, kClearRoom24, 1u, kClearFlags24, 1u, true},
    {25u, kClearRoom25, 1u, kClearFlags25, 1u, true},
    {28u, kClearRoom28, 1u, kClearFlags28, 4u, true},
    {34u, kClearRoom34, 2u, nullptr, 0u, true},
    {41u, nullptr, 0u, kClearFlags41, 3u, true},
    {55u, nullptr, 0u, kClearFlags55, 1u, true},
    {57u, kClearRoom57, 4u, kClearFlags57, 9u, true},
    {59u, nullptr, 0u, kClearFlags59, 1u, true, true},
    {61u, kClearRoom61, 1u, kClearFlags61, 1u, true},
    {70u, nullptr, 0u, kClearFlags70, 1u, true, true},
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
u32 sArchiveId = 1u;
bool sArchiveBrowser;
u32 sArchiveSelection;
u32 sArchiveHistory[64];
u32 sArchiveDepth;
LMArchiveDelete::Prompt sDeletePrompt = {};
bool sDeletePending;
char sDeleteResult[96];
enum class NameAction : u8 { None, Export, Rename };
NameAction sNameAction;
LMNameKeyboard::State sNameDraft;
u32 sNameArchiveId;
bool sNamePending;
u32 sColourPreset;
u32 sElementChoice;
bool sBooSafe;
unsigned sResetBind;
LMResetBind::Trigger sResetTrigger;
LMResetBind::Recorder sResetRecorder;
bool sResetRecording, sResetPending;
LmRoomResetRecipe sResetRecipe;
u32 sResetPersistence[2];
u32 sResetPersistenceCount;
u32 sFoyerLightEntry;
u32 sHeldFrames;
bool sHome = true;
u32 sPageSelections[kPageCount];
LMMenuNavigation::Stick sMenuStick;
constexpr const char *kPageNames[] = {
    "Savestates", "Room warps", "Displays", "Input timing", "Luigi colour",
    "Game options", "Doors", "Room tools", "Audio",
};
constexpr const char *kPageHelp[] = {
    "Memory slots and SD archives",
    "Jump to a room or boss arena",
    "Controller, position and lag overlays",
    "Measure presses against a reference",
    "Recolour Luigi's shirt and cap",
    "Mansion, Boos, health and game saving",
    "Door locks and room traps",
    "Plant presets, room clear and reset",
    "Choose or stop the background music",
};

inline u32 readWord(u32 address) {
    return *reinterpret_cast<volatile u32 *>(address);
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
    return (address & 3u) == 0u && size <= kMem1End - kMem1Start && address >= kMem1Start &&
           address <= kMem1End - size;
}

bool validGameHeap(u32 address, u32 size) {
    const u32 heap = readWord(kGameHeapAddress);
    if (!validMem1(heap, 0x38u)) return false;
    const u32 start = readWord(heap + 0x30u);
    const u32 end = readWord(heap + 0x34u);
    return LmDoorRangeInside(address, size, start, end) != 0;
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

bool menuReady() {
    const u32 map = readWord(kMapValueAddress);
    return LMState::readyForAction() && !LMWarp::active() &&
           (map == 2u || map == 9u || map == 10u || map == 11u || map == 13u) &&
           readWord(kSceneValueAddress) == 2u &&
           readWord(kMainLoopModeAddress) == 2u &&
           readWord(kPendingSceneAddress) == 2u &&
           readWord(kMainLoopExitAddress) == 0u &&
           validMem1(readWord(kMissionModeAddress), 0x24u);
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
    if (!missionReady() || !LMState::readyForActionNow()) {
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
    const u32 mission = readWord(kMissionModeAddress);
    if (!validGameHeap(mission, 0x24u)) return 0u;
    const u32 manager = readWord(mission + 8u);
    if (!validGameHeap(manager, 0xE0Cu)) return 0u;
    const u32 index = readWord(manager + 0xE08u);
    const u32 count = readWord(kActorCountAddress);
    if (count > 128u || index >= count) return 0u;
    const u32 player = readWord(kActorTableAddress + index * 4u);
    return validGameHeap(player, 0x1000u) && readWord(player) == kPlayerVtable ? player : 0u;
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
    const u32 roomCount = *reinterpret_cast<volatile u8 *>(kRoomCountAddress);
    const u32 managerRoom = readWord(roomManager + 0x0Cu);
    const u32 loadedRoom = managerRoom >> 24;
    if (!roomCount || roomCount > kRoomCountLimit || loadedRoom >= roomCount ||
        !validGameHeap(roomTable, roomCount * kRoomEntrySize)) {
        return false;
    }
    return LmRoomRecordMatches(readWord(player + 0xB4u), managerRoom,
               readWord(roomTable + loadedRoom * kRoomEntrySize + 0x10u), roomCount) != 0;
}

u32 currentRoomId() {
    if (!roomActionReady()) return 0xFFFFFFFFu;
    const u32 roomManager = readWord(kRoomManagerAddress);
    return static_cast<u32>(LmPlayerRoomId(readWord(roomManager + 0x0Cu)));
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
    return actor == 0u || (validGameHeap(actor, 0x3Cu) && readWord(actor + 0x38u) < 128u);
}

void killActor(const char *name) {
    const u32 actor = reinterpret_cast<u32>(lookupActor(name));
    if (!actor || !validGameHeap(actor, 0x3Cu) || readWord(actor + 0x38u) >= 128u) return;
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
        const u32 mission = readWord(kMissionModeAddress);
        if (!validGameHeap(readWord(mission + 0x20u), 0x10u) ||
            !validGameHeap(readWord(mission + 0x18u), 0x10u)) {
            showNotice("LIGHTING MANAGER NOT READY"); return;
        }
        reinterpret_cast<WordFn>(kBlackoutAddress)(on ? 1u : 0u);
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
    } else if (row == 6u) {
        if (direction) {
            sElementChoice = direction < 0 ? (sElementChoice + LMElements::kChoiceCount - 1u) % LMElements::kChoiceCount :
                                            (sElementChoice + 1u) % LMElements::kChoiceCount;
        } else {
            showNotice(LMElements::statusText(LMElements::apply(sElementChoice, playerAddress())));
        }
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
    if (recipe->reload) {
        if (!LMWarp::requestRoomReload(true, sBooSafe)) { showNotice(LMWarp::statusText()); return; }
    } else {
        if (!LMPractice::prepareRoomReload(room, true)) return;
        LMPractice::commitRoomReload(room, true);
    }
    sOpen = false;
    sConsumeUntilRelease = true;
    showNotice(recipe->reload ? "ROOM CLEAR REQUESTED" : "CURRENT ROOM CLEARED");
}

void resetCurrentRoom(bool shortcut = false) {
    if (!menuReady() || !LMState::readyForActionNow() || !noEventRunning()) {
        showNotice("WAIT: ROOM/EVENT BUSY");
        if (shortcut) LMNotice::show(LM_POPUP_BUSY);
        return;
    }
    if (!shortcut && !confirm(6u)) return;
    if (!LMWarp::requestRoomReload(false, sBooSafe)) { showNotice(LMWarp::statusText()); return; }
    sOpen = false;
    sConsumeUntilRelease = true;
    showNotice("ROOM RESET REQUESTED");
}

u32 rowCount() {
    switch (sPage) {
    case Page::States:
        return 8u;
    case Page::Warps:
        return LMWarp::count();
    case Page::Display:
    case Page::Timing:
        return LMTools::rows(sPage == Page::Timing);
    case Page::Colours:
        return 6u;
    case Page::Settings:
        return 7u;
    case Page::Doors:
        return 4u;
    case Page::Plant:
        return 4u;
    case Page::Audio:
        return 2u;
    }
    return 1u;
}

bool edge(u16 buttons, u16 mask) {
    return (buttons & mask) != 0u && (sPreviousButtons & mask) == 0u;
}

bool navigation(u16 buttons, u16 mask) {
    return edge(buttons, mask) || ((buttons & mask) && sHeldFrames >= 18u &&
                                  (sHeldFrames - 18u) % 3u == 0u);
}

u16 inputButtons() {
    if (sOpen) {
        if (sMenuPad.mCurError) { sMenuStick.held = 0u; return 0u; }
        u16 buttons = sMenuPad.mButton;
        const u16 stick = sMenuStick.sample(static_cast<s8>(sMenuPad.mStickX),
                                            static_cast<s8>(sMenuPad.mStickY));
        if (!(buttons & 15u)) buttons |= stick;
        if (sMenuPad.mTriggerLeft >= 128u) buttons |= kButtonL;
        if (sMenuPad.mTriggerRight >= 128u) buttons |= kButtonR;
        return buttons;
    }
    if (sConsumeUntilRelease) return sMenuPad.mButton;
    return reinterpret_cast<volatile PADStatus *>(kPadStatusAddress)->mButton;
}

void clearConfirmation() { sConfirmAction = sConfirmTimer = 0u; }

bool browseArchives(u32 cursor) {
    clearConfirmation();
    if (!LMState::requestCatalog(cursor)) {
        showNotice(LMState::catalogText());
        return false;
    }
    sArchiveSelection = 0u;
    return true;
}

void beginNameKeyboard(NameAction action, u32 archiveId, const char *name) {
    if (LMState::storageBusy()) { showNotice(LMState::storageText()); return; }
    if (action == NameAction::Export && !LMState::slotHasState(LMState::selectedSlot())) {
        showNotice("SAVE A MEMORY STATE FIRST");
        return;
    }
    clearConfirmation();
    sNoticeTimer = 0u;
    sNameAction = action;
    sNameArchiveId = archiveId;
    sNamePending = false;
    LMNameKeyboard::begin(sNameDraft, name);
}

void nameKeyboardInput(u16 buttons) {
    if (sNamePending) {
        if (LMState::storageBusy()) return;
        const bool renamed = sNameAction == NameAction::Rename;
        sNamePending = false;
        sNameAction = NameAction::None;
        if (renamed) browseArchives(LMState::catalogCursor());
        showNotice(LMState::storageText());
        return;
    }
    u16 pressed = buttons & ~sPreviousButtons;
    if (sNameDraft.confirmation == LMNameKeyboard::NoPrompt && !(buttons & kButtonStart)) {
        const u16 repeat[] = {kDPadLeft, kDPadRight, kDPadUp, kDPadDown,
                              kButtonA, kButtonB, kButtonX};
        for (u32 i = 0u; i < sizeof(repeat) / sizeof(repeat[0]); ++i)
            if (navigation(buttons, repeat[i])) pressed |= repeat[i];
    }
    const LMNameKeyboard::Result result = LMNameKeyboard::update(sNameDraft, pressed, buttons);
    if (result == LMNameKeyboard::Discard) {
        sNameAction = NameAction::None;
        showNotice("NAME CHANGES DISCARDED");
    } else if (result == LMNameKeyboard::Commit) {
        sNamePending = sNameAction == NameAction::Rename ?
            LMState::requestRename(sNameArchiveId, sNameDraft.text) :
            LMState::requestExport(sNameDraft.text);
        if (!sNamePending) showNotice(LMState::storageText());
    }
}

void drawNameKeyboard(void *directPrint, void *xfb) {
    auto text = reinterpret_cast<DirectPrintDrawStringFn>(kDirectPrintDrawStringAddress);
    const bool rename = sNameAction == NameAction::Rename;
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 308, kArchivePanelHeight, 0x0C171Eu);
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 308, 17, 0x24543Fu);
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 2, kArchivePanelHeight, 0x8FDCABu);
    text(directPrint, 13u, kMenuTop + 5u, "%s", rename ? "RENAME SD STATE" : "NAME NEW SD STATE");
    text(directPrint, 272u, kMenuTop + 5u, "%lu/31", static_cast<u32>(sNameDraft.length));
    if (rename) text(directPrint, 14u, 55u, "ARCHIVE %08lu  /  NAME ONLY", sNameArchiveId);
    else text(directPrint, 14u, 55u, "SLOT %lu  /  CREATES A NEW SD FILE", LMState::selectedSlot() + 1u);
    LMDraw::fillBox(xfb, 640u, 480u, 14, 68, 292, 22, 0x1A2E35u);
    text(directPrint, 21u, 75u, "%s_", sNameDraft.text);
    const char *characters = LMNameKeyboard::characters(sNameDraft);
    for (u32 i = 0u; i < 32u; ++i) {
        const u16 x = 29u + (i % 8u) * 34u, y = 100u + (i / 8u) * 18u;
        LMDraw::fillBox(xfb, 640u, 480u, x, y, 24, 16,
                       i == sNameDraft.cursor ? 0x31734Fu : 0x1A2E35u);
        if (i == sNameDraft.cursor) LMDraw::fillBox(xfb, 640u, 480u, x, y, 2, 16, 0xB0F1C8u);
        const char one[2] = {characters[i], '\0'};
        text(directPrint, x + 9u, y + 5u, "%s", one);
    }
    text(directPrint, 13u, 180u, "D-pad: Select  A: Type  B: Delete  X: Space");
    text(directPrint, 13u, 192u, "Y: Case   L/R: Page   Z: Clear");
    text(directPrint, 13u, 204u, "START: Keep   X+START: Discard changes");
    if (sNoticeTimer && sNotice) text(directPrint, 13u, 222u, "%.48s", sNotice);
    else text(directPrint, 13u, 222u, "Empty name: show archive ID. Memory unchanged.");
    if (sNamePending || sNameDraft.confirmation != LMNameKeyboard::NoPrompt) {
        LMDraw::fillBox(xfb, 640u, 480u, 34, 105, 252, 63, 0x101E26u);
        LMDraw::fillBox(xfb, 640u, 480u, 34, 105, 252, 2, 0x8FDCABu);
        if (sNamePending) {
            text(directPrint, 45u, 121u, "%s", rename ? "RENAMING SD ARCHIVE..." : "EXPORTING STATE TO SD...");
            text(directPrint, 45u, 146u, "Please wait - do not remove the SD.");
        } else {
            const char *prompt = sNameDraft.confirmation == LMNameKeyboard::KeepPrompt ?
                (rename ? "Use this archive name?" : "Export this state to SD?") :
                sNameDraft.confirmation == LMNameKeyboard::DiscardPrompt ?
                "Discard name changes?" : "Clear the name?";
            text(directPrint, 45u, 119u, "%s", prompt);
            text(directPrint, 45u, 133u, "Memory state stays unchanged.");
            text(directPrint, 45u, 152u, "A: Confirm    B: Go back");
        }
    }
    LMDraw::flush(xfb, 640u, 480u, kMenuTop, kArchivePanelHeight);
}

void archiveInput(u16 buttons) {
    if (sDeletePending) {
        if (LMState::storageBusy()) return;
        sDeletePending = false;
        const char *result = LMState::storageText();
        u32 i = 0u;
        for (; i < sizeof(sDeleteResult) - 1u && result[i]; ++i) sDeleteResult[i] = result[i];
        sDeleteResult[i] = 0;
        if (browseArchives(0u)) sArchiveDepth = 0u;
        showNotice(sDeleteResult);
        return;
    }
    if (sDeletePrompt.active) {
        const LMArchiveDelete::Result result = LMArchiveDelete::update(
            sDeletePrompt, buttons & ~sPreviousButtons, sMenuPad.mCurError == 0u);
        if (result == LMArchiveDelete::Cancelled) showNotice("DELETE CANCELLED");
        else if (result == LMArchiveDelete::Confirmed) {
            sDeletePending = LMState::requestDelete(sDeletePrompt.id, sDeletePrompt.token);
            if (!sDeletePending) showNotice(LMState::storageText());
        }
        return;
    }
    if (edge(buttons, kButtonB)) {
        sArchiveBrowser = false;
        clearConfirmation();
        return;
    }
    if (LMState::storageBusy()) return;
    const u32 count = LMState::catalogCount();
    if (count && sArchiveSelection < count && edge(buttons, kButtonZ)) {
        clearConfirmation();
        sNoticeTimer = 0u;
        if (!LMArchiveDelete::begin(sDeletePrompt, LMState::catalogId(sArchiveSelection),
                LMState::catalogDeleteToken(sArchiveSelection), LMState::catalogName(sArchiveSelection)))
            showNotice("REFRESH THE LIST BEFORE DELETING");
    } else if (count && edge(buttons, kButtonX)) {
        beginNameKeyboard(NameAction::Rename, LMState::catalogId(sArchiveSelection),
                          LMState::catalogName(sArchiveSelection));
    } else if (edge(buttons, kButtonY)) {
        if (browseArchives(0u)) sArchiveDepth = 0u;
    } else if (edge(buttons, kDPadLeft) && sArchiveDepth) {
        if (browseArchives(sArchiveHistory[sArchiveDepth - 1u])) --sArchiveDepth;
    } else if (edge(buttons, kDPadRight) && LMState::catalogHasMore()) {
        const u32 previous = LMState::catalogCursor();
        if (browseArchives(LMState::catalogNextCursor())) {
            if (sArchiveDepth == 64u) {
                for (u32 i = 1u; i < 64u; ++i) sArchiveHistory[i - 1u] = sArchiveHistory[i];
                --sArchiveDepth;
            }
            sArchiveHistory[sArchiveDepth++] = previous;
        }
    } else if (count && (navigation(buttons, kDPadUp) || navigation(buttons, kDPadDown))) {
        sArchiveSelection = (sArchiveSelection +
            (navigation(buttons, kDPadUp) ? count - 1u : 1u)) % count;
        clearConfirmation();
    } else if (count && edge(buttons, kButtonA)) {
        if (!LMState::catalogCompatible(sArchiveSelection)) {
            showNotice(LMState::catalogEntryText(sArchiveSelection));
            clearConfirmation();
        } else {
            const u32 id = LMState::catalogId(sArchiveSelection);
            if (confirm(0x53440000u ^ id)) {
                sArchiveId = id;
                showNotice(LMState::requestImport(id) ? "IMPORTING - WAIT..." : LMState::storageText());
            }
        }
    }
}

void switchPage(u32 page) {
    sPageSelections[static_cast<u32>(sPage)] = sSelection;
    sPage = static_cast<Page>(page);
    sSelection = sPageSelections[page] % rowCount();
    sHome = false;
    clearConfirmation();
}

void handleAction(s32 direction) {
    if (sPage == Page::States) {
        bool close = false;
        if (sSelection == 0u) {
            const u32 n = LMState::slotCount();
            const u32 slot = LMState::selectedSlot();
            LMState::selectSlot(direction < 0 ? (slot + n - 1u) % n : (slot + 1u) % n);
        } else if (sSelection == 5u) {
            if (direction < 0) { if (sArchiveId > 1u) --sArchiveId; }
            else if (direction > 0) { if (sArchiveId < 99999999u) ++sArchiveId; }
            else if (LMState::lastArchiveId()) sArchiveId = LMState::lastArchiveId();
        } else if (!direction) {
            if (sSelection == 1u) close = LMState::requestSave();
            if (sSelection == 2u) close = LMState::requestLoad();
            if (sSelection == 3u) beginNameKeyboard(NameAction::Export, 0u, nullptr);
            if (sSelection == 4u) {
                sArchiveBrowser = true;
                sArchiveDepth = sArchiveSelection = 0u;
                browseArchives(0u);
            }
            if (sSelection == 6u && confirm(4u))
                showNotice(LMState::requestImport(sArchiveId) ? "IMPORTING - WAIT..." : LMState::storageText());
            if (sSelection == 7u && confirm(5u)) LMState::clearSelectedSlot();
        }
        if (close) { sOpen = false; sConsumeUntilRelease = true; }
    } else if (sPage == Page::Warps) {
        if (direction) {
            const u32 count = LMWarp::count();
            sSelection = direction < 0 ? (sSelection + count - 10u % count) % count : (sSelection + 10u) % count;
            sConfirmAction = sConfirmTimer = 0u;
        } else if (confirm(100u + sSelection)) {
            if (LMWarp::request(sSelection, sBooSafe)) { sOpen = false; sConsumeUntilRelease = true; }
            else showNotice(LMWarp::statusText());
        }
    } else if (sPage == Page::Display || sPage == Page::Timing) {
        if (LMTools::action(sPage == Page::Timing, sSelection, direction)) {
            sOpen = false; sConsumeUntilRelease = true;
        }
    } else if (sPage == Page::Colours) {
        if (sSelection == 0u) LMColour::setEnabled(!LMColour::enabled());
        else if (sSelection == 1u) {
            const u32 n = LMColour::presetCount();
            if (sColourPreset >= n) sColourPreset = 0u;
            if (direction < 0) sColourPreset = (sColourPreset + n - 1u) % n;
            if (direction > 0) sColourPreset = (sColourPreset + 1u) % n;
            LMColour::applyPreset(sColourPreset);
        } else if (sSelection < 5u) {
            const u32 shift = (4u - sSelection) * 8u;
            const u32 rgb = LMColour::rgb();
            const u32 channel = (rgb >> shift) & 255u;
            const u32 value = direction < 0 ? (channel + 255u) & 255u : (channel + 1u) & 255u;
            LMColour::setRgb((rgb & ~(255u << shift)) | (value << shift));
            sColourPreset = LMColour::presetCount();
        } else if (!direction) { LMColour::setEnabled(false); sColourPreset = 0u; }
    } else if (sPage == Page::Settings) {
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
        } else if (sSelection == 1u && direction == 0) {
            clearCurrentRoom();
        } else if (sSelection == 2u && direction == 0) {
            resetCurrentRoom();
        } else if (sSelection == 3u && direction == 0) {
            sResetRecorder.begin();
            sResetRecording = true;
            sResetTrigger.armed = false;
            clearConfirmation();
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
void writePreferences(unsigned int v[48]) { v[45]=sBooSafe; v[47]=sResetBind; }
void readPreferences(const unsigned int v[48],unsigned int,unsigned int hi) {
    if((hi&(1u<<13)) && v[45]<=1u) sBooSafe=v[45];
    if((hi&(1u<<15)) && (!v[47] || LMResetBind::valid(v[47]))) {
        sResetBind=v[47];
        sResetTrigger.armed=false;
    }
}

void filterPadRead(PADStatus *statuses) {
    if (!statuses) return;
    LMTools::samplePad(statuses[0]);
    // The closing A/B debounce must not discard a fresh D-pad action.
    LMState::samplePad(statuses[0], !sOpen);
    if (sResetTrigger.sample(sResetBind, statuses[0].mButton,
                            !sOpen && !sConsumeUntilRelease, statuses[0].mCurError == 0)) {
        sResetPending = true;
        sConsumeUntilRelease = true;
    }
    if (!sOpen && !sConsumeUntilRelease) return;
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
    sHeldFrames = buttons == sPreviousButtons ? (sHeldFrames + 1u) % 0x1000000u : 0u;
    if (!sOpen) {
        if (sResetPending) {
            sResetPending = false;
            resetCurrentRoom(true);
            sPreviousButtons = buttons;
            return;
        }
        if (!sConsumeUntilRelease && edge(buttons, kDPadDown) &&
            menuReady()) {
            sOpen = true;
            clearConfirmation();
            sMenuStick.held = 0u;
        }
        sPreviousButtons = buttons;
        return;
    }

    if (sResetRecording) {
        const LMResetBind::Result result = sResetRecorder.sample(sMenuPad.mButton, sMenuPad.mCurError == 0);
        if (result != LMResetBind::Waiting) {
            sResetRecording = false;
            if (result == LMResetBind::Accepted) {
                sResetBind = sResetRecorder.chord;
                showNotice("RESET COMBO SAVED; CLOSE MENU TO SAVE TO SD");
            } else showNotice(result == LMResetBind::Cancelled ? "BIND UNCHANGED" : "USE 2-4 BUTTONS; NO START OR D-LEFT/RIGHT/DOWN");
        }
    } else if (LMTimer::menuOpen()) {
        LMTimer::updateMenu(sMenuPad);
    } else if (sNameAction != NameAction::None) {
        nameKeyboardInput(buttons);
    } else if (sArchiveBrowser) {
        archiveInput(buttons);
    } else if (edge(buttons, kButtonB)) {
        if (!sHome) {
            sPageSelections[static_cast<u32>(sPage)] = sSelection;
            sHome = true;
        } else {
            sOpen = false;
            sConsumeUntilRelease = true;
        }
        clearConfirmation();
    } else if (sHome) {
        const u16 directions[] = {kDPadUp, kDPadDown, kDPadLeft, kDPadRight};
        for (u32 i = 0u; i < 4u; ++i) {
            if (navigation(buttons, directions[i])) {
                sPage = static_cast<Page>(LMMenuNavigation::grid(static_cast<u32>(sPage), directions[i]));
                break;
            }
        }
        if (edge(buttons, kButtonA)) {
            sSelection = sPageSelections[static_cast<u32>(sPage)] % rowCount();
            sHome = false;
        }
    } else if (sPage == Page::Warps && edge(buttons, kButtonX)) {
        sBooSafe = !sBooSafe;
        clearConfirmation();
        showNotice(sBooSafe ? "BOO-SAFE ENTRY ON" : "STANDARD ROOM ENTRIES");
    } else if (sPage == Page::Plant && sSelection == 3u && edge(buttons, kButtonZ)) {
        sResetBind = 0u;
        sResetTrigger.armed = false;
        showNotice("RESET COMBO OFF");
    } else if (edge(buttons, kButtonL) || edge(buttons, kButtonR)) {
        const u32 page = static_cast<u32>(sPage);
        switchPage(edge(buttons, kButtonL) ? (page + kPageCount - 1u) % kPageCount :
                                           (page + 1u) % kPageCount);
    } else if (navigation(buttons, kDPadUp)) {
        const u32 count = rowCount();
        sSelection = (sSelection + count - 1u) % count;
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (navigation(buttons, kDPadDown)) {
        sSelection = (sSelection + 1u) % rowCount();
        sConfirmAction = 0u;
        sConfirmTimer = 0u;
    } else if (edge(buttons, kDPadLeft) ||
               (((sPage == Page::Colours && sSelection >= 2u && sSelection <= 4u) ||
                 (sPage == Page::Timing && sSelection >= 4u) ||
                 (sPage == Page::States && sSelection == 5u)) && navigation(buttons, kDPadLeft))) {
        handleAction(-1);
    } else if (edge(buttons, kDPadRight) ||
               (((sPage == Page::Colours && sSelection >= 2u && sSelection <= 4u) ||
                 (sPage == Page::Timing && sSelection >= 4u) ||
                 (sPage == Page::States && sSelection == 5u)) && navigation(buttons, kDPadRight))) {
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

void draw(void *directPrint, void *xfb) {
    if (!sOpen) return;
    if (LMTimer::menuOpen()) { LMTimer::drawMenu(directPrint, xfb); return; }
    if (sNameAction != NameAction::None) { drawNameKeyboard(directPrint, xfb); return; }
    auto text = reinterpret_cast<DirectPrintDrawStringFn>(
        kDirectPrintDrawStringAddress);
    if (sResetRecording) {
        char chord[32];
        LMResetBind::text(sResetRecorder.chord, chord);
        LMDraw::fillBox(xfb, 640u, 480u, 12, 65, 296, 112, 0x0C171Eu);
        text(directPrint, 22u, 74u, "RECORD RESET ROOM COMBO");
        text(directPrint, 22u, 94u, "%s", sResetRecorder.releaseFirst ? "Release all buttons first." : "Hold 2-4 buttons together, then release.");
        text(directPrint, 22u, 111u, "Combo: %s", chord);
        text(directPrint, 22u, 128u, "Use L/R/Z/A/B/X/Y or D-Up.");
        text(directPrint, 22u, 144u, "L/R: full click. B alone: cancel.");
        text(directPrint, 22u, 160u, "No Start or D-left/right/down.");
        LMDraw::flush(xfb, 640u, 480u, 65, 112);
        return;
    }
    const u16 panelHeight = sArchiveBrowser ? kArchivePanelHeight : kMenuHeight;
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 308, panelHeight, 0x0C171Eu);
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 308, 17, 0x24543Fu);
    LMDraw::fillBox(xfb, 640u, 480u, 6, kMenuTop, 2, panelHeight, 0x8FDCABu);
    const u32 page = static_cast<u32>(sPage);
    if (sArchiveBrowser) {
        text(directPrint, 13u, kMenuTop + 5u, "SD ARCHIVES  /  SLOT %lu", LMState::selectedSlot() + 1u);
        text(directPrint, 238u, kMenuTop + 5u, "B: Back");
        const u32 count = LMState::catalogCount();
        if (count && sArchiveSelection < count)
            LMDraw::fillBox(xfb, 640u, 480u, 10, kMenuTop + 23u + sArchiveSelection * 16u,
                           300, 16, 0x284D46u);
        for (u32 i = 0u; i < count && i < 8u; ++i) {
            const char *name = LMState::catalogName(i);
            if (name && name[0])
                text(directPrint, 13u, kMenuTop + 25u + i * 16u, "%s %.31s",
                     i == sArchiveSelection ? ">" : " ", name);
            else text(directPrint, 13u, kMenuTop + 25u + i * 16u, "%s Archive %08lu",
                      i == sArchiveSelection ? ">" : " ", LMState::catalogId(i));
            text(directPrint, 25u, kMenuTop + 33u + i * 16u, "%08lu  %5luK  %s",
                 LMState::catalogId(i), LMState::catalogBytes(i) >> 10, LMState::catalogEntryText(i));
        }
        if (!count && !LMState::catalogBusy())
            text(directPrint, 13u, kMenuTop + 36u, "No archives on this page.");
        if (sConfirmTimer)
            text(directPrint, 13u, kMenuTop + 156u, "A again: replace memory slot %lu", LMState::selectedSlot() + 1u);
        else text(directPrint, 13u, kMenuTop + 156u, "%.48s",
                  sNoticeTimer && sNotice ? sNotice : LMState::storageBusy() ?
                  LMState::storageText() : LMState::catalogText());
        text(directPrint, 13u, kMenuTop + 168u, "A: Import  X: Rename  Z: Delete  Y: Refresh");
        text(directPrint, 13u, kMenuTop + 180u, "Left: previous  Right: next  %s",
             LMState::catalogHasMore() ? "MORE >" : "END");
        text(directPrint, 13u, kMenuTop + 192u, "Import fills memory. Load restores gameplay.");
        if (sDeletePrompt.active || sDeletePending) {
            LMDraw::fillBox(xfb, 640u, 480u, 25, 80, 270, 106, 0x101E26u);
            LMDraw::fillBox(xfb, 640u, 480u, 25, 80, 270, 2, 0xF29880u);
            text(directPrint, 35u, 92u, "%s", sDeletePending ? "DELETING SD STATE..." : "PERMANENTLY DELETE SD STATE?");
            text(directPrint, 35u, 110u, "%.31s", sDeletePrompt.name[0] ? sDeletePrompt.name : "Unnamed state");
            text(directPrint, 35u, 123u, "Archive %08lu", sDeletePrompt.id);
            text(directPrint, 35u, 142u, "Memory slot stays unchanged.");
            text(directPrint, 35u, 155u, "%s", sDeletePending ?
                 "Please wait. Do not remove the SD." : "This cannot be undone.");
            if (!sDeletePending) text(directPrint, 35u, 174u, "A: Delete permanently   B: Cancel");
        }
        LMDraw::flush(xfb, 640u, 480u, kMenuTop, kArchivePanelHeight);
        return;
    }
    if (sHome) {
        text(directPrint, 13u, kMenuTop + 5u, "%s", LM_BRANDING_NAME);
        for (u32 i = 0u; i < kPageCount; ++i) {
            const u16 x = 13u + (i % 3u) * 99u, y = kMenuTop + 25u + (i / 3u) * 34u;
            LMDraw::fillBox(xfb, 640u, 480u, x, y, 95, 29,
                            i == page ? 0x31734Fu : 0x1A2E35u);
            if (i == page) LMDraw::fillBox(xfb, 640u, 480u, x, y, 2, 29, 0xB0F1C8u);
            text(directPrint, x + 5u, y + 11u, "%s", kPageNames[i]);
        }
        text(directPrint, 13u, kMenuTop + 129u, "%s", kPageHelp[page]);
        text(directPrint, 13u, kMenuTop + 139u, "%s", LMPreferences::statusText());
        text(directPrint, 13u, kMenuTop + 151u, "Stick / D-pad: choose    A: open    B: close");
        text(directPrint, 13u, kMenuTop + 164u, "%s", LM_BRANDING_VERSION);
        LMDraw::flush(xfb, 640u, 480u, kMenuTop, kMenuHeight);
        return;
    }
    text(directPrint, 13u, kMenuTop + 5u, "%s", kPageNames[page]);
    text(directPrint, 205u, kMenuTop + 5u, "B: Categories");
    u32 firstWarp = sSelection > 3u ? sSelection - 3u : 0u;
    if (sPage == Page::Warps && LMWarp::count() > 7u && firstWarp > LMWarp::count() - 7u)
        firstWarp = LMWarp::count() - 7u;
    const u32 visibleRow = sPage == Page::Warps ? sSelection - firstWarp : sSelection;
    LMDraw::fillBox(xfb, 640u, 480u, 10, kMenuTop + 23u + visibleRow * 11u,
                    300, 11, 0x284D46u);

    if (sPage == Page::States) {
        const u32 slot = LMState::selectedSlot();
        text(directPrint, 13u, kMenuTop + 25u, "%s Memory slot  < %lu / %lu >   %s  %luK",
             selectionMark(0u), slot + 1u, LMState::slotCount(), LMState::slotHasState(slot) ? "SAVED" : "EMPTY", LMState::slotKiB(slot));
        text(directPrint, 13u, kMenuTop + 36u, "%s Save here", selectionMark(1u));
        text(directPrint, 13u, kMenuTop + 47u, "%s Load this slot", selectionMark(2u));
        text(directPrint, 13u, kMenuTop + 58u, "%s Export to a new SD file", selectionMark(3u));
        text(directPrint, 13u, kMenuTop + 69u, "%s Browse SD archives...", selectionMark(4u));
        text(directPrint, 13u, kMenuTop + 80u, "%s Manual ID  < %08lu >   A: latest", selectionMark(5u), sArchiveId);
        text(directPrint, 13u, kMenuTop + 91u, "%s Import manual ID", selectionMark(6u));
        text(directPrint, 13u, kMenuTop + 102u, "%s Clear this memory slot", selectionMark(7u));
        text(directPrint, 13u, kMenuTop + 117u, "Gate %s %08lX  SD: browse to import", LMState::gateText(), LMState::gateValue());
        text(directPrint, 13u, kMenuTop + 128u, "%s", sConfirmTimer ? "A again confirms replacing this slot" : LMState::storageText());
    } else if (sPage == Page::Warps) {
        const u32 count = LMWarp::count();
        for (u32 i = 0u; i < 7u && firstWarp + i < count; ++i) {
            const u32 index = firstWarp + i;
            text(directPrint, 13u, kMenuTop + 25u + i * 11u, "%s %02lu  %s", selectionMark(index), index + 1u, LMWarp::name(index));
        }
        text(directPrint, 13u, kMenuTop + 110u, "Map %lu / Room %lu  |  %lu of %lu", LMWarp::map(sSelection), LMWarp::room(sSelection), sSelection + 1u, count);
        text(directPrint, 13u, kMenuTop + 121u, "X: Boo-safe %s  %s", onOff(sBooSafe),
             LMWarp::hasBooSafePoint(sSelection) ? "Alternate entry available" : "Uses standard entry");
    } else if (sPage == Page::Display || sPage == Page::Timing) {
        LMTools::drawPage(directPrint, sPage == Page::Timing, sSelection, kMenuTop + 25u);
    } else if (sPage == Page::Colours) {
        const u32 rgb = LMColour::rgb();
        text(directPrint, 13u, kMenuTop + 25u, "%s Custom colour   %s", selectionMark(0u), onOff(LMColour::enabled()));
        text(directPrint, 13u, kMenuTop + 36u, "%s Preset        < %s >", selectionMark(1u), sColourPreset < LMColour::presetCount() ? LMColour::presetName(sColourPreset) : "CUSTOM RGB");
        text(directPrint, 13u, kMenuTop + 47u, "%s Red           < %3lu >", selectionMark(2u), rgb >> 16u);
        text(directPrint, 13u, kMenuTop + 58u, "%s Green         < %3lu >", selectionMark(3u), (rgb >> 8u) & 255u);
        text(directPrint, 13u, kMenuTop + 69u, "%s Blue          < %3lu >", selectionMark(4u), rgb & 255u);
        text(directPrint, 13u, kMenuTop + 80u, "%s Restore original clothing", selectionMark(5u));
        LMDraw::fillBox(xfb, 640u, 480u, 265, kMenuTop + 45u, 35, 30, rgb);
        text(directPrint, 13u, kMenuTop + 110u, "%s", LMColour::statusText());
        text(directPrint, 13u, kMenuTop + 121u, "Shirt + cap. Hold left/right to adjust RGB.");
    } else if (sPage == Page::Settings && missionReady()) {
        const u32 count = booCount();
        const bool booSpawn = flag(22u) && flag(73u) && flag(75u);
        text(directPrint, 13u, kMenuTop + 25u, "%s Mansion       %s",
             selectionMark(0u),
             readWord(kMansionModeAddress) ? "HIDDEN" : "NORMAL");
        text(directPrint, 13u, kMenuTop + 36u, "%s Boo spawning  %s",
             selectionMark(1u), onOff(booSpawn));
        if (count == 0xFFFFFFFFu) {
            text(directPrint, 13u, kMenuTop + 47u,
                 "%s Boo gates     NO PLAYER", selectionMark(2u));
        } else if (count >= 40u) {
            text(directPrint, 13u, kMenuTop + 47u,
                 "%s Boo gates     N/A (%lu Boos)", selectionMark(2u), count);
        } else {
            text(directPrint, 13u, kMenuTop + 47u,
                 "%s Boo gates     %s (%lu Boos)", selectionMark(2u),
                 booRequirementsDisabled(count) ? "OFF" : "ON", count);
        }
        text(directPrint, 13u, kMenuTop + 58u, "%s Blackout      %s",
             selectionMark(3u), onOff(flag(61u) != 0u));
        const u32 player = playerAddress();
        text(directPrint, 13u, kMenuTop + 69u, "%s Health        %ld/%ld",
             selectionMark(4u), player ? readSignedHalf(player + 0xFCu) : 0,
             player ? readSignedHalf(player + 0xFFCu) : 0);
        text(directPrint, 13u, kMenuTop + 80u, "%s Save game to memory card",
             selectionMark(5u));
        text(directPrint, 13u, kMenuTop + 91u, "%s Tank preset  < %s >  A: set/refill",
             selectionMark(6u), LMElements::name(sElementChoice));
        text(directPrint, 13u, kMenuTop + 110u, "World options affect progress; card save is manual.");
        if (sSelection == 6u)
            text(directPrint, 13u, kMenuTop + 121u, "Tank fuel/type only. Medals unchanged.");
    } else if (sPage == Page::Settings) {
        text(directPrint, 10u, kMenuTop + 23u, "ENTER A STABLE MANSION ROOM");
    } else if (sPage == Page::Doors && missionReady()) {
        text(directPrint, 13u, kMenuTop + 25u, "%s Fire doors       %s",
             selectionMark(0u), onOff(flag(54u) == 0u));
        text(directPrint, 13u, kMenuTop + 36u, "%s Observatory door %s",
             selectionMark(1u), onOff(flag(83u) == 0u));
        text(directPrint, 13u, kMenuTop + 47u, "%s Room traps       %s",
             selectionMark(2u), onOff(flag(70u) == 0u));
        text(directPrint, 13u, kMenuTop + 58u, "%s Unlock every door",
             selectionMark(3u));
    } else if (sPage == Page::Doors) {
        text(directPrint, 10u, kMenuTop + 23u, "ENTER A STABLE MANSION ROOM");
    } else if (sPage == Page::Plant && missionReady()) {
        u32 count;
        const PlantPreset *presets = plantPresets(sPlantArea, &count);
        if (!presets) {
            text(directPrint, 13u, kMenuTop + 25u,
                 "%s Plant preset   Area 1 N/A", selectionMark(0u));
        } else {
            text(directPrint, 13u, kMenuTop + 25u,
                 "%s Plant area %lu  %s", selectionMark(0u), sPlantArea,
                 presets[sPlantChoice % count].name);
        }
        const u32 room = currentRoomId();
        text(directPrint, 13u, kMenuTop + 36u,
             "%s Clear room %s R%lu", selectionMark(1u),
             roomClearRecipe(room) ? "READY" : "N/A",
             room == 0xFFFFFFFFu ? 999u : room);
        text(directPrint, 13u, kMenuTop + 47u, "%s Reset current room  %s", selectionMark(2u),
             LMWarp::roomReloadAvailable(room) ? "RELOAD" : "NO ENTRY POINT");
        char bind[32];
        LMResetBind::text(sResetBind, bind);
        text(directPrint, 13u, kMenuTop + 58u, "%s Reset bind  %s", selectionMark(3u), bind);
        text(directPrint, 13u, kMenuTop + 80u, "Bind: A records; Z disables. Default OFF.");
        text(directPrint, 13u, kMenuTop + 91u, "Combo resets immediately; release to re-arm.");
        text(directPrint, 13u, kMenuTop + 110u,
             "Clear/reset change progress. Press A twice.");
        text(directPrint, 13u, kMenuTop + 121u, "Reset rebuilds actors; no automatic card save.");
    } else if (sPage == Page::Plant) {
        text(directPrint, 13u, kMenuTop + 25u, "%s Plant presets: mansion only", selectionMark(0u));
        text(directPrint, 13u, kMenuTop + 36u, "%s Clear room: mansion only", selectionMark(1u));
        text(directPrint, 13u, kMenuTop + 47u, "%s Restart current boss battle", selectionMark(2u));
        char bind[32];
        LMResetBind::text(sResetBind, bind);
        text(directPrint, 13u, kMenuTop + 58u, "%s Reset bind  %s", selectionMark(3u), bind);
        text(directPrint, 13u, kMenuTop + 80u, "Bind: A records; Z disables.");
    } else {
        text(directPrint, 13u, kMenuTop + 25u, "%s Music %02lu  %s",
             selectionMark(0u), sBgmId, kBgmNames[sBgmId]);
        text(directPrint, 13u, kMenuTop + 36u, "%s Stop music",
             selectionMark(1u));
    }

    if (sNoticeTimer != 0u && sNotice) {
        text(directPrint, 13u, kMenuTop + 137u, "%s", sNotice);
    } else if (!menuReady()) {
        text(directPrint, 13u, kMenuTop + 137u, "Please wait for the game to finish loading.");
    } else if (sPage == Page::States) {
        text(directPrint, 13u, kMenuTop + 137u, "Dpad %04lX Link%lu Edge%lu Use%lu",
             LMState::hotkeyDebug(0u), LMState::hotkeyDebug(1u),
             LMState::hotkeyDebug(2u), LMState::hotkeyDebug(3u));
    } else {
        text(directPrint, 13u, kMenuTop + 137u, "%s", LMPreferences::statusText());
    }
    text(directPrint, 13u, kMenuTop + 151u,
         sPage == Page::Warps ? "Up/down: choose   Left/right: skip 10 rooms" :
                               "Up/down: choose   Left/right: change value");
    text(directPrint, 13u, kMenuTop + 164u,
         "A: apply   B: back   L/R: pages   Game runs");
    LMDraw::flush(xfb, 640u, 480u, kMenuTop, kMenuHeight);
}

bool isOpen() {
    return sOpen;
}

bool prepareRoomReload(u32 room, bool clear) {
    if (!beginAction() || currentRoomId() != room || !actorRegistryReady()) return false;
    sResetPersistenceCount = 0u;
    sResetPersistence[sResetPersistenceCount++] = kRoomPersistenceAddress + room * 2u;
    const u32 partnerIndex = readHalf(kFoyerPartnerAddress) & 255u;
    const u32 count = *reinterpret_cast<volatile u8 *>(kRoomCountAddress);
    const u32 table = readWord(kRoomTableAddress);
    if (!count || count > kRoomCountLimit || partnerIndex >= count ||
        !validMem1(table, count * kRoomEntrySize)) return false;
    const u32 packed = readWord(table + partnerIndex * kRoomEntrySize + 0x10u);
    if ((packed >> 24u) != partnerIndex || (packed & 255u) >= kRoomCountLimit) return false;
    const u32 companion = LmRoomFoyerCompanion(room, partnerIndex, count, packed);
    if (room == 2u && companion == LM_ROOM_NO_PARTNER) return false;
    if (companion != LM_ROOM_NO_PARTNER)
        sResetPersistence[sResetPersistenceCount++] = kRoomPersistenceAddress + companion * 2u;
    sFoyerLightEntry = 0u;
    if (clear && companion != LM_ROOM_NO_PARTNER) {
        for (u32 i = 0u; i < count; ++i) {
            const u32 entry = table + i * kRoomEntrySize;
            const u32 id = readWord(entry + 0x10u);
            if ((id & 255u) != 2u) continue;
            if ((id >> 24u) != i || sFoyerLightEntry) return false;
            sFoyerLightEntry = entry;
        }
        if (!sFoyerLightEntry) return false;
    }
    if (clear) {
        const RoomClearRecipe *recipe = roomClearRecipe(room);
        if (!recipe || flag(61u)) { showNotice("TURN BLACKOUT OFF FIRST"); return false; }
        for (u32 i = 0u; i < recipe->actorCount; ++i) {
            if (!actorLookupSafe(recipe->actors[i])) { showNotice("ACTOR TABLE CHANGED"); return false; }
        }
        return true;
    }
    u32 shown = 0u;
    if (room == 3u) {
        for (u32 i = 0u; i < 5u; ++i) if (flag(20u + i * 3u)) shown |= 1u << i;
    }
    if (!LmRoomResetRecipeFor(room, shown, &sResetRecipe)) return false;
    if (!sResetRecipe.darken) sResetPersistenceCount = 0u;
    return true;
}

void commitRoomReload(u32 room, bool clear) {
    if (clear) {
        const RoomClearRecipe *recipe = roomClearRecipe(room);
        for (u32 i = 0u; i < recipe->actorCount; ++i) killActor(recipe->actors[i]);
        for (u32 i = 0u; i < recipe->flagCount; ++i) setFlag(recipe->flags[i], true);
        if (!recipe->noLight) {
            // The native lower-room entry routine updates both loaded-room
            // flags AND both persistence entries, also when invoked upstairs.
            if (sFoyerLightEntry)
                reinterpret_cast<WordFn>(kRoomEntryLightAddress)(sFoyerLightEntry);
            else reinterpret_cast<VoidFn>(kRoomLightAddress)();
        }
        return;
    }
    for (u32 i = 0u; i < sResetPersistenceCount; ++i) {
        const u32 address = sResetPersistence[i];
        *reinterpret_cast<volatile u16 *>(address) = LmRoomDarkPersistence(readHalf(address));
    }
    for (u32 i = 0u; i < sResetRecipe.count; ++i)
        setFlag(sResetRecipe.edits[i].id, sResetRecipe.edits[i].value != 0u);
    for (u32 i = 0u; i < sResetRecipe.eventCount; ++i)
        *reinterpret_cast<volatile u8 *>(kEventPlayCountsAddress + sResetRecipe.events[i]) = 0u;
}

}  // namespace LMPractice

extern "C" u32 diagnosticPadRead(PADStatus *statuses) {
    const u32 connected =
        reinterpret_cast<PadReadFn>(kPadReadAddress)(statuses);
    LMPractice::filterPadRead(statuses);
    return connected;
}

#endif  // defined(SUSAMUNE_VERSION_LMJ)
