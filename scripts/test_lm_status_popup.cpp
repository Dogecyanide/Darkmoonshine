#include "susamune/lm_status_popup.h"
static bool equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
int main() {
    LmStatusPopup popup;
    popup.kind = popup.remaining = 0;
    if (*LmStatusPopupText(&popup)) return 1;
    for (unsigned int i = 0; i < 1000; ++i) LmStatusPopupTick(&popup);
    if (popup.remaining) return 2;
    static const char* const names[] = {"", "Saving", "Saved", "Loading", "Loaded", "Busy", "Rejected"};
    for (unsigned int kind = 1; kind <= 6; ++kind) {
        LmStatusPopupShow(&popup, kind);
        if (popup.remaining != 60 || !equal(LmStatusPopupText(&popup), names[kind])) return 3;
        for (unsigned int i = 0; i < 59; ++i) LmStatusPopupTick(&popup);
        if (popup.remaining != 1 || !*LmStatusPopupText(&popup)) return 4;
        // An identical failed attempt must restart, not disappear as old status.
        LmStatusPopupShow(&popup, kind);
        if (popup.remaining != 60) return 5;
        for (unsigned int i = 0; i < 60; ++i) LmStatusPopupTick(&popup);
        if (popup.remaining || *LmStatusPopupText(&popup)) return 6;
        LmStatusPopupTick(&popup);
        if (popup.remaining) return 7;
    }
    LmStatusPopupShow(&popup, LM_POPUP_LOADING);
    LmStatusPopupShow(&popup, LM_POPUP_LOADED);
    if (!equal(LmStatusPopupText(&popup), "Loaded")) return 8;
    LmStatusPopupShow(&popup, LM_POPUP_BUSY);
    if (!equal(LmStatusPopupText(&popup), "Busy")) return 9;
    LmStatusPopupShow(&popup, 0xFFFFFFFFu);
    if (popup.remaining || *LmStatusPopupText(&popup)) return 10;
    LmStatusPopupShow(&popup, LM_POPUP_NONE);
    if (popup.remaining) return 11;
    // Menu/editor D-pad navigation and discarded pending taps create no toast.
    if (LmStatusPopupGameplayEvent(1, 1) || LmStatusPopupGameplayEvent(1, 0) ||
        LmStatusPopupGameplayEvent(0, 0)) return 12;
    if (!LmStatusPopupGameplayEvent(0, 1)) return 13;
    for (unsigned int i = 0; i < 80; ++i) {
        if (LmStatusPopupGameplayEvent(1, 1)) LmStatusPopupShow(&popup, LM_POPUP_BUSY);
        LmStatusPopupTick(&popup);
    }
    if (*LmStatusPopupText(&popup)) return 14;
    return 0;
}
