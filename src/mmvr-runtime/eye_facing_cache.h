#pragma once
#include "projection.h"
#include <cstring>
namespace mmvr {
// Derived eye-facing basis only: never filters a pose or delays an update.
class EyeFacingCache {
    Matrix world{}, basis{};
    XrPosef eye{}, origin{};
    bool valid = false;

  public:
    const Matrix& Get(const Matrix& newWorld, const XrPosef& newEye, const XrPosef& newOrigin) {
        if (!valid || std::memcmp(&world, &newWorld, sizeof(world)) || std::memcmp(&eye, &newEye, sizeof(eye)) ||
            std::memcmp(&origin, &newOrigin, sizeof(origin))) {
            world = newWorld;
            eye = newEye;
            origin = newOrigin;
            valid = true;
            basis = Multiply(Multiply(PoseMatrix(eye), InversePose(PoseMatrix(origin))), InversePose(world));
        }
        return basis;
    }
};
} // namespace mmvr
