#pragma once
#include "eye_facing_cache.h"
inline void CheckEyeFacingCache() {
    using namespace mmvr;
    EyeFacingCache cache;
    XrPosef eye{ { 0, 0, 0, 1 }, { .03f, 1.6f, 0 } }, origin{ { 0, 0, 0, 1 }, { .1f, 0, 0 } };
    auto world = YawPose(.4f, 20, 30, 40);
    for (int i = 0; i < 200; ++i) {
        if (i % 4 == 0) {
            float angle = i * .019f;
            eye.orientation = { 0, std::sin(angle / 2), 0, std::cos(angle / 2) };
        } else if (i % 4 == 1)
            origin.position.z = i * .031f;
        else if (i % 4 == 2)
            world = YawPose(i * .026f, 20 + i, 30, 40);
        // Fourth iteration must reuse the basis without drifting or retaining a stale input.
        auto expected = Multiply(Multiply(PoseMatrix(eye), InversePose(PoseMatrix(origin))), InversePose(world));
        const auto& result = cache.Get(world, eye, origin);
        check(std::memcmp(&expected, &result, sizeof(Matrix)) == 0);
    }
}
