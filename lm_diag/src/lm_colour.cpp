#if defined(SUSAMUNE_VERSION_LMJ)

#include "lm_colour.hxx"
#include "lm_state.hxx"
#include "lm_warp.hxx"
#include "susamune/lm_colour_codec.h"
#include "susamune/lm_colour_model.h"

namespace {

constexpr u32 kModelDescriptor = 0x803435ACu;
constexpr u32 kGameHeap = 0x804A0B98u;
constexpr u32 kSystemHeap = 0x804A0B94u;
constexpr u32 kTextureSize = LM_COLOUR_TEXTURE_BYTES;
constexpr u32 kTextureCrc[] = {0x65714408u, 0x4C866065u};
constexpr u32 kGetPlayer = 0x800E7E7Cu;
constexpr u32 kDCStoreRange = 0x801D5E58u;
constexpr u32 kGXInvalidateTexAll = 0x801F1C10u;

struct Preset { const char *name; u32 rgb; };
constexpr Preset kPresets[] = {
    {"ORIGINAL", 0xFFFFFFu},
    {"RED", 0xFF3030u},
    {"BLUE", 0x3060FFu},
    {"CYAN", 0x20FFFFu},
    {"PURPLE", 0xC040FFu},
    {"PINK", 0xFF70C0u},
    {"YELLOW", 0xFFE020u},
    {"ORANGE", 0xFF8020u},
    {"WHITE", 0xFFFFFFu},
    {"BLACK", 0x000000u},
};

alignas(32) u32 sOriginal[2u * kTextureSize / 4u];
bool sOriginalReady;
bool sEnabled;
bool sDirty;
u32 sRgb = 0x30FFFFu;
u32 sModel;
u32 sLoadRevision;
const char *sStatus = "ORIGINAL";

using GetPlayerFn = void *(*)(u32);
using CacheFn = void (*)(const void *, u32);
using VoidFn = void (*)();

u32 read(u32 address) { return *reinterpret_cast<const volatile u32 *>(address); }
bool range(u32 address, u32 size) {
    return address >= 0x80000000u && size <= 0x01800000u &&
           address <= 0x81800000u - size;
}

bool heapRange(u32 global, u32 address, u32 size) {
    const u32 heap = read(global);
    if (!range(heap, 0x38u) || (heap & 3u)) return false;
    const u32 start = read(heap + 0x30u), end = read(heap + 0x34u);
    return start <= end && range(start, end - start) &&
           size <= end - start && address >= start && address <= end - size;
}

bool gameRange(u32 address, u32 size) { return heapRange(kGameHeap, address, size); }
unsigned int modelRead(void *, unsigned int address) { return read(address); }

u32 crc32(const u8 *data, u32 size) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0u; i < size; ++i) {
        crc ^= data[i];
        for (u32 j = 0u; j < 8u; ++j) {
            crc = (crc >> 1u) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool captureOriginal(const LmColourModelView &view) {
    for (u32 i = 0u; i < 2u; ++i) {
        const u8 *pixels = reinterpret_cast<const u8 *>(view.texture[i] + 32u);
        if (crc32(pixels, kTextureSize) != kTextureCrc[i]) return false;
    }
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 *pixels = reinterpret_cast<const u32 *>(view.texture[i] + 32u);
        for (u32 j = 0u; j < kTextureSize / 4u; ++j) {
            sOriginal[i * (kTextureSize / 4u) + j] = pixels[j];
        }
    }
    sOriginalReady = true;
    return true;
}

}  // namespace

namespace LMColour {

bool enabled() { return sEnabled; }
void setEnabled(bool enabled) { sEnabled = enabled; sDirty = true; }
u32 rgb() { return sRgb; }
void setRgb(u32 value) { sRgb = value & 0xFFFFFFu; sEnabled = true; sDirty = true; }
u32 presetCount() { return sizeof(kPresets) / sizeof(kPresets[0]); }
const char *presetName(u32 index) { return index < presetCount() ? kPresets[index].name : "INVALID"; }
void applyPreset(u32 index) {
    if (index >= presetCount()) return;
    if (index == 0u) setEnabled(false);
    else setRgb(kPresets[index].rgb);
}
const char *statusText() { return sStatus; }

void tick() {
    if (!LMState::readyForAction() || LMWarp::active()) {
        sModel = 0u;
        return;
    }
    const u32 scene = read(0x804A0C20u), mission = read(0x804A17C8u);
    if (scene != 2u || read(0x80398A44u) != scene || !gameRange(mission, 0x24u)) return;
    const u32 player = reinterpret_cast<u32>(reinterpret_cast<GetPlayerFn>(kGetPlayer)(0u));
    if (!gameRange(player, 0x1000u)) return;
    const u32 model = read(kModelDescriptor + 8u);
    LmColourModelView view;
    if (read(kModelDescriptor + 0x30u) != 3u ||
        !LmColourResolveModel(nullptr, modelRead, read(kSystemHeap), model, &view)) {
        sModel = 0u;
        sStatus = sEnabled ? "COLOUR: MODEL UNAVAILABLE" : "ORIGINAL";
        return;
    }
    if (!sOriginalReady && !captureOriginal(view)) {
        sStatus = "COLOUR: RETAIL TEXTURE CHECK FAILED";
        return;
    }
    const u32 revision = LMState::loadRevision();
    if (!sDirty && model == sModel && revision == sLoadRevision) return;
    for (u32 i = 0u; i < 2u; ++i) {
        u8 *pixels = reinterpret_cast<u8 *>(view.texture[i] + 32u);
        const u8 *original = reinterpret_cast<const u8 *>(&sOriginal[i * (kTextureSize / 4u)]);
        if (sEnabled) {
            for (u32 j = 0u; j < kTextureSize; j += 8u) {
                LmColourBlock(pixels + j, original + j, sRgb >> 16u,
                              (sRgb >> 8u) & 255u, sRgb & 255u);
            }
        } else {
            for (u32 j = 0u; j < kTextureSize / 4u; ++j) {
                reinterpret_cast<u32 *>(pixels)[j] = reinterpret_cast<const u32 *>(original)[j];
            }
        }
        reinterpret_cast<CacheFn>(kDCStoreRange)(pixels, kTextureSize);
    }
    reinterpret_cast<VoidFn>(kGXInvalidateTexAll)();
    sModel = model;
    sLoadRevision = revision;
    sDirty = false;
    sStatus = sEnabled ? "COLOUR APPLIED" : "ORIGINAL";
}

}  // namespace LMColour

#endif
