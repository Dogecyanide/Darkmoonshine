#ifndef SUSAMUNE_LM_NAME_KEYBOARD_H
#define SUSAMUNE_LM_NAME_KEYBOARD_H

// Moonshine's Creation keyboard, with a bounded local archive-name draft.
namespace LMNameKeyboard {
static constexpr char kLower[33] = "abcdefghijklmnopqrstuvwxyz.,!?-_";
static constexpr char kUpper[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-_";
static constexpr char kSymbols[33] = "0123456789+-*/=()[]<>!?:;'\"_#.&@";
enum Prompt : unsigned char { NoPrompt, KeepPrompt, DiscardPrompt, ClearPrompt };
enum Result { None, Commit, Discard };
struct State {
    char text[32];
    unsigned char length, cursor, page;
    bool uppercase;
    Prompt confirmation;
};
static_assert(sizeof(State) <= 40, "Naming draft must remain heapless and small");

inline void begin(State &state, const char *name) {
    state = {};
    if (name) {
        while (state.length < 31 && name[state.length]) {
            const unsigned char ch = static_cast<unsigned char>(name[state.length]);
            state.text[state.length++] = ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '?';
        }
    }
    state.text[state.length] = '\0';
}

inline const char *characters(const State &state) {
    return state.page ? kSymbols : state.uppercase ? kUpper : kLower;
}

inline void append(State &state, char ch) {
    if (state.length < 31) {
        state.text[state.length++] = ch;
        state.text[state.length] = '\0';
    }
}

inline Result update(State &state, unsigned short pressed, unsigned short held) {
    if (state.confirmation != NoPrompt) {
        if (pressed & 0x0100u) {
            const Prompt prompt = state.confirmation;
            state.confirmation = NoPrompt;
            if (prompt == KeepPrompt) return Commit;
            if (prompt == DiscardPrompt) return Discard;
            state.length = 0;
            state.text[0] = '\0';
        } else if (pressed & 0x0200u) state.confirmation = NoPrompt;
        return None;
    }
    // Chords and confirmation are resolved before X can insert a space.
    if (pressed & 0x1000u) {
        state.confirmation = held & 0x0400u ? DiscardPrompt : KeepPrompt;
        return None;
    }
    if (pressed & 0x0010u) {
        state.confirmation = ClearPrompt;
        return None;
    }
    if (pressed & 1u) state.cursor = (state.cursor + 31u) % 32u;
    else if (pressed & 2u) state.cursor = (state.cursor + 1u) % 32u;
    else if (pressed & 8u) state.cursor = (state.cursor + 24u) % 32u;
    else if (pressed & 4u) state.cursor = (state.cursor + 8u) % 32u;
    if (pressed & 0x0060u) {
        state.page ^= 1u;
        state.cursor = 0;
    }
    if (pressed & 0x0800u) state.uppercase = !state.uppercase;
    if ((pressed & 0x0200u) && state.length) state.text[--state.length] = '\0';
    if (pressed & 0x0400u) append(state, ' ');
    if (pressed & 0x0100u) append(state, characters(state)[state.cursor]);
    return None;
}
}  // namespace LMNameKeyboard
#endif
