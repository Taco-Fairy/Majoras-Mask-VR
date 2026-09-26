#pragma once
#include <algorithm>
#include <cmath>
namespace mmvr {
// When the player is grounded on scene geometry, correct a materially lowered
// actor/interpolated root back toward its support. Keep ordinary floor-skin and
// tracking/interpolation offsets intact; room-scale headset motion is applied
// separately by the XR runtime.
// Callers must disable this for moving supports, airborne poses, rides,
// climbing, and scripted camera work.
inline float GroundCameraRootY(float visualY, float actorY, float floorY, bool stableSceneGround, bool interpolatedTravel = false) {
    constexpr float supportedRootTolerance = 64.f;
    constexpr float loweredRootThreshold = 2.f;
    const bool actorOnReportedFloor = std::isfinite(actorY) && std::isfinite(floorY) &&
                                      std::abs(actorY - floorY) <= supportedRootTolerance;
    if (!stableSceneGround || !actorOnReportedFloor || !std::isfinite(visualY)) return visualY;

    float correctedY = visualY;
    // Native rolling may lower the actor itself, or just its interpolated root.
    // Correct either case, without snapping small normal offsets to floorY.
    if (actorY < floorY - loweredRootThreshold) correctedY += floorY - actorY;
    if (!interpolatedTravel && correctedY < actorY - loweredRootThreshold)
        correctedY += actorY - correctedY;
    return correctedY;
}
// Evaluate the static support plane at the interpolated horizontal position.
// Rolling body offsets are animation, while slope travel must stay interpolated.
inline float RollingSupportY(float fallback, float actorY, float floorY,
                             float dx, float dz, float nx, float ny, float nz) {
    if (!std::isfinite(actorY) || !std::isfinite(floorY) || std::abs(actorY-floorY)>64.f ||
        !std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(nx) ||
        !std::isfinite(ny) || !std::isfinite(nz) || ny<=.1f) return fallback;
    const float support = floorY - (nx*dx + nz*dz)/ny;
    return std::isfinite(support) ? support : fallback;
}
// Smooth only a small change in walkable ground. The caller supplies the
// interpolated game root; real headset translation is added afterward.
class WalkStepCamera {
    bool ready = false, easing = false, previousSlope = false;
    float previousFloor = 0, previousTarget = 0, visualY = 0, slopeFloorDelta = 0;
public:
    void Reset() { ready = easing = previousSlope = false; slopeFloorDelta = 0; }
    float Update(float target, float floor, float dt, bool walking, bool reset, bool slopedGround = false) {
        if (reset || !walking || !std::isfinite(target) || !std::isfinite(floor) ||
            !std::isfinite(dt) || dt < 0 || dt > .1f) {
            Reset();
            return target;
        }
        if (!ready) { ready = true; visualY = previousTarget = target; previousFloor = floor; previousSlope = slopedGround; return target; }
        const float targetDelta = target - previousTarget;
        previousTarget = target;
        const float rise = floor - previousFloor;
        previousFloor = floor;
        // At a ramp crest, the support polygon can become flat before the
        // interpolated root catches up. Continue only a same-direction floor
        // increment no larger than the last measured native ramp increment.
        // A larger stair or a drop in the opposite direction keeps normal logic.
        const bool continuousExit = previousSlope && !slopedGround &&
            rise * slopeFloorDelta > 0.f &&
            std::abs(rise) <= std::abs(slopeFloorDelta) + .01f;
        if (slopedGround && std::abs(rise) > .01f) slopeFloorDelta = rise;
        if (!slopedGround) slopeFloorDelta = 0.f;
        previousSlope = slopedGround;
        // A slope's floor height advances at native simulation cadence, while
        // target is already interpolated at display cadence. Do not interpret
        // those floor samples as a staircase or restart/snap its easing.
        if ((slopedGround || continuousExit) && std::abs(targetDelta) <= 30.f) {
            visualY += targetDelta;
            if (easing) {
                visualY += (target - visualY) * (1.f - std::exp(-18.f * dt));
                if (std::abs(target - visualY) < .015f) easing = false;
            } else visualY = target;
            return visualY;
        }
        if (std::abs(rise) > 24.f || std::abs(target - visualY) > 30.f) {
            visualY = target; easing = false; return target;
        }
        if (std::abs(rise) > .01f) easing = true;
        if (easing) {
            visualY += (target - visualY) * (1.f - std::exp(-18.f * dt));
            if (std::abs(target - visualY) < .015f) easing = false;
        } else visualY = target;
        return visualY;
    }
};
}
