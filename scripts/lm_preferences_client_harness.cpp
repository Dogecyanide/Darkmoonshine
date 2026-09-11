#define SUSAMUNE_VERSION_LMJ 1
#define LM_PREFERENCES_HOST_TEST 1
#include "susamune/lm_preferences.h"
namespace {
alignas(32) LmPreferencesBlock sTestBlock;
unsigned int sLive[48], sApplied, sEvents[256][3], sEventCount;
bool sMenuOpen;
void event(unsigned int type, void *p, unsigned int bytes) {
    if(sEventCount<256u) {
        sEvents[sEventCount][0]=type;
        sEvents[sEventCount][1]=(unsigned int)(static_cast<unsigned char *>(p)-reinterpret_cast<unsigned char *>(&sTestBlock));
        sEvents[sEventCount++][2]=bytes;
    }
}
volatile LmPreferencesBlock *lmPreferencesTestBlock() { return &sTestBlock; }
void lmPreferencesTestInvalidate(void *p,unsigned int n) { event(1u,p,n); }
void lmPreferencesTestStore(void *p,unsigned int n) { event(2u,p,n); }
void lmPreferencesTestBarrier() { event(3u,&sTestBlock,0u); }
void readRange(const unsigned int *values,unsigned int low,unsigned int high,unsigned int first,unsigned int end) {
    ++sApplied;
    for(unsigned int i=first;i<end;++i) if(LmPreferencesHas(low,high,i)) sLive[i]=values[i];
}
}
namespace LMTools {
void writePreferences(unsigned int *v) { for(unsigned int i=0;i<14u;++i) v[i]=sLive[i]; }
void readPreferences(const unsigned int *v,unsigned int l,unsigned int h) { readRange(v,l,h,0,14); }
}
namespace LMTimer {
void writePreferences(unsigned int *v) { for(unsigned int i=14;i<45u;++i) v[i]=sLive[i]; v[46]=sLive[46]; }
void readPreferences(const unsigned int *v,unsigned int l,unsigned int h) {
    readRange(v,l,h,14,45);
    if(LmPreferencesHas(l,h,46)) sLive[46]=v[46];
}
}
namespace LMPractice {
bool isOpen() { return sMenuOpen; }
void writePreferences(unsigned int *v) { v[45]=sLive[45]; v[47]=sLive[47]; }
void readPreferences(const unsigned int *v,unsigned int l,unsigned int h) {
    readRange(v,l,h,45,46);
    if(LmPreferencesHas(l,h,47)) sLive[47]=v[47];
}
}
#include "../lm_diag/src/lm_preferences.cpp"
extern "C" {
__declspec(dllexport) void reset_client() {
    volatile unsigned char *b=reinterpret_cast<volatile unsigned char *>(&sTestBlock);
    for(unsigned int i=0;i<sizeof(sTestBlock);++i) b[i]=0;
    for(unsigned int i=0;i<48;++i) sLive[i]=100+i;
    sApplied=sEventCount=0u; sMenuOpen=false;
#if !IS_EMULATOR
    sInitialized=sCapturedDefaults=sPending=sQueued=sNeedsDefaults=sWasOpen=false;
    sSequence=sSentChecksum=0;
    for(unsigned int i=0;i<48;++i) sBaseline[i]=sDesired[i]=0u;
    sText="SETTINGS: WAITING FOR SD";
#endif
}
__declspec(dllexport) void boot_config(unsigned int status,unsigned int low,unsigned int high,unsigned int seq) {
    sTestBlock.magic=LM_PREFERENCES_MAGIC; sTestBlock.version=LM_PREFERENCES_VERSION;
    sTestBlock.requestSeq=sTestBlock.ackSeq=seq;
    sTestBlock.status=status; sTestBlock.ready=1;
    sTestBlock.presentLo=low;sTestBlock.presentHi=high;
    for(unsigned int i=0;i<LM_PREFERENCES_WORD_COUNT;++i) sTestBlock.values[i]=LmPreferencesHas(low,high,i)?1000+i:0xffffffffu;
    sTestBlock.checksum=LmPreferencesChecksum(sTestBlock.values,low,high);
}
__declspec(dllexport) void step() { LMPreferences::tick(); }
__declspec(dllexport) void save_config() { LMPreferences::requestSave(); }
__declspec(dllexport) void set_live(unsigned int i,unsigned int value) { if(i<LM_PREFERENCES_VALUE_COUNT) sLive[i]=value; }
__declspec(dllexport) unsigned int get_live(unsigned int i) { return i<LM_PREFERENCES_VALUE_COUNT?sLive[i]:0; }
__declspec(dllexport) void set_open(unsigned int v) { sMenuOpen=v!=0; }
__declspec(dllexport) unsigned int mailbox_word(unsigned int i) { return i<64?reinterpret_cast<unsigned int *>(&sTestBlock)[i]:0; }
__declspec(dllexport) void tamper(unsigned int i,unsigned int value) { if(i<64) reinterpret_cast<unsigned int *>(&sTestBlock)[i]=value; }
__declspec(dllexport) void ack(unsigned int seq,unsigned int status) { sTestBlock.ackSeq=seq;sTestBlock.status=status; }
__declspec(dllexport) unsigned int applied() { return sApplied; }
__declspec(dllexport) unsigned int event_count() { return sEventCount; }
__declspec(dllexport) unsigned int event_word(unsigned int i,unsigned int j) { return i<sEventCount&&j<3?sEvents[i][j]:0; }
__declspec(dllexport) void clear_events() { sEventCount=0; }
__declspec(dllexport) const char *status_text() { return LMPreferences::statusText(); }
}
