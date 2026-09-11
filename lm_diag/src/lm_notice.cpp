#if defined(SUSAMUNE_VERSION_LMJ)
#include "lm_notice.hxx"
namespace {
LmStatusPopup sPopup;
}
void LMNotice::show(LmStatusPopupKind kind) { LmStatusPopupShow(&sPopup, kind); }
void LMNotice::tick() { LmStatusPopupTick(&sPopup); }
const char* LMNotice::text() { return LmStatusPopupText(&sPopup); }
#endif
