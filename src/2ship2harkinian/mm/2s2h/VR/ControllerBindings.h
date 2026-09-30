#pragma once
#ifdef MMVR_ENABLE
namespace mmvrgame {
bool SetVRControlBinding(int action, int source);
bool ResetVRControlBindings();
void DrawVRControllerBindings();
}
#endif
