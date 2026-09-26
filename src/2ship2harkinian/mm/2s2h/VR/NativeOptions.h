#pragma once
#ifdef MMVR_ENABLE
namespace mmvr { struct UiDrawFrame; }
namespace Fast { class Fast3dGui; }
namespace mmvrgame {
void DrawNativeOptions(const mmvr::UiDrawFrame& frame, Fast::Fast3dGui& gui);
}
#endif
