/* The test inserts unmodified journal functions from SusamuneCrash.c. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "susamune/lm_crash_telemetry.h"
#include "susamune/mod_bin.h"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef unsigned int UINT;
#define FR_OK 0
#define FR_DISK_ERR 1
#define FR_EXIST 2
#define FA_READ 1
#define FA_OPEN_EXISTING 0
#define FA_WRITE 2
#define FA_CREATE_ALWAYS 4
#define _sprintf sprintf
#define SUSAMUNE_LM_DUMP_SLOTS 8u
#define SUSAMUNE_LM_DUMP_MAGIC 0x4C4D4450u
#define SUSAMUNE_LM_DUMP_LEGACY_VERSION 1u
#define SUSAMUNE_LM_DUMP_VERSION 2u
#define SUSAMUNE_LM_SAVE_COMPLETE_PHASE 0x7Fu
#define SUSAMUNE_LM_LOAD_START_PHASE 0x01u
typedef struct { unsigned int slot, position; } FIL;
typedef struct { unsigned char data[4096]; unsigned int size, exists; } TestFile;
static TestFile Files[SUSAMUNE_LM_DUMP_SLOTS];
static unsigned int Writes, FailWrite;
static char LmDumpPaths[SUSAMUNE_LM_DUMP_SLOTS][64], LmDumpDirectory[32];
static u32 LmDumpGenerations[SUSAMUNE_LM_DUMP_SLOTS];
static u32 LmDumpGeneration, LmDumpModCrc, GAME_ID;
static bool CrashEnabled, LmDumpEnabled, LmDumpOpen;
static FIL LmDumpFile;
static const char *SusamuneCfgStoragePrefix(void) { return "sd:"; }
static int f_mkdir_char(const char *path) { (void)path; return FR_EXIST; }
static int f_open_char(FIL *file, const char *path, int mode) {
    const char *name = strrchr(path, '/');
    unsigned int slot;
    if (!name) return FR_DISK_ERR;
    ++name;
    if (strncmp(name, "lm_attempt_", 11) || name[11] < 'a' || name[11] > 'h' ||
        strcmp(name + 12, ".bin")) return FR_DISK_ERR;
    slot = (unsigned int)(name[11] - 'a');
    if (mode & FA_CREATE_ALWAYS) { Files[slot].size = 0u; Files[slot].exists = 1u; }
    else if (!Files[slot].exists) return FR_DISK_ERR;
    file->slot = slot; file->position = 0u;
    return FR_OK;
}
static int f_read(FIL *file, void *out, UINT size, UINT *read) {
    TestFile *source = &Files[file->slot];
    *read = source->size - file->position;
    if (*read > size) *read = size;
    memcpy(out, source->data + file->position, *read);
    file->position += *read;
    return FR_OK;
}
static int f_write(FIL *file, const void *data, UINT size, UINT *wrote) {
    TestFile *target = &Files[file->slot];
    *wrote = 0u;
    if (++Writes == FailWrite || size > sizeof(target->data) - file->position)
        return FR_DISK_ERR;
    memcpy(target->data + file->position, data, size);
    file->position += size; target->size = file->position; *wrote = size;
    return FR_OK;
}
static int f_close(FIL *file) { (void)file; return FR_OK; }
static int f_sync(FIL *file) { (void)file; return FR_OK; }

#include "lm_dump_journal_production.inc"

static struct SusamunePhaseTrace TestTrace(void) {
    struct SusamunePhaseTrace trace;
    memset(&trace, 0, sizeof(trace));
    trace.magic = SUSAMUNE_PHASE_TRACE_MAGIC;
    trace.sequenceBegin = trace.sequenceEnd = 2u;
    trace.action = SUSAMUNE_PHASE_ACTION_SAVE;
    trace.phase = SUSAMUNE_LM_SAVE_COMPLETE_PHASE;
    trace.phaseInverse = ~trace.phase;
    return trace;
}
void journal_reset(void) {
    memset(Files, 0, sizeof(Files));
    Writes = FailWrite = 0u; LmDumpOpen = false;
    CrashEnabled = true; GAME_ID = SUSAMUNE_MOD_GAME_ID_LMJ;
    InitLmDump(0x12345678u, true);
}
void journal_seed(unsigned int slot, unsigned int generation, unsigned int valid) {
    struct SusamuneLmDumpHeader h;
    const struct SusamunePhaseTrace trace = TestTrace();
    if (slot >= SUSAMUNE_LM_DUMP_SLOTS) return;
    memset(&h, 0, sizeof(h));
    h.magic = SUSAMUNE_LM_DUMP_MAGIC; h.version = SUSAMUNE_LM_DUMP_VERSION;
    h.headerSize = sizeof(h); h.generation = generation;
    h.generationInverse = valid ? ~generation : generation;
    h.gameId = SUSAMUNE_MOD_GAME_ID_LMJ; h.recordSize = sizeof(trace);
    h.saveSequence = trace.sequenceBegin;
    memcpy(Files[slot].data, &h, sizeof(h));
    memcpy(Files[slot].data + sizeof(h), &trace, sizeof(trace));
    Files[slot].size = sizeof(h) + sizeof(trace); Files[slot].exists = 1u;
}
void journal_restart(void) { CloseLmDump(); InitLmDump(0x12345678u, true); }
void journal_save(void) {
    const struct SusamunePhaseTrace trace = TestTrace();
    RecordLmDump(&trace);
}
void journal_phase(unsigned int action, unsigned int phase) {
    struct SusamunePhaseTrace trace = TestTrace();
    trace.action = action; trace.phase = phase; trace.phaseInverse = ~phase;
    trace.sequenceBegin = trace.sequenceEnd = (Writes + 1u) * 2u;
    RecordLmDump(&trace);
}
void journal_seed_anchor(unsigned int slot, unsigned int version, unsigned int action) {
    struct SusamuneLmDumpHeader *h = (void *)Files[slot].data;
    struct SusamunePhaseTrace *trace = (void *)(Files[slot].data + sizeof(*h));
    h->version = version; trace->action = action;
    trace->phase = action == 1u ? SUSAMUNE_LM_SAVE_COMPLETE_PHASE : SUSAMUNE_LM_LOAD_START_PHASE;
    trace->phaseInverse = ~trace->phase;
}
unsigned int journal_first_action(unsigned int slot) {
    struct SusamunePhaseTrace trace;
    memcpy(&trace, Files[slot].data + sizeof(struct SusamuneLmDumpHeader), sizeof(trace));
    return trace.action;
}
unsigned int journal_records(unsigned int slot) {
    return Files[slot].size >= sizeof(struct SusamuneLmDumpHeader) ?
        (Files[slot].size - sizeof(struct SusamuneLmDumpHeader)) / sizeof(struct SusamunePhaseTrace) : 0u;
}
unsigned int journal_generation(unsigned int slot) {
    struct SusamuneLmDumpHeader h;
    return slot < SUSAMUNE_LM_DUMP_SLOTS && ReadLmDumpHeader(LmDumpPaths[slot], &h) ?
        h.generation : 0u;
}
unsigned int journal_recovered(void) { return LmDumpGeneration; }
unsigned int journal_writes(void) { return Writes; }
void journal_fail_write(unsigned int nth) { Writes = 0u; FailWrite = nth; }
