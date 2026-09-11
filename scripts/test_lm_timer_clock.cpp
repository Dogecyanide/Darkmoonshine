#include "susamune/lm_timer_clock.h"
#define CHECK(x) if (!(x)) return __LINE__
extern "C" int main() {
    CHECK(LmTimerClockCentiseconds(0, 0) == 0);
    CHECK(LmTimerClockCentiseconds(0, 1) == 3);
    CHECK(LmTimerClockCentiseconds(0, 2) == 7);
    CHECK(LmTimerClockCentiseconds(0, 3) == 10);
    CHECK(LmTimerClockCentiseconds(0, 29) == 97);
    CHECK(LmTimerClockCentiseconds(0, 30) == 100);
    CHECK(LmTimerClockCentiseconds(0, 1800) == 6000);
    CHECK(LmTimerClockCentiseconds(0, 179999) == 599997);
    CHECK(LmTimerClockCentiseconds(0, 180000) == 599999);
    CHECK(LmTimerClockCentiseconds(0, 0xffffffffu) == 599999);
    CHECK(LmTimerClockCentiseconds(1, 0) == 599999);
    CHECK(LmTimerClockCentiseconds(0xffffffffu, 0xffffffffu) == 599999);
    CHECK(LmTimerClockReadable(2, 2, 2, 0, 1));
    CHECK(LmTimerClockReadable(2, 2, 2, 0, 0));
    CHECK(!LmTimerClockReadable(1, 2, 2, 0, 1));
    CHECK(!LmTimerClockReadable(2, 1, 2, 0, 1));
    CHECK(!LmTimerClockReadable(2, 2, 1, 0, 1));
    CHECK(!LmTimerClockReadable(2, 2, 2, 1, 1));
    CHECK(!LmTimerClockReadable(2, 2, 2, 0, 2));
    CHECK(LmTimerClockAdvances(1));
    CHECK(!LmTimerClockAdvances(0));
    CHECK(!LmTimerClockAdvances(2));
    for(unsigned owner=0;owner<256;++owner) {
        const bool native=owner>=1 && owner<=3;
        CHECK(LmTimerClockNativeMenu(owner)==native);
        for(unsigned active=0;active<3;++active)
            for(unsigned enabled=0;enabled<2;++enabled)
                for(unsigned modMenu=0;modMenu<2;++modMenu) {
                    CHECK(LmTimerClockMenuRuns(owner,enabled,modMenu)==(!native || enabled || modMenu));
                    const bool supplement=native && active==1 && (enabled || modMenu);
                    CHECK(LmTimerClockSupplement(1,active,owner,enabled,modMenu,0,120,0,120)==supplement);
                    CHECK(!LmTimerClockSupplement(0,active,owner,enabled,modMenu,0,120,0,120));
                    CHECK(!LmTimerClockSupplement(1,active,owner,enabled,modMenu,0,120,0,121));
                    CHECK(!LmTimerClockSupplement(1,active,owner,enabled,modMenu,0,120,0,30));
                    CHECK(!LmTimerClockSupplement(1,active,owner,enabled,modMenu,0,120,1,120));
                }
    }
    // Entry/steady/exit: only a menu update omitted by retail gains one tick.
    CHECK(LmTimerClockSupplement(1,1,1,1,0,0,300,0,300));
    CHECK(LmTimerClockSupplement(1,1,3,1,0,0,301,0,301));
    CHECK(!LmTimerClockSupplement(1,1,0,1,0,0,302,0,303));
    CHECK(!LmTimerClockSupplement(1,1,2,0,0,0,303,0,303));
    CHECK(LmTimerClockSupplement(1,1,2,0,1,0,303,0,303));
    // Mod-menu override never overrides the game's TIMESTOP/invalid active flag.
    CHECK(!LmTimerClockSupplement(1,0,2,1,1,0,303,0,303));
    CHECK(!LmTimerClockSupplement(1,2,2,1,1,0,303,0,303));
    CHECK(!LmTimerClockSupplement(1,1,2,1,1,0,0xffffffffu,1,0));
    CHECK(LmTimerClockEventAction(29, 1) == 1);
    CHECK(LmTimerClockEventAction(53, 1) == 2);
    CHECK(LmTimerClockEventAction(29, 0) == 0);
    CHECK(LmTimerClockEventAction(53, 0) == 0);
    CHECK(LmTimerClockEventAction(55, 1) == 0);
    CHECK(LmTimerClockEventAction(101, 1) == 0);
    CHECK(LmTimerClockEventAction(65, 1) == 0);
    for (unsigned id = 0; id < 256; ++id) {
        if (id != 29 && id != 53) CHECK(LmTimerClockEventAction(id, 1) == 0);
    }
    // No monotonic overlay accumulator: loading an earlier native count rewinds.
    CHECK(LmTimerClockCentiseconds(0, 300) > LmTimerClockCentiseconds(0, 30));
    for (unsigned frame = 0; frame < 180000; ++frame) {
        CHECK(LmTimerClockCentiseconds(0, frame) == (frame * 100u + 15u) / 30u);
    }
    return 0;
}
