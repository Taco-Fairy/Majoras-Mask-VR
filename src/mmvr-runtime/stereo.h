#pragma once
#include "projection.h"
#include <DirectXMath.h>
#include <cstring>
namespace mmvr {
// Thin desktop adapter. All pose, world-scale, projection and fog math is shared.
inline DirectX::XMMATRIX EyeProjection(const XrPosef& eye, const XrFovf& fov, const XrPosef& origin, float n = 1.f,
                                       float f = 30000.f) {
    const auto matrix = EyeProjectionMatrix(eye, fov, origin, n, f);
    DirectX::XMFLOAT4X4 storage;
    static_assert(sizeof(storage) == sizeof(matrix));
    std::memcpy(&storage, &matrix, sizeof(storage));
    return DirectX::XMLoadFloat4x4(&storage);
}
} // namespace mmvr
