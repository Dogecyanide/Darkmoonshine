#include "susamune/lm_name_keyboard.h"
namespace {
struct Guarded {
    unsigned before;
    LMNameKeyboard::State state;
    unsigned after;
} draft;
}
extern "C" {
void reset(const char *name) {
    draft.before = 0x12345678u;
    draft.after = 0xABCDEF98u;
    LMNameKeyboard::begin(draft.state, name);
}
unsigned step(unsigned pressed, unsigned held) {
    return LMNameKeyboard::update(draft.state, pressed, held);
}
const char *draft_text() { return draft.state.text; }
unsigned metric(unsigned index) {
    switch (index) {
    case 0: return draft.state.length;
    case 1: return draft.state.cursor;
    case 2: return draft.state.page;
    case 3: return draft.state.uppercase;
    case 4: return draft.state.confirmation;
    case 5: return draft.before == 0x12345678u && draft.after == 0xABCDEF98u;
    default: return sizeof(draft.state);
    }
}
}
