#include "../lm_diag/include/lm_menu_navigation.hxx"
#define CHECK(value) do { if (!(value)) return __LINE__; } while (0)
int main() {
    using namespace LMMenuNavigation;
    Stick stick;
    stick.held = 0;
    CHECK(stick.sample(47, 0) == 0);
    CHECK(stick.sample(48, 0) == 2);
    CHECK(stick.sample(29, 0) == 2);
    CHECK(stick.sample(27, 0) == 0);
    CHECK(stick.sample(32, 0) == 0);
    CHECK(stick.sample(-80, 0) == 1);
    CHECK(stick.sample(0, -80) == 4);
    CHECK(stick.sample(0, 80) == 8);
    CHECK(stick.sample(-80, 60) == 1);
    CHECK(stick.sample(-60, 80) == 8);
    CHECK(stick.sample(0, 0) == 0);
    for (unsigned i = 0; i < 9; ++i) {
        CHECK(grid(grid(i, 1), 2) == i);
        CHECK(grid(grid(i, 8), 4) == i);
        CHECK(grid(grid(grid(i, 1), 1), 1) == i);
        CHECK(grid(grid(grid(i, 4), 4), 4) == i);
    }
    CHECK(grid(0, 1) == 2);
    CHECK(grid(0, 8) == 6);
    CHECK(grid(8, 4) == 2);
    return 0;
}
