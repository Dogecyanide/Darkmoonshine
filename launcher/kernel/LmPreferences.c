#ifdef LM_PREFERENCES_HOST_TEST
#include "lm_preferences_test_shim.h"
#else
#include "LmPreferences.h"
#include "SusamuneCfg.h"
#include "string.h"
#include "alloc.h"
#include "ff_utf8.h"
#endif
#include "susamune/lm_preferences.h"

extern u32 GAME_ID;
#define INI_LIMIT 32768u
static bool Enabled;
static u32 Acknowledged;
static const char Section[] = "lm_preferences";
static const char *const Keys[LM_PREFERENCES_VALUE_COUNT] = {
    "metadata", "inputs", "lag", "r_pump", "luigi_colour", "luigi_rgb",
    "timer_visible", "timer_label", "timer_x", "timer_y", "timer_scale",
    "timer_opacity", "timer_brightness", "timer_background_rgb",
    "timer_background_opacity", "timer_padding", "timer_char_0_rgb",
    "timer_char_1_rgb", "timer_char_2_rgb", "timer_char_3_rgb", "timer_char_4_rgb",
    "timer_char_5_rgb", "timer_char_6_rgb", "timer_char_7_rgb", "timer_char_8_rgb",
    "streak_x", "streak_y", "streak_scale", "streak_opacity", "streak_brightness",
    "streak_background_rgb", "streak_background_opacity", "streak_padding",
    "streak_rgb", "timing_profile", "reference_1_delay", "reference_1_hold",
    "reference_1_tolerance", "reference_1_button", "reference_1_valid",
    "reference_2_delay", "reference_2_hold", "reference_2_tolerance",
    "reference_2_button", "reference_2_valid", "boo_safe",
    "timer_run_in_menus", "reset_room_bind"
};

static volatile struct LmPreferencesBlock *Block(void) {
#ifdef LM_PREFERENCES_HOST_TEST
    return &TestBlock;
#else
    return LM_PREFERENCES_PHYS_PTR;
#endif
}

static char *IniBuffer(void) {
#ifdef LM_PREFERENCES_HOST_TEST
    return (char *)TestAllocate(INI_LIMIT);
#else
    /* Unlike malloca(), failure here must not shut down the Wii. */
    return (char *)heap_alloc_aligned(0, INI_LIMIT, 32);
#endif
}

static bool Space(char c) { return c == ' ' || c == '\t' || c == '\r'; }
static bool Equal(const char *a, const char *end, const char *b) {
    while (a < end && *b && *a == *b) { ++a; ++b; }
    return a == end && !*b;
}
static void Trim(const char **start, const char **end) {
    while (*start < *end && Space(**start)) ++*start;
    while (*end > *start && Space((*end)[-1])) --*end;
}
static int Header(const char *start, const char *end) {
    const char *close;
    Trim(&start, &end);
    if (start == end || *start != '[') return -1;
    close = ++start;
    while (close < end && *close != ']') ++close;
    if (close == end) return 0;
    return Equal(start, close, Section) ? 1 : 0;
}
/* -1 unknown; metadata keys follow the preference indices. */
static int Key(const char *start, const char *end, const char **value) {
    const char *equals, *keyEnd;
    u32 i;
    Trim(&start, &end);
    if (start == end || *start == ';' || *start == '#') return -1;
    equals = start;
    while (equals < end && *equals != '=') ++equals;
    if (equals == end) return -1;
    keyEnd = equals;
    while (keyEnd > start && Space(keyEnd[-1])) --keyEnd;
    *value = equals + 1;
    for (i = 0; i < LM_PREFERENCES_VALUE_COUNT; ++i)
        if (Equal(start, keyEnd, Keys[i])) return (int)i;
    if (Equal(start, keyEnd, "version")) return LM_PREFERENCES_VALUE_COUNT;
    if (Equal(start, keyEnd, "checksum")) return LM_PREFERENCES_VALUE_COUNT + 1u;
    return -1;
}
static bool Number(const char *start, const char *end, u32 *value) {
    u32 result = 0, base = 10, digit;
    bool negative = false, any = false;
    const char *comment = start;
    while (comment < end && *comment != ';' && *comment != '#') ++comment;
    end = comment;
    Trim(&start, &end);
    if (start < end && *start == '-') { negative = true; ++start; }
    if (end - start >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
        base = 16; start += 2;
    }
    while (start < end) {
        char c = *start++;
        if (c >= '0' && c <= '9') digit = (u32)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (u32)(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F') digit = (u32)(c - 'A') + 10u;
        else return false;
        if (digit >= base || result > (0xFFFFFFFFu - digit) / base) return false;
        result = result * base + digit; any = true;
    }
    if (!any || (negative && result > 0x80000000u)) return false;
    *value = negative ? 0u - result : result;
    return true;
}

static void Empty(struct LmPreferencesBlock *out) {
    u32 i;
    memset(out, 0, sizeof(*out));
    out->magic = LM_PREFERENCES_MAGIC;
    out->version = LM_PREFERENCES_VERSION;
    for (i = 0; i < LM_PREFERENCES_VALUE_COUNT; ++i) out->values[i] = 0xFFFFFFFFu;
    out->checksum = LmPreferencesChecksum(out->values, 0, 0);
}

static u32 Parse(const char *text, u32 size, struct LmPreferencesBlock *out) {
    const char *line = text, *limit = text + size, *end, *value;
    bool ours = false, seen = false, versionSeen = false, checksumSeen = false;
    u32 expected = 0, number, fileVersion = 0, actual, i;
    int header, key;
    Empty(out);
    while (line < limit) {
        end = line;
        while (end < limit && *end != '\n') { if (!*end) return LM_PREFERENCES_INVALID; ++end; }
        header = Header(line, end);
        if (header >= 0) { ours = header != 0; if (ours) seen = true; }
        else if (ours && (key = Key(line, end, &value)) >= 0) {
            if (!Number(value, end, &number)) return LM_PREFERENCES_INVALID;
            if (key == LM_PREFERENCES_VALUE_COUNT) {
                if (versionSeen || (number != LM_PREFERENCES_VERSION &&
                    number != LM_PREFERENCES_LEGACY_VERSION)) return LM_PREFERENCES_INVALID;
                versionSeen = true; fileVersion = number;
            } else if (key == LM_PREFERENCES_VALUE_COUNT + 1u) {
                if (checksumSeen) return LM_PREFERENCES_INVALID;
                checksumSeen = true; expected = number;
            } else {
                if (LmPreferencesHas(out->presentLo, out->presentHi, (u32)key)) return LM_PREFERENCES_INVALID;
                out->values[key] = number;
                if (key < 32) out->presentLo |= 1u << key;
                else out->presentHi |= 1u << (key - 32);
            }
        }
        line = end < limit ? end + 1 : limit;
    }
    if (!seen) return LM_PREFERENCES_NO_FILE;
    if (!versionSeen || !checksumSeen) return LM_PREFERENCES_INVALID;
    if (fileVersion == LM_PREFERENCES_LEGACY_VERSION) {
        /* Authenticate exactly v1's 46 values plus two reserved zero words.
         * New keys in a v1 section are invalid, even with a forged v1 CRC. */
        if (out->presentHi & ~LM_PREFERENCES_LEGACY_PRESENT_HI)
            return LM_PREFERENCES_INVALID;
        for (i = LM_PREFERENCES_LEGACY_VALUE_COUNT; i < LM_PREFERENCES_WORD_COUNT; ++i)
            out->values[i] = 0u;
        actual = LmPreferencesChecksumForVersion(out->values, out->presentLo,
                                               out->presentHi, fileVersion);
        for (i = LM_PREFERENCES_LEGACY_VALUE_COUNT; i < LM_PREFERENCES_WORD_COUNT; ++i)
            out->values[i] = 0xFFFFFFFFu;
    } else actual = LmPreferencesChecksum(out->values, out->presentLo, out->presentHi);
    if (expected != actual) return LM_PREFERENCES_INVALID;
    /* Only canonical v2 blocks cross the ARM/PPC boundary; absent new settings
     * retain their runtime defaults until the next explicit preference save. */
    out->checksum = LmPreferencesChecksum(out->values, out->presentLo, out->presentHi);
    return LmPreferencesValid(out) ? LM_PREFERENCES_OK : LM_PREFERENCES_INVALID;
}

static u32 Read(const char *path, char *buffer, u32 *size) {
    FIL file;
    UINT got = 0;
    u32 expected;
    FRESULT result = f_open_char(&file, path, FA_READ), closed;
    *size = 0;
    if (result == FR_NO_FILE) return LM_PREFERENCES_NO_FILE;
    if (result != FR_OK) return LM_PREFERENCES_IO_ERROR;
    expected = f_size(&file);
    if (expected > INI_LIMIT) { f_close(&file); return LM_PREFERENCES_INVALID; }
    result = f_read(&file, buffer, INI_LIMIT, &got);
    closed = f_close(&file);
    if (result != FR_OK || closed != FR_OK || got != expected) return LM_PREFERENCES_IO_ERROR;
    *size = got;
    return LM_PREFERENCES_OK;
}
static FRESULT Rename(const char *source, const char *destination) {
    WCHAR a[128], b[128];
    u32 i;
    for (i = 0; i < 127u && source[i]; ++i) a[i] = (u8)source[i];
    if (source[i]) return FR_INVALID_NAME;
    a[i] = 0;
    for (i = 0; i < 127u && destination[i]; ++i) b[i] = (u8)destination[i];
    if (destination[i]) return FR_INVALID_NAME;
    b[i] = 0;
    return f_rename(a, b);
}
static bool Emit(FIL *file, const char *data, u32 size, u32 *total) {
    UINT written = 0;
    if (size > INI_LIMIT - *total) return false;
    if (f_write(file, data, size, &written) != FR_OK || written != size) return false;
    *total += size;
    return true;
}
static bool EmitPreferences(FIL *file, const struct LmPreferencesBlock *request,
                            u32 *total) {
    char formatted[96];
    u32 i;
    _sprintf(formatted, "version = %u\r\n", LM_PREFERENCES_VERSION);
    if (!Emit(file, formatted, strlen(formatted), total)) return false;
    for (i = 0; i < LM_PREFERENCES_VALUE_COUNT; ++i) {
        if (!LmPreferencesHas(request->presentLo, request->presentHi, i)) continue;
        _sprintf(formatted, "%s = 0x%08X\r\n", Keys[i], request->values[i]);
        if (!Emit(file, formatted, strlen(formatted), total)) return false;
    }
    _sprintf(formatted, "checksum = 0x%08X\r\n", request->checksum);
    return Emit(file, formatted, strlen(formatted), total);
}

static u32 Save(const struct LmPreferencesBlock *request) {
    char *buffer, temporary[128], backup[128];
    const char *path = SusamuneCfgIniPath(), *line, *limit, *end, *value;
    u32 size = 0, readStatus, total = 0;
    bool ours = false, okay = true, original, emitted = false;
    int header;
    FIL file;
    FRESULT result;
    if (strlen(path) > 112u) return LM_PREFERENCES_IO_ERROR;
    _sprintf(temporary, "%s.lm.tmp", path);
    _sprintf(backup, "%s.lm.bak", path);
    buffer = IniBuffer();
    if (!buffer) return LM_PREFERENCES_IO_ERROR;
    readStatus = Read(path, buffer, &size);
    original = readStatus == LM_PREFERENCES_OK;
    if (readStatus == LM_PREFERENCES_NO_FILE) {
        readStatus = Read(backup, buffer, &size);
        if (readStatus == LM_PREFERENCES_NO_FILE) readStatus = LM_PREFERENCES_OK;
    }
    if (readStatus != LM_PREFERENCES_OK) { free(buffer); return readStatus; }
    result = f_open_char(&file, temporary, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) { free(buffer); return LM_PREFERENCES_IO_ERROR; }
    line = buffer; limit = buffer + size;
    while (okay && line < limit) {
        end = line;
        while (end < limit && *end != '\n') ++end;
        header = Header(line, end);
        if (header >= 0) ours = header != 0;
        if (!(ours && header < 0 && Key(line, end, &value) >= 0))
            okay = Emit(&file, line, (u32)(end - line) + (end < limit), &total);
        if (okay && ours && header >= 0 && !emitted) {
            if (end == limit) okay = Emit(&file, "\r\n", 2u, &total);
            if (okay) okay = EmitPreferences(&file, request, &total);
            emitted = true;
        }
        line = end < limit ? end + 1 : limit;
    }
    if (okay && !emitted) {
        okay = Emit(&file, "\r\n[lm_preferences]\r\n", 20u, &total);
        if (okay) okay = EmitPreferences(&file, request, &total);
    }
    if (okay) okay = f_sync(&file) == FR_OK;
    result = f_close(&file);
    free(buffer);
    if (!okay || result != FR_OK) return LM_PREFERENCES_IO_ERROR;
    if (original) {
        result = f_unlink_char(backup);
        if (result != FR_OK && result != FR_NO_FILE) return LM_PREFERENCES_IO_ERROR;
        if (Rename(path, backup) != FR_OK) return LM_PREFERENCES_IO_ERROR;
    }
    if (Rename(temporary, path) != FR_OK) {
        if (original) Rename(backup, path);
        return LM_PREFERENCES_IO_ERROR;
    }
    return LM_PREFERENCES_OK;
}

void LmPreferencesInit(void) {
    volatile struct LmPreferencesBlock *block = Block();
    struct LmPreferencesBlock loaded;
    char *buffer, backup[128];
    u32 size, status = LM_PREFERENCES_UNAVAILABLE;
    Enabled = GAME_ID == 0x474C4D4Au;
    Acknowledged = 0;
    if (!Enabled) return;
    Empty(&loaded);
    Enabled = SusamuneCfgStorageAvailable();
    if (Enabled) {
        buffer = IniBuffer();
        status = buffer ? Read(SusamuneCfgIniPath(), buffer, &size) : LM_PREFERENCES_IO_ERROR;
        if (status == LM_PREFERENCES_OK) status = Parse(buffer, size, &loaded);
        if (buffer && status == LM_PREFERENCES_NO_FILE && strlen(SusamuneCfgIniPath()) <= 112u) {
            _sprintf(backup, "%s.lm.bak", SusamuneCfgIniPath());
            status = Read(backup, buffer, &size);
            if (status == LM_PREFERENCES_OK) status = Parse(buffer, size, &loaded);
        }
        if (buffer) free(buffer);
        if (status != LM_PREFERENCES_OK) Empty(&loaded);
    }
    loaded.status = status;
    loaded.ready = Enabled ? 1u : 0u;
    memcpy((void *)block, &loaded, sizeof(loaded));
    sync_after_write((void *)block, sizeof(*block));
}

bool LmPreferencesPending(void) {
    volatile struct LmPreferencesBlock *block = Block();
    if (!Enabled) return false;
    sync_before_read((void *)block, 32u);
    return block->requestSeq != Acknowledged;
}

void LmPreferencesService(void) {
    volatile struct LmPreferencesBlock *block = Block();
    struct LmPreferencesBlock request;
    u32 sequence, status;
    if (!Enabled) return;
    sync_before_read((void *)block, 32u);
    sync_before_read((void *)block->values, sizeof(block->values));
    memcpy(&request, (const void *)block, sizeof(request));
    sequence = request.requestSeq;
    status = LmPreferencesValid(&request) ? Save(&request) : LM_PREFERENCES_INVALID;
    Acknowledged = sequence;
    block->ackSeq = sequence;
    block->status = status;
    sync_after_write((void *)&block->ackSeq, 32u);
}
