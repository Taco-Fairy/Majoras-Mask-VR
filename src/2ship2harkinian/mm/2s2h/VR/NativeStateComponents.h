#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "save_states/Archive.h"
namespace mmvrgame {
// Native memory and C++ containers are prepared separately. Container adapters
// allocate/validate before native commit, then perform only non-throwing swaps.
mmvr::states::Component ItemInputResetComponent();
void VerifyItemInputResetComponent();
}
#endif
