#include "../include/susamune/lm_creation.h"
#define CHECK(v) do { if (!(v)) return __LINE__; } while (0)
int testOriginal() {
    using namespace LMCreation;
    const Style defaults = {200, 60, 100, 255, 0, 0, 0, 0, 100, 255};
    Style style = defaults;
    unsigned char rgb[2][3] = {{255,255,255},{255,255,255}};
    const unsigned char colours[2][3] = {{255,255,255},{255,255,255}};
    Editor e;
    e.begin(style, rgb, 2, 0x100);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(e.confirm == None); // Opening A is not a keep confirmation.
    e.update(style,rgb,defaults,colours,0,0,0,true);
    e.update(style,rgb,defaults,colours,0,-80,0,true);
    CHECK(rgb[0][0] == 251 && rgb[1][0] == 251);
    e.update(style,rgb,defaults,colours,0x1000,0,0,true);
    CHECK(e.target == 1);
    e.update(style,rgb,defaults,colours,0,-80,0,true);
    CHECK(rgb[0][0] == 247 && rgb[1][0] == 251);
    e.target = 0;
    CHECK(e.value(style,rgb,0) == -1);
    e.update(style,rgb,defaults,colours,2|0x20,0,0,true);
    CHECK(style.x == 202 && style.scale == 102);
    e.update(style,rgb,defaults,colours,0x200,0,0,true);
    CHECK(e.confirm == Discard);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(!e.active && style.x == 200 && style.scale == 100 && rgb[0][0] == 255);
    e.begin(style,rgb,2,0);
    e.option = Padding;
    e.update(style,rgb,defaults,colours,0,80,0,true);
    CHECK(style.padding == 0);
    e.update(style,rgb,defaults,colours,0,-80,0,true);
    CHECK(style.padding == 255);
    e.option = Opacity;
    e.update(style,rgb,defaults,colours,0,-80,0,true);
    e.update(style,rgb,defaults,colours,0,0,0,true);
    e.update(style,rgb,defaults,colours,0,-80,0,true);
    CHECK(style.textA == 251);
    e.update(style,rgb,defaults,colours,0x10,0,0,true);
    CHECK(e.confirm == Reset);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(style.textA == 255 && e.active);
    e.update(style,rgb,defaults,colours,0,0,0,true);
    e.update(style,rgb,defaults,colours,0x100,0,0,false);
    CHECK(e.confirm == None);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(e.confirm == None);
    e.update(style,rgb,defaults,colours,0,0,0,true);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(e.confirm == Keep);
    e.update(style,rgb,defaults,colours,0,0,0,true);
    e.update(style,rgb,defaults,colours,0x100,0,0,true);
    CHECK(!e.active);
    return 0;
}

struct MovementFixture {
    const LMCreation::Style defaults = {200,60,100,255,0,0,0,0,100,255};
    LMCreation::Style style = defaults;
    unsigned char rgb[2][3] = {{255,255,255},{255,255,255}};
    const unsigned char colours[2][3] = {{255,255,255},{255,255,255}};
    LMCreation::Editor editor;
    MovementFixture() { editor.begin(style,rgb,2,0); }
    void tick(unsigned buttons, int cx=0, int cy=0, bool connected=true) {
        editor.update(style,rgb,defaults,colours,buttons,cx,cy,connected);
    }
};

int testPreciseTaps() {
    MovementFixture f;
    f.tick(2); CHECK(f.style.x==202);
    for(unsigned i=0;i<5;++i) f.tick(2);
    CHECK(f.style.x==202);
    f.tick(0); f.tick(2); CHECK(f.style.x==204);
    f.tick(0); f.tick(8); CHECK(f.style.y==58);
    f.tick(0); f.tick(4); CHECK(f.style.y==60);
    return 0;
}

int testCrossScreen() {
    MovementFixture f;
    f.style.x=640; f.style.y=0;
    unsigned updates=0;
    while((f.style.x || f.style.y<480) && updates<90) { f.tick(1|4); ++updates; }
    CHECK(f.style.x==0 && f.style.y==480);
    CHECK(updates==83); // 2.77 seconds at LM's 30 updates per second.
    f.tick(0);
    updates=0;
    while((f.style.x<640 || f.style.y) && updates<90) { f.tick(2|8); ++updates; }
    CHECK(f.style.x==640 && f.style.y==0 && updates==83);
    f.style.x=320; f.style.y=95; f.tick(0); updates=0;
    while((f.style.x || f.style.y<419) && updates<60) { f.tick(1|4); ++updates; }
    CHECK(f.style.x==0 && f.style.y>=419 && updates<=53);
    return 0;
}

int testDirectionSwitch() {
    MovementFixture f;
    for(unsigned i=0;i<40;++i) f.tick(2);
    unsigned before=f.style.x;
    f.tick(1); CHECK(f.style.x==before-2 && f.editor.moveX.updates==0);
    for(unsigned i=0;i<5;++i) f.tick(1);
    CHECK(f.style.x==before-2);
    f.tick(0); f.tick(1); CHECK(f.style.x==before-4);
    f.tick(2); CHECK(f.style.x==before-2);
    return 0;
}

int testIndependentAxesAndColour() {
    MovementFixture f;
    for(unsigned i=0;i<35;++i) f.tick(2);
    unsigned x=f.style.x, y=f.style.y;
    f.tick(2|4); CHECK(f.style.x==x+10 && f.style.y==y+2);
    f.tick(2|8); CHECK(f.style.x==x+20 && f.style.y==y);
    MovementFixture moving, still;
    for(unsigned i=0;i<60;++i) {
        moving.tick(i&1 ? 1u : 2u,-80,0);
        still.tick(0,-80,0);
        CHECK(moving.rgb[0][0]==still.rgb[0][0]);
        CHECK(moving.editor.option==still.editor.option);
    }
    return 0;
}

int testBoundsAndOpposites() {
    MovementFixture f;
    for(unsigned i=0;i<1000;++i) f.tick(1|8);
    CHECK(f.style.x==0 && f.style.y==0);
    for(unsigned i=0;i<1000;++i) f.tick(15);
    CHECK(f.style.x==0 && f.style.y==0);
    for(unsigned i=0;i<1000;++i) f.tick(2|4);
    CHECK(f.style.x==640 && f.style.y==480);
    for(unsigned i=0;i<1000;++i) f.tick(15);
    CHECK(f.style.x==640 && f.style.y==480);
    f.tick(1|8); CHECK(f.style.x==638 && f.style.y==478);
    return 0;
}

int testConfirmationsResetMovement() {
    MovementFixture f;
    for(unsigned i=0;i<40;++i) f.tick(2);
    unsigned before=f.style.x;
    f.tick(2|0x100); CHECK(f.editor.confirm==LMCreation::Keep && f.style.x==before);
    for(unsigned i=0;i<100;++i) f.tick(2);
    CHECK(f.style.x==before);
    f.tick(2|0x200); CHECK(f.editor.confirm==LMCreation::None && f.style.x==before);
    f.tick(2); CHECK(f.style.x==before+2 && f.editor.moveX.updates==0);
    f.tick(0); f.tick(0x200); f.tick(0x100);
    CHECK(!f.editor.active && f.style.x==f.defaults.x && f.style.y==f.defaults.y);
    f.editor.begin(f.style,f.rgb,2,0x100);
    f.tick(0); f.tick(2); CHECK(f.style.x==f.defaults.x+2);
    f.tick(0x10|2); CHECK(f.editor.confirm==LMCreation::Reset);
    f.tick(0x100|2); CHECK(f.editor.confirm==LMCreation::None);
    before=f.style.x; f.tick(2); CHECK(f.style.x==before+2);
    return 0;
}

int testDisconnectAndRelease() {
    MovementFixture f;
    for(unsigned i=0;i<40;++i) f.tick(2|4);
    unsigned x=f.style.x, y=f.style.y;
    f.tick(2|4,0,0,false); CHECK(f.style.x==x && f.style.y==y);
    f.tick(2|4); CHECK(f.style.x==x+2 && f.style.y==y+2);
    for(unsigned i=0;i<40;++i) f.tick(1|8);
    x=f.style.x; y=f.style.y;
    f.tick(0); f.tick(0); CHECK(f.style.x==x && f.style.y==y);
    f.tick(1|8); CHECK(f.style.x==x-2 && f.style.y==y-2);
    return 0;
}

int testAccelerationThresholds() {
    LMCreation::Movement m;
    CHECK(m.step(1)==2);
    for(unsigned i=1;i<6;++i) CHECK(m.step(1)==0);
    for(unsigned i=6;i<15;++i) CHECK(m.step(1)==2);
    for(unsigned i=15;i<30;++i) CHECK(m.step(1)==6);
    for(unsigned i=30;i<10000;++i) CHECK(m.step(1)==10);
    CHECK(m.updates==30);
    CHECK(m.step(-1)==-2 && m.updates==0);
    CHECK(m.step(0)==0 && m.direction==0 && m.updates==0);
    return 0;
}

int testScaleKeepsOriginalRepeat() {
    MovementFixture moving, still;
    for(unsigned i=0;i<90;++i) {
        moving.tick((i&1 ? 1u : 2u)|0x20u);
        still.tick(0x20u);
        CHECK(moving.style.scale==still.style.scale);
    }
    CHECK(still.style.scale<200u);
    return 0;
}

#ifndef LM_CREATION_CASE
#define LM_CREATION_CASE 0
#endif
int main() {
    switch(LM_CREATION_CASE) {
    case 0: return testOriginal();
    case 1: return testPreciseTaps();
    case 2: return testCrossScreen();
    case 3: return testDirectionSwitch();
    case 4: return testIndependentAxesAndColour();
    case 5: return testBoundsAndOpposites();
    case 6: return testConfirmationsResetMovement();
    case 7: return testDisconnectAndRelease();
    case 8: return testAccelerationThresholds();
    case 9: return testScaleKeepsOriginalRepeat();
    }
    return 1;
}
