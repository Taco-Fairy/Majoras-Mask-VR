#pragma once
#include "motion.h"
namespace mmvr {
inline std::array<float, 3> AssistThrow(std::array<float, 3> velocity, bool bomb, float lift, float angle,
                                        float nutGain, float cap, bool moving = true) {
    const float horizontal = std::hypot(velocity[0], velocity[2]);
    if (bomb) {
        // Preserve intentional straight-down drops. A horizontal throw gets the native game's rising arc.
        if (moving && horizontal >= 30.f && velocity[1] > -horizontal) {
            const float target = std::max(lift * 40.f, horizontal * std::tan(angle * .01745329252f));
            velocity[1] = std::max(velocity[1], target);
        }
    } else
        for (auto& v : velocity)
            v *= nutGain;
    return BoundedVelocity(velocity, 1, cap * 40.f);
}
} // namespace mmvr
