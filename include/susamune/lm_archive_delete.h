#ifndef SUSAMUNE_LM_ARCHIVE_DELETE_H
#define SUSAMUNE_LM_ARCHIVE_DELETE_H

namespace LMArchiveDelete {
struct Prompt {
    unsigned int id, token;
    char name[32];
    bool active;
};
enum Result { Waiting, Cancelled, Confirmed };
inline bool begin(Prompt &p, unsigned int id, unsigned int token, const char *name) {
    p.active = false;
    if (!id || id > 99999999u || !token) return false;
    p.id = id; p.token = token;
    unsigned int i = 0;
    if (name) for (; i < 31u && name[i]; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        p.name[i] = c >= 32u && c <= 126u ? static_cast<char>(c) : '?';
    }
    for (; i < 32u; ++i) p.name[i] = 0;
    p.active = true;
    return true;
}
// Only fresh edges reach this modal. B/disconnect always wins over A.
inline Result update(Prompt &p, unsigned int pressed, bool connected) {
    if (!p.active) return Waiting;
    if (!connected || (pressed & 0x200u)) { p.active = false; return Cancelled; }
    if (pressed & 0x100u) { p.active = false; return Confirmed; }
    return Waiting;
}
}
#endif
