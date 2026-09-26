#pragma once
#include "projection.h"
#include <cmath>
#include <algorithm>
namespace mmvr {
struct SwimDirection {
    float yaw = 0, pitch = 0;
    bool valid = false;
};
inline SwimDirection HeadSwimDirection(const Matrix& pose, float pitchLimitDegrees) {
    float x = -pose.m[2][0], y = -pose.m[2][1], z = -pose.m[2][2];
    float horizontal = std::hypot(x, z), length = std::hypot(horizontal, y);
    if (!std::isfinite(length) || length < .001f || !std::isfinite(pitchLimitDegrees))
        return {};
    float limit = std::clamp(pitchLimitDegrees, 20.f, 85.f) * .01745329252f;
    return { std::atan2(x, z), std::clamp(std::atan2(-y, horizontal), -limit, limit), true };
}
} // namespace mmvr
