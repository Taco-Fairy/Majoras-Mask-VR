#pragma once
#include <openxr/openxr.h>
#include <cmath>
namespace mmvr {
// Pose of a level, world-stable screen, 3 metres along head yaw. Looking up/down
// while recentering cannot tilt the screen. Position is captured only on recenter.
inline XrPosef TheaterPose(const XrPosef& head, float distance = 3.f) {
    const auto& q = head.orientation;
    const float forwardX = -2.f * (q.x * q.z + q.w * q.y);
    const float forwardZ = -(1.f - 2.f * (q.x * q.x + q.y * q.y));
    const float yaw = std::atan2(-forwardX, -forwardZ);
    XrPosef screen{};
    screen.orientation = { 0, std::sin(yaw / 2), 0, std::cos(yaw / 2) };
    screen.position = { head.position.x - distance * std::sin(yaw), head.position.y,
                        head.position.z - distance * std::cos(yaw) };
    return screen;
}
} // namespace mmvr
