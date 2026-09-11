#ifndef LM_MENU_NAVIGATION_HXX
#define LM_MENU_NAVIGATION_HXX

namespace LMMenuNavigation {
struct Stick {
    unsigned short held;
    unsigned short sample(int x, int y) {
        // Hysteresis keeps a stick resting near the threshold from repeating edges.
        const int threshold = held ? 28 : 48;
        const int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
        held = ax < threshold && ay < threshold ? 0 :
               ax > ay ? (x < 0 ? 1 : 2) : (y < 0 ? 4 : 8);
        return held;
    }
};
inline unsigned grid(unsigned selected, unsigned short direction) {
    unsigned row = selected / 3, col = selected % 3;
    if (direction == 1) col = (col + 2) % 3;
    if (direction == 2) col = (col + 1) % 3;
    if (direction == 4) row = (row + 1) % 3;
    if (direction == 8) row = (row + 2) % 3;
    return row * 3 + col;
}
}
#endif
