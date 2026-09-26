#pragma once
#include "projection.h"
#include <algorithm>
namespace mmvr {
inline XrFovf MagnifiedFov(XrFovf f, float zoom) {
    zoom = std::clamp(zoom, .25f, 16.f);
    for (float* a : { &f.angleLeft, &f.angleRight, &f.angleDown, &f.angleUp })
        *a = std::atan(std::tan(*a) / zoom);
    return f;
}
struct ScopeAngles {
    float yaw, pitch, fade;
};
inline ScopeAngles BoundScope(float yaw, float pitch) {
    constexpr float y = 16000.f * 3.14159265359f / 32768.f, p = 12000.f * 3.14159265359f / 32768.f;
    float cy = std::clamp(yaw, -y, y), cp = std::clamp(pitch, -p, p);
    return { cy, cp, std::clamp(std::max(std::abs(yaw - cy), std::abs(pitch - cp)) / .2617993878f, 0.f, 1.f) };
}
// Native photographs crop 160x112 from a 320x240 perspective image.
inline XrVector2f PhotoHalfTangents(float verticalFov, float aspect) {
    float t = std::tan(verticalFov * .00872664626f);
    return { t * aspect * .5f, t * (112.f / 240.f) };
}
inline XrVector2f TangentPixel(float x, float y, const XrFovf& f, float width, float height) {
    return { (x - std::tan(f.angleLeft)) / (std::tan(f.angleRight) - std::tan(f.angleLeft)) * width,
             (std::tan(f.angleUp) - y) / (std::tan(f.angleUp) - std::tan(f.angleDown)) * height };
}
} // namespace mmvr
