#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_timer.hxx"
#include "lm_timer_clock.hxx"
#include "lm_draw.hxx"
#include "lm_menu_navigation.hxx"
#include "Dolphin/PAD.h"
#include "susamune/lm_creation.h"
#include "susamune/lm_timer_render.h"

namespace {
using TextFn = void (*)(void *, u16, u16, const char *, ...);
constexpr LMCreation::Style kDefaults = {320, 95, 100, 255, 0, 0, 0, 0, 100, 255};
constexpr unsigned char kWhite[9][3] = {
    {255,255,255},{255,255,255},{255,255,255},{255,255,255},{255,255,255},
    {255,255,255},{255,255,255},{255,255,255},{255,255,255}
};
constexpr unsigned char kStreakColour[1][3] = {{255,40,255}};
const char *const kTargets[] = {"Minutes tens", "Minutes units", "Minute mark",
    "Seconds tens", "Seconds units", "Second mark", "Hundredths tens",
    "Hundredths units", "TIME icon"};
const char *const kOptions[] = {"Red", "Green", "Blue", "Opacity", "Brightness",
    "Background red", "Background green", "Background blue", "Background opacity", "Padding"};
LMCreation::Style sStyle = kDefaults, sStreakStyle = kDefaults;
unsigned char sRgb[9][3] = {
    {255,255,255},{255,255,255},{255,255,255},{255,255,255},{255,255,255},
    {255,255,255},{255,255,255},{255,255,255},{255,255,255}
};
unsigned char sStreakRgb[1][3] = {{255,40,255}};
LMCreation::Editor sEditor;
LmTimerCache sRasterCache;
LMMenuNavigation::Stick sStick;
bool sVisible, sLabel = true, sMenu, sEditingStreak;
unsigned sSelected, sPrevious, sRepeatMask, sRepeatFrames;
unsigned rgb(const unsigned char *v) { return (v[0] << 16) | (v[1] << 8) | v[2]; }
void setRgb(unsigned char *out,unsigned value) { out[0]=value>>16; out[1]=value>>8; out[2]=value; }
bool present(unsigned lo,unsigned hi,unsigned index) {
    return ((index<32 ? lo>>index : hi>>(index-32))&1u)!=0u;
}
void writeStyle(unsigned int *v,const LMCreation::Style &s) {
    v[0]=s.x; v[1]=s.y; v[2]=s.scale; v[3]=s.textA; v[4]=s.textBrightness;
    v[5]=(s.bgR<<16)|(s.bgG<<8)|s.bgB; v[6]=s.bgA; v[7]=s.padding;
}
void readStyle(LMCreation::Style &s,const unsigned int *values,unsigned at,unsigned lo,unsigned hi) {
    const unsigned int *v=values+at;
    if(present(lo,hi,at) && v[0]<=640u) s.x=v[0];
    if(present(lo,hi,at+1) && v[1]<=480u) s.y=v[1];
    if(present(lo,hi,at+2) && v[2]>=50u && v[2]<=200u) s.scale=v[2];
    if(present(lo,hi,at+3) && v[3]<=255u) s.textA=v[3];
    if(present(lo,hi,at+4) && v[4]>=25u && v[4]<=200u) s.textBrightness=v[4];
    if(present(lo,hi,at+5) && v[5]<=0xFFFFFFu) { s.bgR=v[5]>>16; s.bgG=v[5]>>8; s.bgB=v[5]; }
    if(present(lo,hi,at+6) && v[6]<=255u) s.bgA=v[6];
    if(present(lo,hi,at+7) && (v[7]<=16u || v[7]==255u)) s.padding=v[7];
}
unsigned repeat(unsigned held) {
    unsigned fired = held & ~sRepeatMask;
    if (!held || held != sRepeatMask) sRepeatFrames = 0;
    else if (++sRepeatFrames >= 18u) { fired |= held; sRepeatFrames = 14u; }
    sRepeatMask = held;
    return fired;
}
void background(LmXfbSurface *surface, const LMCreation::Style &style, const int *bounds) {
    if (!style.bgA || style.padding == 255u) return;
    const int padding = style.padding;
    LmXfbBox(surface,bounds[0]-padding,bounds[1]-padding,
        bounds[2]-bounds[0]+padding*2,bounds[3]-bounds[1]+padding*2,
        (style.bgR<<16)|(style.bgG<<8)|style.bgB,style.bgA);
}
void render(void *xfb, bool preview) {
    LmTimerStyle style = {};
    style.x = sStyle.x; style.y = sStyle.y; style.scale = sStyle.scale;
    style.opacity = sStyle.textA; style.brightness = sStyle.textBrightness;
    for (unsigned i = 0; i < 9; ++i) style.colours[i] = rgb(sRgb[i]);
    style.showLabel = sLabel || (preview && !sEditingStreak && sEditor.target == 9u);
    style.showStreak = 1;
    LmTimerStreakStyle streak = {};
    streak.x = static_cast<int>(sStreakStyle.x) - kDefaults.x;
    streak.y = static_cast<int>(sStreakStyle.y) - kDefaults.y;
    streak.scale = sStreakStyle.scale; streak.opacity = sStreakStyle.textA;
    streak.brightness = sStreakStyle.textBrightness; streak.rgb = rgb(sStreakRgb[0]);
    LmXfbSurface surface = {static_cast<unsigned char *>(xfb), 640, 480, 1280};
    int bounds[4];
    LmTimerBounds(&style,&streak,bounds);
    LmTimerDirtyRows dirty={bounds[1],bounds[3]};
    if(sStyle.bgA && sStyle.padding!=255u) {
        LmTimerIncludeRows(&dirty,bounds[1]-sStyle.padding,bounds[3]+sStyle.padding);
    }
    background(&surface,sStyle,bounds);
    LmTimerStreakBounds(&style,&streak,bounds);
    if(sStreakStyle.bgA && sStreakStyle.padding!=255u) {
        LmTimerIncludeRows(&dirty,bounds[1]-sStreakStyle.padding,bounds[3]+sStreakStyle.padding);
    }
    background(&surface,sStreakStyle,bounds);
    LmTimerDraw(&surface, LMTimerClock::centiseconds(), &style, &streak, &sRasterCache);
    if (preview && sEditor.target && !sEditingStreak) {
        LmTimerTargetBounds(&style,sEditor.target-1u,bounds);
        const int x=(bounds[0]+bounds[2])/2, y=bounds[3]+3;
        const short arrow[] = {static_cast<short>(x),static_cast<short>(y),
            static_cast<short>(x-4),static_cast<short>(y+5),
            static_cast<short>(x+4),static_cast<short>(y+5)};
        LmXfbFillPoly(&surface,arrow,3,0xB0F1C8u,255);
        LmTimerIncludeRows(&dirty,y,y+6);
    }
    int top,height;
    LmTimerFlushRows(&dirty,&top,&height);
    LMDraw::flush(xfb, 640, 480, top, height);
}
}
namespace LMTimer {
void writePreferences(unsigned int v[48]) {
    v[6]=sVisible; v[7]=sLabel;
    writeStyle(v+8,sStyle);
    for(unsigned i=0;i<9;++i) v[16+i]=rgb(sRgb[i]);
    writeStyle(v+25,sStreakStyle); v[33]=rgb(sStreakRgb[0]);
    v[46]=LMTimerClock::runInNativeMenus();
}
void readPreferences(const unsigned int v[48],unsigned int lo,unsigned int hi) {
    if(present(lo,hi,6) && v[6]<=1u) sVisible=v[6];
    if(present(lo,hi,7) && v[7]<=1u) sLabel=v[7];
    readStyle(sStyle,v,8,lo,hi);
    for(unsigned i=0;i<9;++i) if(present(lo,hi,16+i) && v[16+i]<=0xFFFFFFu) setRgb(sRgb[i],v[16+i]);
    readStyle(sStreakStyle,v,25,lo,hi);
    if(present(lo,hi,33) && v[33]<=0xFFFFFFu) setRgb(sStreakRgb[0],v[33]);
    if(present(lo,hi,46) && v[46]<=1u) LMTimerClock::setRunInNativeMenus(v[46]!=0u);
}
void tick(bool) { LMTimerClock::tick(); }
void draw(void *, void *xfb) { if (sVisible && LMTimerClock::available()) render(xfb, false); }
void openMenu(const PADStatus &pad) {
    sMenu = true; sPrevious = pad.mButton; sSelected = 0;
    sRepeatMask = sRepeatFrames = 0; sStick.held = 0;
}
bool menuOpen() { return sMenu; }
void updateMenu(const PADStatus &pad) {
    const unsigned buttons = pad.mButton;
    if (sEditor.active) {
        sEditor.update(sEditingStreak ? sStreakStyle : sStyle,
            sEditingStreak ? sStreakRgb : sRgb, kDefaults,
            sEditingStreak ? kStreakColour : kWhite,
            buttons, static_cast<signed char>(pad.mSubStickX),
            static_cast<signed char>(pad.mSubStickY), !pad.mCurError);
        sPrevious = buttons;
        return;
    }
    const unsigned pressed = pad.mCurError ? 0u : buttons & ~sPrevious;
    sPrevious = buttons;
    if (pad.mCurError) { sStick.held = 0; sRepeatMask = sRepeatFrames = 0; return; }
    const unsigned nav = repeat((buttons & 15u) | sStick.sample(
        static_cast<signed char>(pad.mStickX), static_cast<signed char>(pad.mStickY)));
    if (pressed & 0x200u) { sMenu = false; return; }
    if (nav & 8u) sSelected = (sSelected + 4u) % 5u;
    else if (nav & 4u) sSelected = (sSelected + 1u) % 5u;
    if (!(pressed & 0x100u) && !(nav & 3u)) return;
    if (sSelected == 0) sVisible = !sVisible;
    else if (sSelected == 2) sLabel = !sLabel;
    else if (sSelected == 4) LMTimerClock::setRunInNativeMenus(!LMTimerClock::runInNativeMenus());
    else if (pressed & 0x100u) {
        sEditingStreak = sSelected == 3;
        sEditor.begin(sEditingStreak ? sStreakStyle : sStyle,
                      sEditingStreak ? sStreakRgb : sRgb,
                      sEditingStreak ? 1u : 9u, buttons);
    }
}
void drawMenu(void *dp, void *xfb) {
    auto text = reinterpret_cast<TextFn>(0x801D49F8u);
    if (!sEditor.active) {
        LMDraw::fillBox(xfb,640,480,6,31,308,176,0x0C171Eu);
        LMDraw::fillBox(xfb,640,480,6,31,308,17,0x24543Fu);
        text(dp,13,36,"DISPLAYS / SUNSHINE TIMER");
        const char *const names[] = {"Visible", "Edit...", "TIME icon", "Edit streak...", "Run in native menus"};
        for (unsigned i = 0; i < 5; ++i) {
            if (sSelected == i) LMDraw::fillBox(xfb,640,480,10,57+i*18,300,16,0x284D46u);
            text(dp,16,61+i*18,"%s %s",sSelected==i?">":" ",names[i]);
            if (i == 0 || i == 2) text(dp,254,61+i*18,"%s",(i==0?sVisible:sLabel)?"ON":"OFF");
            if (i == 4) text(dp,254,61+i*18,"%s",LMTimerClock::runInNativeMenus()?"ON":"OFF");
        }
        text(dp,13,148,"D-pad down menu always runs.");
        text(dp,13,160,"Close the main menu to save preferences.");
        text(dp,13,183,"A: select   B: Displays   Stick: choose");
        LMDraw::flush(xfb,640,480,31,176);
        return;
    }
    render(xfb, true);
    LMCreation::Style &style = sEditingStreak ? sStreakStyle : sStyle;
    const unsigned char (*colours)[3] = sEditingStreak ? sStreakRgb : sRgb;
    // Dock opposite the preview, as in Moonshine's Creation panel.
    const unsigned top = sStyle.y < 240 ? 123u : 28u;
    LMDraw::fillBox(xfb,640,480,6,top,308,109,0x0C171Eu,238);
    LMDraw::fillBox(xfb,640,480,6,top,308,14,0x24543Fu);
    text(dp,12,top+4,"CREATION / %s",sEditingStreak?"STREAK":"SUNSHINE TIMER");
    text(dp,12,top+19,"Target: %s",sEditingStreak?"Streak":sEditor.target?kTargets[sEditor.target-1]:"All characters + TIME");
    text(dp,12,top+29,"X:%u Y:%u  Size:%u%%",style.x,style.y,style.scale);
    for (unsigned i=0;i<LMCreation::OptionCount;++i) {
        const unsigned x=12+(i/5)*152, y=top+41+(i%5)*8;
        if (i==sEditor.option) LMDraw::fillBox(xfb,640,480,x-2,y-1,150,8,0x284D46u);
        const int value=sEditor.value(style,colours,i);
        text(dp,x,y,"%s %s",i==sEditor.option?">":" ",kOptions[i]);
        if (value<0) text(dp,x+120,y,"Mix");
        else if(i==LMCreation::Padding && value==255) text(dp,x+120,y,"Off");
        else text(dp,x+126,y,"%3d",value);
    }
    text(dp,12,top+84,"C-stick: edit  START: next  X+START: back");
    text(dp,12,top+92,"D-pad: tap fine / hold fast  L/R: size");
    text(dp,12,top+100,"A: keep  B: discard  Z: reset option");
    if (sEditor.confirm) {
        LMDraw::fillBox(xfb,640,480,50,top+34,220,42,0x08101Au);
        text(dp,60,top+44,"%s",sEditor.confirm==LMCreation::Keep?"Keep these changes?":
            sEditor.confirm==LMCreation::Discard?"Discard all changes?":"Reset selected option?");
        text(dp,60,top+61,"A: confirm   B: go back");
    }
    LMDraw::flush(xfb,640,480,top,109);
}
}
#endif
