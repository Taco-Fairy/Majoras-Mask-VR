#pragma once
#include "first_person.h"
#include <atomic>
namespace mmvr {
// Calibration is captured once at recenter, never followed per-frame while
// crouching. Without a runtime floor, the explicit eye-height setting is the
// predicted floor distance. This value is physical tracking, not save-state data.
inline std::atomic<float> calibratedFloorEyeHeight{0};
inline void SetWorldScaleFloorHeight(float metres) {
    calibratedFloorEyeHeight.store(std::isfinite(metres) && metres >= .4f && metres <= 2.5f ? metres : 0.f,
                                   std::memory_order_relaxed);
}
// Native form order: Fierce Deity, Goron, Zora, Deku, human.
inline float WorldTrackingScale(const Settings& settings, int form, float standingHeight) {
    if (settings.Get(Setting::WorldScaleCalibration) < .5f || form < 0 || form > 4)
        return 1.f;
    constexpr Setting size[] = {Setting::DeityWorldSize, Setting::GoronWorldSize,
        Setting::ZoraWorldSize, Setting::DekuWorldSize, Setting::HumanWorldSize};
    const float measured = calibratedFloorEyeHeight.load(std::memory_order_relaxed);
    const float metres = measured > 0.f ? measured : settings.Get(Setting::StandingEyeHeight) * .01f;
    return std::clamp(standingHeight / (40.f * metres * settings.Get(size[form]) * .01f), .1f, 5.f);
}
inline TrackingFrame ScaleWorldTracking(const TrackingFrame& input, float factor) {
    TrackingFrame result = input;
    result.trackingScale = factor;
    if (factor == 1.f) return result;
    auto scale = [&](XrPosef& pose) {
        pose.position.x = input.origin.position.x + (pose.position.x - input.origin.position.x) * factor;
        pose.position.y = input.origin.position.y + (pose.position.y - input.origin.position.y) * factor;
        pose.position.z = input.origin.position.z + (pose.position.z - input.origin.position.z) * factor;
    };
    scale(result.head);
    for (int h = 0; h < 2; ++h) {
        scale(result.hands[h]); scale(result.aims[h]);
        result.handVelocity[h].x *= factor;
        result.handVelocity[h].y *= factor;
        result.handVelocity[h].z *= factor;
    }
    return result;
}
} // namespace mmvr
