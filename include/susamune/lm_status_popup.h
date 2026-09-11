#ifndef SUSAMUNE_LM_STATUS_POPUP_H
#define SUSAMUNE_LM_STATUS_POPUP_H

enum LmStatusPopupKind {
    LM_POPUP_NONE, LM_POPUP_SAVING, LM_POPUP_SAVED, LM_POPUP_LOADING,
    LM_POPUP_LOADED, LM_POPUP_BUSY, LM_POPUP_REJECTED
};
struct LmStatusPopup {
    unsigned int kind, remaining;
};
static inline int LmStatusPopupGameplayEvent(int menuOpen, int event) {
    return event && !menuOpen;
}
static inline void LmStatusPopupShow(struct LmStatusPopup* popup, unsigned int kind) {
    popup->kind = kind <= LM_POPUP_REJECTED ? kind : LM_POPUP_NONE;
    popup->remaining = popup->kind ? 60u : 0u;
}
static inline void LmStatusPopupTick(struct LmStatusPopup* popup) {
    if (popup->remaining) --popup->remaining;
}
static inline const char* LmStatusPopupText(const struct LmStatusPopup* popup) {
    if (!popup->remaining) return "";
    switch (popup->kind) {
    case LM_POPUP_SAVING: return "Saving";
    case LM_POPUP_SAVED: return "Saved";
    case LM_POPUP_LOADING: return "Loading";
    case LM_POPUP_LOADED: return "Loaded";
    case LM_POPUP_BUSY: return "Busy";
    case LM_POPUP_REJECTED: return "Rejected";
    default: return "";
    }
}
#endif
