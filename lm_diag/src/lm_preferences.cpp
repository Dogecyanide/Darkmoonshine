#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_preferences.hxx"
#include "susamune/lm_preferences.h"

namespace LMTools {
void writePreferences(unsigned int *);
void readPreferences(const unsigned int *, unsigned int, unsigned int);
}
namespace LMTimer {
void writePreferences(unsigned int *);
void readPreferences(const unsigned int *, unsigned int, unsigned int);
}
namespace LMPractice {
bool isOpen();
void writePreferences(unsigned int *);
void readPreferences(const unsigned int *, unsigned int, unsigned int);
}

#if !IS_EMULATOR
namespace {
#if defined(LM_PREFERENCES_HOST_TEST)
extern volatile LmPreferencesBlock *lmPreferencesTestBlock();
extern void lmPreferencesTestInvalidate(void *, unsigned int);
extern void lmPreferencesTestStore(void *, unsigned int);
extern void lmPreferencesTestBarrier();
volatile LmPreferencesBlock *mailbox() { return lmPreferencesTestBlock(); }
void invalidate(void *p, unsigned int n) { lmPreferencesTestInvalidate(p,n); }
void store(void *p, unsigned int n) { lmPreferencesTestStore(p,n); }
void barrier() { lmPreferencesTestBarrier(); }
#else
volatile LmPreferencesBlock *mailbox() { return LM_PREFERENCES_PPC_PTR; }
typedef void (*CacheRangeFn)(void *, unsigned int);
void invalidate(void *p, unsigned int n) { reinterpret_cast<CacheRangeFn>(0x801D5DF4u)(p,n); }
void store(void *p, unsigned int n) { reinterpret_cast<CacheRangeFn>(0x801D5E58u)(p,n); }
void barrier() { asm volatile("sync" ::: "memory"); }
#endif
bool sInitialized, sCapturedDefaults, sPending, sQueued, sNeedsDefaults, sWasOpen;
unsigned int sSequence, sBaseline[LM_PREFERENCES_WORD_COUNT], sDesired[LM_PREFERENCES_WORD_COUNT], sSentChecksum;
const char *sText="SETTINGS: WAITING FOR SD";

void capture(unsigned int *values) {
    for (unsigned int i=0;i<LM_PREFERENCES_WORD_COUNT;++i) values[i]=0u;
    LMTools::writePreferences(values);
    LMTimer::writePreferences(values);
    LMPractice::writePreferences(values);
}
void apply(const unsigned int *values, unsigned int low, unsigned int high) {
    LMTools::readPreferences(values,low,high);
    LMTimer::readPreferences(values,low,high);
    LMPractice::readPreferences(values,low,high);
}
bool different(const unsigned int *a, const unsigned int *b) {
    for (unsigned int i=0;i<LM_PREFERENCES_VALUE_COUNT;++i) if(a[i]!=b[i]) return true;
    return false;
}
const char *failure(unsigned int status) {
    return status==LM_PREFERENCES_INVALID ? "SETTINGS: INVALID FILE" :
           status==LM_PREFERENCES_UNAVAILABLE ? "SETTINGS: SD UNAVAILABLE" :
           "SETTINGS: SD WRITE FAILED";
}

bool initialize() {
    if(sInitialized) return true;
    volatile LmPreferencesBlock *m=mailbox();
    invalidate(const_cast<LmPreferencesBlock *>(m),32u);
    invalidate(const_cast<unsigned int *>(&m->ackSeq),32u);
    if(m->magic!=LM_PREFERENCES_MAGIC || m->version!=LM_PREFERENCES_VERSION || m->ready!=1u) return false;
    if(m->ackSeq!=m->requestSeq) { sText="SETTINGS: FINISHING SD WRITE"; return false; }
    if(LMPractice::isOpen()) return false;
    invalidate(const_cast<unsigned int *>(m->values),192u);
    unsigned int current[LM_PREFERENCES_WORD_COUNT], low=0u, high=0u;
    capture(current);
    for(unsigned int i=0;i<LM_PREFERENCES_VALUE_COUNT;++i) if(current[i]!=sBaseline[i]) {
        if(i<32u) low|=1u<<i; else high|=1u<<(i-32u);
    }
    sSequence=m->requestSeq;
    const bool valid=LmPreferencesValid(m)!=0;
    if(valid && m->status==LM_PREFERENCES_OK) {
        unsigned int loaded[LM_PREFERENCES_WORD_COUNT];
        for(unsigned int i=0;i<LM_PREFERENCES_WORD_COUNT;++i) loaded[i]=m->values[i];
        apply(loaded,m->presentLo,m->presentHi);
        capture(sBaseline);
        sText="SETTINGS: LOADED FROM SD";
        if(low || high) apply(current,low,high);
    } else {
        sText=valid && m->status==LM_PREFERENCES_NO_FILE ? "SETTINGS: NEW CONFIGURATION" :
            valid ? failure(m->status) : "SETTINGS: MEMORY CHECK FAILED";
        sNeedsDefaults=valid && m->status==LM_PREFERENCES_NO_FILE;
    }
    sInitialized=true;
    if(sNeedsDefaults || low || high) { capture(sDesired); sQueued=true; }
    return true;
}

void publish() {
    if(!sQueued || sPending || !sInitialized) return;
    sQueued=false;
    if(!sNeedsDefaults && !different(sDesired,sBaseline)) return;
    volatile LmPreferencesBlock *m=mailbox();
    invalidate(const_cast<unsigned int *>(&m->ackSeq),32u);
    if(m->ready!=1u || m->ackSeq!=sSequence || m->requestSeq!=sSequence) {
        sText="SETTINGS: SD UNAVAILABLE";
        return;
    }
    for(unsigned int i=0;i<LM_PREFERENCES_WORD_COUNT;++i) m->values[i]=sDesired[i];
    store(const_cast<unsigned int *>(m->values),192u);
    barrier();
    m->magic=LM_PREFERENCES_MAGIC; m->version=LM_PREFERENCES_VERSION;
    m->presentLo=LM_PREFERENCES_PRESENT_LO; m->presentHi=LM_PREFERENCES_PRESENT_HI;
    m->reserved0[0]=m->reserved0[1]=0u;
    sSentChecksum=LmPreferencesChecksum(sDesired,m->presentLo,m->presentHi);
    m->checksum=sSentChecksum;
    if(++sSequence==0u) ++sSequence;
    m->requestSeq=sSequence;
    store(const_cast<LmPreferencesBlock *>(m),32u);
    barrier();
    sPending=true;
    sText="SETTINGS: SAVING TO SD...";
}

void poll() {
    if(!sPending) return;
    volatile LmPreferencesBlock *m=mailbox();
    invalidate(const_cast<unsigned int *>(&m->ackSeq),32u);
    if(m->ackSeq!=sSequence) return;
    sPending=false;
    if(m->ready==1u && m->status==LM_PREFERENCES_OK && m->checksum==sSentChecksum && LmPreferencesValid(m)) {
        for(unsigned int i=0;i<LM_PREFERENCES_WORD_COUNT;++i) sBaseline[i]=m->values[i];
        sNeedsDefaults=false;
        sText="SETTINGS: SAVED TO SD";
    } else sText=m->status==LM_PREFERENCES_OK ? "SETTINGS: MEMORY CHECK FAILED" : failure(m->status);
}
}
#endif

namespace LMPreferences {
void requestSave() {
#if !IS_EMULATOR
    if(!sCapturedDefaults) { capture(sBaseline); sCapturedDefaults=true; }
    if(!initialize()) return;
    capture(sDesired);
    sQueued=true;
    publish();
#endif
}
void tick() {
#if !IS_EMULATOR
    if(!sCapturedDefaults) { capture(sBaseline); sCapturedDefaults=true; }
    const bool open=LMPractice::isOpen();
    if(initialize()) {
        poll();
        if(sWasOpen && !open) { capture(sDesired); sQueued=true; }
        publish();
    }
    sWasOpen=open;
#endif
}
const char *statusText() {
#if IS_EMULATOR
    return "SETTINGS: SD PERSISTENCE WII ONLY";
#else
    return sText;
#endif
}
}
#endif
