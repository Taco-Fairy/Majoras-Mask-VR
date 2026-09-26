#pragma once
#include <algorithm>
namespace mmvr {
// Render-cadence motion, independent of native squash/shake and authored yaw.
struct FlowerCameraMotion {
    float progress = 0, drop = 0;
    bool entering = false;
    void Reset() {
        progress = drop = 0;
        entering = false;
    }
    void Update(bool flower, bool launching, float dt) {
        dt = std::clamp(dt, 0.f, .05f);
        if (flower && !launching) {
            if (!entering)
                progress = 0;
            entering = true;
            progress = std::min(1.f, progress + dt / 1.f);
            float t = progress * progress * (3 - 2 * progress);
            drop = 12.f * t;
        } else {
            entering = false;
            progress = 0;
            drop = std::max(0.f, drop - dt * 80.f);
        }
    }
    float Yaw() const {
        float t = progress * progress * (3 - 2 * progress);
        return entering ? 12.56637061436f * t : 0.f;
    }
};
} // namespace mmvr
