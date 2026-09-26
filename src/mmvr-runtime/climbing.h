#pragma once
#include "motion.h"
namespace mmvr {
struct ClimbHand {
    bool latched = false, armed = false;
    std::array<float, 3> velocity{};
    uint64_t epoch = 0;
    MotionHistory history;
    MotionPoint previous{};
    bool have = false;
    void Reset() {
        latched = false;
        armed = false;
        history.Reset();
        have = false;
        velocity = {};
    }
    std::array<float, 3> Update(MotionPoint point, uint64_t generation, float grip, bool tracked, bool surface) {
        if (epoch != generation || !tracked) {
            Reset();
            epoch = generation;
            return {};
        }
        if (grip < .25f) {
            latched = false;
            armed = true;
            history.Reset();
            have = false;
            velocity = {};
            return {};
        }
        if (!surface) {
            if (latched)
                Reset();
            else {
                history.Reset();
                have = false;
                velocity = {};
            }
            return {};
        }
        if (!latched && armed && grip > .7f && surface) {
            latched = true;
            armed = false;
            history.Reset();
            have = false;
            velocity = {};
        }
        if (!latched)
            return {};
        if (have && point.time == previous.time)
            return velocity; // The other eye repeats the same tracking sample.
        if (have) {
            float x = point.x - previous.x, y = point.y - previous.y, z = point.z - previous.z;
            if (point.time <= previous.time || point.time - previous.time > .15 ||
                x * x + y * y + z * z > .25f * .25f) {
                Reset();
                return {};
            }
        }
        previous = point;
        have = true;
        history.Push(point, generation);
        velocity = history.Velocity();
        for (auto& c : velocity)
            c = -c;
        return velocity;
    }
};
inline std::array<float, 3> ClimbVelocity(const std::array<float, 3>& left, const std::array<float, 3>& right, bool a,
                                          bool b, float gain, float cap) {
    std::array<float, 3> result{};
    int count = int(a) + int(b);
    if (!count)
        return result;
    for (int i = 0; i < 3; ++i)
        result[i] = ((a ? left[i] : 0) + (b ? right[i] : 0)) / count;
    return BoundedVelocity(result, gain, cap);
}
} // namespace mmvr
