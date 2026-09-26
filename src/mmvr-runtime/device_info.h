#pragma once
#include <string>
namespace mmvr {
struct DeviceInfo {
    std::string runtime = "Not connected", headset = "Not detected", profiles[2]{ "Not active", "Not active" };
    float displayHz = 0;
    unsigned cadenceHz = 0, eyeWidth = 0, eyeHeight = 0;
};
// Render-thread diagnostic snapshot, shared with the VR menu.
inline DeviceInfo deviceInfo;
} // namespace mmvr
