#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr {
struct OrbitFocus { float x = 0, z = 0; };

// The actor focus advances on native ticks, but the camera runs at XR cadence.
// Use the same previous/current pair and alpha as the rendered scene. Reset
// instead of interpolating across a new target, skipped tick or teleport.
class OrbitFocusInterpolation {
    uintptr_t target = 0;
    uint64_t frame = 0;
    OrbitFocus previous{}, current{};
    bool paired = false;
public:
    void Reset() { target = 0; paired = false; }
    OrbitFocus Sample(uintptr_t nextTarget, uint64_t nextFrame, OrbitFocus point,
                      float alpha, bool interpolated, bool reset) {
        if (!nextTarget || !std::isfinite(point.x) || !std::isfinite(point.z)) {
            Reset(); return point;
        }
        if (reset || target != nextTarget || nextFrame < frame) {
            previous = current = point; paired = false;
        } else if (nextFrame != frame) {
            previous = current; current = point;
            paired = nextFrame == frame + 1 &&
                     std::hypot(current.x - previous.x, current.z - previous.z) <= 200.f;
        }
        target = nextTarget; frame = nextFrame;
        if (!interpolated || !paired || !std::isfinite(alpha)) return current;
        alpha = std::clamp(alpha, 0.f, 1.f);
        return {previous.x + (current.x - previous.x) * alpha,
                previous.z + (current.z - previous.z) * alpha};
    }
};

// Horizontal target centering keeps the floor level. Capture the physical/manual
// gaze offset only on acquisition: subsequent head and stick turns stay free.
// Acquisition eases into place; established tracking has no added low-pass lag.
class LockOnOrbit {
    static constexpr float TwoPi = 6.28318530718f;
    uintptr_t target = 0;
    float playerX = 0, playerZ = 0, targetX = 0, targetZ = 0;
    float gazeAtLock = 0, acquisitionOffset = 0;
    double time = 0;
    static float Wrapped(float angle) { return std::remainder(angle, TwoPi); }
public:
    void Reset() { target = 0; }
    float Update(uintptr_t nextTarget, float x, float z, float tx, float tz,
                 double seconds, float currentBaseYaw, float physicalGazeYaw, bool reset) {
        if (!nextTarget || !std::isfinite(x) || !std::isfinite(z) ||
            !std::isfinite(tx) || !std::isfinite(tz) || !std::isfinite(seconds) ||
            !std::isfinite(currentBaseYaw) || !std::isfinite(physicalGazeYaw) ||
            std::hypot(tx - x, tz - z) < 12.f) {
            Reset();
            return 0;
        }
        // A second eye/callback at the same predicted time must not turn twice.
        if (!reset && target == nextTarget && seconds == time) return 0;
        const double dt = seconds - time;
        const bool discontinuity = reset || target != nextTarget || dt <= 0 || dt > .1 ||
            std::hypot(x - playerX, z - playerZ) > 200.f ||
            std::hypot(tx - targetX, tz - targetZ) > 200.f;
        const float bearing = std::atan2(tx - x, tz - z);
        if (discontinuity) {
            gazeAtLock = physicalGazeYaw;
            acquisitionOffset = Wrapped(bearing - gazeAtLock - currentBaseYaw);
        }
        target = nextTarget;
        playerX = x; playerZ = z; targetX = tx; targetZ = tz; time = seconds;
        if (discontinuity) return 0;
        acquisitionOffset *= std::exp(-16.f * float(dt));
        if (std::abs(acquisitionOffset) < .0001f) acquisitionOffset = 0;
        const float correction = Wrapped(bearing - gazeAtLock - acquisitionOffset - currentBaseYaw);
        // Avoid sudden flips on crossing the target or changing its focus point.
        const float maxTurn = TwoPi * float(dt);
        return std::clamp(correction, -maxTurn, maxTurn);
    }
};
} // namespace mmvr
