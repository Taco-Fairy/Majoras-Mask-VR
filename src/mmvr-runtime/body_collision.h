#pragma once
#include <algorithm>
#include <cmath>
namespace mmvr {
// Sweep the native upright body cylinder against a solid prop cylinder. Keep
// the full segment: endpoint-only overlap lets fast room-scale steps tunnel.
inline bool BodyCylinderSweep(float fromX, float fromY, float fromZ,
                              float toX, float toY, float toZ,
                              float bodyRadius, float bodyHeight,
                              float solidX, float solidY, float solidZ,
                              float solidRadius, float solidHeight) {
    if (bodyRadius <= 0 || bodyHeight <= 0 || solidRadius <= 0 || solidHeight <= 0)
        return false;
    const float x = fromX - solidX, z = fromZ - solidZ;
    const float dx = toX - fromX, dz = toZ - fromZ, dy = toY - fromY;
    const float radius = bodyRadius + solidRadius;
    const float start2 = x * x + z * z, radius2 = radius * radius;
    // Native simulation may leave a small overlap. Allow movement out rather
    // than trapping the player, but do not permit crossing deeper through it.
    if (start2 < radius2 && fromY < solidY + solidHeight && fromY + bodyHeight > solidY) {
        const float end2 = (x + dx) * (x + dx) + (z + dz) * (z + dz);
        if (x * dx + z * dz >= 0 && end2 > start2)
            return false;
        return dx * dx + dz * dz > 0;
    }
    float enter = 0, exit = 1;
    const float a = dx * dx + dz * dz, b = x * dx + z * dz;
    if (a < 1e-8f) {
        if (start2 >= radius2) return false;
    } else {
        const float discriminant = b * b - a * (start2 - radius2);
        if (discriminant <= 0) return false; // Tangency does not enter the solid.
        const float root = std::sqrt(discriminant);
        enter = std::max(enter, (-b - root) / a);
        exit = std::min(exit, (-b + root) / a);
    }
    const float low = solidY - bodyHeight, high = solidY + solidHeight;
    if (std::abs(dy) < 1e-8f) {
        if (fromY <= low || fromY >= high) return false;
    } else {
        float t0 = (low - fromY) / dy, t1 = (high - fromY) / dy;
        if (t0 > t1) std::swap(t0, t1);
        enter = std::max(enter, t0);
        exit = std::min(exit, t1);
    }
    return enter < exit;
}
} // namespace mmvr
