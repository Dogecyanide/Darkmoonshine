#ifndef LM_TIMER_HXX
#define LM_TIMER_HXX
struct PADStatus;
namespace LMTimer {
void tick(bool modMenuOpen);
void draw(void *directPrint, void *xfb);
void openMenu(const PADStatus &pad);
bool menuOpen();
void updateMenu(const PADStatus &pad);
void drawMenu(void *directPrint, void *xfb);
void writePreferences(unsigned int values[48]);
void readPreferences(const unsigned int values[48], unsigned int presentLo, unsigned int presentHi);
}
#endif
