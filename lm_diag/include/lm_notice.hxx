#pragma once
#include "susamune/lm_status_popup.h"

namespace LMNotice {
void show(LmStatusPopupKind kind);
void tick();
const char* text();
// Only the current, complete presenter may lend its XFB before a transaction.
void present();
}
