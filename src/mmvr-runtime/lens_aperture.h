#pragma once
#include "projection.h"
#include <array>
#include <cmath>
#include <algorithm>
namespace mmvr {
// One aperture definition for actor visibility and the full-eye color surround.
// During XR rendering both eyes project the SAME head-local disc, not a circle
// independently centered in each eye texture. This accounts for IPD, asymmetric
// frusta and canted eye poses. Coordinates below use top-left image convention.
// Coordinates are fractions of the complete eye image, independent of HUD size.
struct LensAperture {
    float cx = .5f, cy = .5f, rx = .31f, ry = .34f;
};
inline constexpr int LensSegments = 96;
struct LensPolygon {
    std::array<float, 2> center{ .5f, .5f };
    std::array<std::array<float, 2>, LensSegments> edge{};
};
inline LensPolygon binocularLens{};
inline bool binocularLensActive = false;
inline void ClearBinocularLens() {
    binocularLensActive = false;
}
inline void SetBinocularLens(const XrFovf& fov, const XrPosef& head, const XrPosef& eye) {
    const auto transform = Multiply(PoseMatrix(head), InversePose(PoseMatrix(eye)));
    const float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
    const float d = std::tan(fov.angleDown), u = std::tan(fov.angleUp);
    if (!(r > l && u > d)) {
        ClearBinocularLens();
        return;
    }
    auto project = [&](float x, float y) {
        float p[3]{};
        for (int j = 0; j < 3; ++j)
            p[j] = x * transform.m[0][j] + y * transform.m[1][j] - transform.m[2][j] + transform.m[3][j];
        const float z = std::max(.01f, -p[2]);
        return std::array<float, 2>{ (p[0] / z - l) / (r - l), (u - p[1] / z) / (u - d) };
    };
    binocularLens.center = project(0, 0);
    for (int i = 0; i < LensSegments; ++i) {
        const float angle = i * 6.28318530718f / LensSegments;
        binocularLens.edge[i] = project(.75f * std::cos(angle), -.75f * std::sin(angle));
    }
    binocularLensActive = true;
}
inline LensAperture GetLensAperture() {
    return {};
}
inline std::array<float, 2> LensBoundary(int i) {
    if (binocularLensActive)
        return binocularLens.edge[(i % LensSegments + LensSegments) % LensSegments];
    auto a = GetLensAperture();
    float t = float(i) * 6.28318530718f / LensSegments;
    return { a.cx + std::cos(t) * a.rx, a.cy + std::sin(t) * a.ry };
}
inline std::array<float, 2> LensCenter() {
    return binocularLensActive ? binocularLens.center : std::array<float, 2>{ .5f, .5f };
}
// Shared reference used by render tests; avoid boundary samples for raster rules.
inline bool LensInside(float x, float y) {
    if (binocularLensActive) {
        // Convex projected disc. This reference also checks asymmetric GPU masks.
        float sign = 0;
        for (int i = 0; i < LensSegments; ++i) {
            const auto a = LensBoundary(i), b = LensBoundary(i + 1);
            const float cross = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
            if (i == 0)
                sign = cross;
            else if (cross * sign < 0)
                return false;
        }
        return true;
    }
    auto a = GetLensAperture();
    x = (x - a.cx) / a.rx;
    y = (y - a.cy) / a.ry;
    return x * x + y * y < 1;
}
} // namespace mmvr
