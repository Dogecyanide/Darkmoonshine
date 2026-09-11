#include "susamune/lm_reset_bind.h"
using namespace LMResetBind;
static bool equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
int main() {
    for (unsigned mask=0; mask<65536; ++mask) {
        unsigned bits=0;
        for (unsigned i=0;i<16;++i) if (mask&(1u<<i)) ++bits;
        if (valid(mask) != (!(mask & ~0x0F78u) && bits>=2 && bits<=4)) return 1;
        char label[34]; label[32]='Q'; label[33]='Z';
        text(mask,label);
        if (label[32]!='Q' || label[33]!='Z') return 2;
        unsigned length=0; while(length<32 && label[length]) ++length;
        if (length==32) return 3;
        if (!valid(mask) && !equal(label,"OFF")) return 4;
    }
    char name[32]; text(0x858,name);
    if (!equal(name,"L+Z+Y+D-Up")) return 5;
    const unsigned bind=0x440; // L+X
    Trigger trigger={false};
    if (trigger.sample(bind,bind,true,true)) return 6; // must first release
    if (trigger.sample(bind,0,true,true)) return 7;
    if (trigger.sample(bind,0x40,true,true)) return 8;
    if (!trigger.sample(bind,bind,true,true)) return 9;
    for (unsigned i=0;i<300;++i) if(trigger.sample(bind,bind,true,true)) return 10;
    if (trigger.sample(bind,0x40,true,true) || trigger.sample(bind,bind,true,true)) return 11;
    trigger.sample(bind,0,true,true);
    if (!trigger.sample(bind,bind,true,true)) return 12;
    trigger.sample(bind,0,true,true);
    if (trigger.sample(bind,bind|0x100,true,true) || trigger.sample(bind,bind,true,true)) return 13;
    trigger.sample(bind,0,true,true);
    if (trigger.sample(bind,0,false,true) || trigger.sample(bind,bind,true,true)) return 14;
    trigger.sample(bind,0,true,true);
    if (trigger.sample(bind,0,true,false) || trigger.sample(bind,bind,true,true)) return 15;
    trigger.sample(bind,0,true,true);
    if (trigger.sample(0,bind,true,true)) return 16;
    Recorder recorder;
    recorder.begin();
    if (recorder.sample(0x100,true)!=Waiting || !recorder.releaseFirst) return 17;
    if (recorder.sample(0,true)!=Waiting || recorder.releaseFirst) return 18;
    if (recorder.sample(0x40,true)!=Waiting || recorder.sample(bind,true)!=Waiting ||
        recorder.sample(0x400,true)!=Waiting || recorder.sample(0,true)!=Accepted ||
        recorder.chord!=bind) return 19;
    recorder.begin(); recorder.sample(0,true); recorder.sample(0x200,true);
    if (recorder.sample(0,true)!=Cancelled) return 20;
    recorder.begin(); recorder.sample(0,true); recorder.sample(0x300,true);
    if (recorder.sample(0,true)!=Accepted) return 21; // B is legal in a combo
    const unsigned badMasks[] = {1u,2u,4u,0x1000u,0xF78u,0x100u};
    for (unsigned bad : badMasks) {
        recorder.begin(); recorder.sample(0,true); recorder.sample(bad,true);
        if (recorder.sample(0,true)!=Invalid) return 22;
    }
    recorder.begin(); recorder.sample(0,true); recorder.sample(bind,true);
    recorder.sample(0x140,true); // cannot assemble a combo by swapping held buttons
    if (recorder.sample(0,true)!=Invalid) return 23;
    recorder.begin(); if(recorder.sample(0,false)!=Cancelled) return 24;
    return 0;
}
