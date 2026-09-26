#pragma once
#include "first_person.h"
namespace mmvr {
inline XrVector3f CalibrateBowAim(XrVector3f direction, float yawDegrees, float pitchDegrees) {
    const float yaw = std::atan2(direction.x, direction.z) + yawDegrees * .01745329252f;
    const float pitch =
        std::clamp(std::atan2(direction.y, std::hypot(direction.x, direction.z)) + pitchDegrees * .01745329252f,
                   -1.553343f, 1.553343f);
    return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
}
// Native high-detail arrow: tip X=-396, nock X=2001, scale 0.01.
constexpr float ArrowTipX = -396.f, ArrowNockX = 2001.f, ArrowLength = (ArrowNockX - ArrowTipX) * .01f;
inline float LimitedArrowDraw(float worldDistance) {
    return std::clamp(worldDistance, 0.f, ArrowLength - 2.f);
}
inline Matrix ArrowPose(XrVector3f direction, XrVector3f nock) {
    auto m = YawPose(0);
    float horizontal = std::hypot(direction.x, direction.z);
    XrVector3f side = horizontal > .00001f ? XrVector3f{ direction.z / horizontal, 0, -direction.x / horizontal }
                                           : XrVector3f{ 1, 0, 0 };
    XrVector3f up{ side.y * direction.z - side.z * direction.y, side.z * direction.x - side.x * direction.z,
                   side.x * direction.y - side.y * direction.x };
    for (int k = 0; k < 3; ++k) {
        m.m[0][k] = -(&direction.x)[k] * .01f;
        m.m[1][k] = (&up.x)[k] * .01f;
        m.m[2][k] = -(&side.x)[k] * .01f;
        m.m[3][k] = (&nock.x)[k] + (&direction.x)[k] * ArrowNockX * .01f;
    }
    return m;
}
// Keep near markers modest and let apparent size decrease over distance.
// Above the far threshold a small angular floor preserves readability.
inline float ReticleWorldScale(float distance) {
    if (!std::isfinite(distance)) return 1.f;
    return std::max(1.f, std::max(0.f, distance) * .0012f);
}
// Shift the whole marker plane in front of the hit surface, including its corners.
inline void LiftReticle(Matrix& pose, XrVector3f normal) {
    float radius = 0;
    for (int row = 0; row < 2; ++row)
        radius += 3.f * std::abs(pose.m[row][0] * normal.x + pose.m[row][1] * normal.y + pose.m[row][2] * normal.z);
    for (int k = 0; k < 3; ++k)
        pose.m[3][k] += (&normal.x)[k] * (radius + 1.f);
}
} // namespace mmvr
