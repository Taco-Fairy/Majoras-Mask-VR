#pragma once
#include "climbing.h"
namespace mmvr {
// Incremental anchor correction. Each accepted tracking sample is consumed once;
// collision/cap losses never accumulate into a later spring or teleport.
struct ClimbPull {
    ClimbHand grip;
    MotionPoint previous{};
    bool have = false;
    double dt = 0;
    void Reset() {
        grip.Reset();
        have = false;
        dt = 0;
    }
    void Rebase(uint64_t generation,bool controllerHeld) {
        const bool keep=grip.latched&&controllerHeld;
        Reset();grip.epoch=generation;grip.latched=keep;
        // First fresh position seeds the pull, so a moved controller never
        // yanks the restored body across the saved/current tracking boundary.
    }
    std::array<float, 3> Update(MotionPoint point, uint64_t epoch, float trigger, bool tracked, bool surface) {
        dt = 0;
        if (!std::isfinite(point.time) || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) {
            Reset();
            return {};
        }
        bool was = grip.latched;
        grip.Update(point, epoch, trigger, tracked, surface);
        if (!grip.latched) {
            have = false;
            return {};
        }
        if (!was || !have) {
            previous = point;
            have = true;
            return {};
        }
        if (point.time == previous.time)
            return {};
        dt = point.time - previous.time;
        std::array<float, 3> step{ previous.x - point.x, previous.y - point.y, previous.z - point.z };
        previous = point;
        return step;
    }
};
inline std::array<float, 3> ClimbPullStep(std::array<float, 3> a, std::array<float, 3> b, bool left, bool right,
                                          double dt, float gain, float cap, float deadzone) {
    if (!std::isfinite(dt) || dt <= 0 || dt > .15)
        return {};
    auto velocity = ClimbVelocity(a, b, left, right, 1, 100);
    for (float& c : velocity)
        c /= float(dt);
    float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
    if (speed < deadzone)
        return {};
    velocity = BoundedVelocity(velocity, gain, cap);
    for (float& c : velocity)
        c *= float(dt);
    return velocity;
}
} // namespace mmvr
