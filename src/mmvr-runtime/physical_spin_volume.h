#pragma once

#include <algorithm>
#include <cmath>

namespace mmvr {
struct SpinPoint {
    float x = 0.f, y = 0.f, z = 0.f;
};

// A body-height cylinder for a physical spin attack. Its horizontal radius is
// derived only from the tracked sword's world-space endpoints, so the spin
// fills the sword's sweep without inventing extra reach.
struct PhysicalSpinVolume {
    float centerX = 0.f, centerY = 0.f, centerZ = 0.f;
    float radius = 0.f, height = 0.f;
    bool valid = false;

    float Bottom() const { return centerY - height * .5f; }
    float Top() const { return centerY + height * .5f; }

    bool ExtendRadius(float nativeRadius) {
        if (!valid || !std::isfinite(nativeRadius) || nativeRadius <= 0.f)
            return false;
        radius = std::max(radius, nativeRadius);
        return true;
    }

    float DistanceSquared(const SpinPoint& point) const {
        if (!valid)
            return INFINITY;
        const float radial = std::max(0.f, std::hypot(point.x - centerX, point.z - centerZ) - radius);
        const float vertical = std::max(0.f, std::abs(point.y - centerY) - height * .5f);
        return radial * radial + vertical * vertical;
    }
};

inline PhysicalSpinVolume MakePhysicalSpinVolume(const SpinPoint& origin, const SpinPoint& bladeBase,
                                                  const SpinPoint& bladeTip, float bladeHalfWidth,
                                                  float bodyHeight) {
    PhysicalSpinVolume volume{};
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
        !std::isfinite(bladeBase.x) || !std::isfinite(bladeBase.y) || !std::isfinite(bladeBase.z) ||
        !std::isfinite(bladeTip.x) || !std::isfinite(bladeTip.y) || !std::isfinite(bladeTip.z) ||
        !std::isfinite(bladeHalfWidth) || !std::isfinite(bodyHeight) || bladeHalfWidth < 0.f || bodyHeight <= 0.f)
        return volume;

    const float baseRadius = std::hypot(bladeBase.x - origin.x, bladeBase.z - origin.z);
    const float tipRadius = std::hypot(bladeTip.x - origin.x, bladeTip.z - origin.z);
    volume.centerX = origin.x;
    volume.centerY = (bladeBase.y + bladeTip.y) * .5f;
    volume.centerZ = origin.z;
    volume.radius = std::max(baseRadius, tipRadius) + bladeHalfWidth;
    volume.height = bodyHeight;
    volume.valid = std::isfinite(volume.radius) && volume.radius > 0.f;
    return volume;
}
} // namespace mmvr
