#include "susamune/lm_archive_delete.h"
extern "C" int main() {
    using namespace LMArchiveDelete;
    Prompt p;
    if (begin(p,0,8,"x") || begin(p,100000000,8,"x") || begin(p,1,0,"x")) return 1;
    char source[5] = {'A','%',1,'B',0};
    if (!begin(p,17,123,source)) return 2;
    source[0] = 'Z';
    if (p.id!=17 || p.token!=123 || p.name[0]!='A' || p.name[1]!='%' || p.name[2]!='?' || p.name[4]) return 3;
    if (update(p,0,true)!=Waiting || update(p,0x10,true)!=Waiting || !p.active) return 4;
    if (update(p,0x200,true)!=Cancelled || p.active) return 5;
    if (update(p,0x100,true)!=Waiting) return 6;
    if (!begin(p,99999999,1,nullptr) || p.name[0]) return 7;
    if (update(p,0x300,true)!=Cancelled) return 8;
    begin(p,1,2,"x");
    if (update(p,0x100,false)!=Cancelled) return 9;
    begin(p,1,2,"x");
    if (update(p,0x100,true)!=Confirmed || p.active || update(p,0x100,true)!=Waiting) return 10;
    const char *longName = "0123456789012345678901234567890123456789";
    begin(p,1,2,longName);
    if (p.name[30]!='0' || p.name[31]) return 11;
    return 0;
}
