#pragma once
namespace mmvr {
struct TrackingFrame;
}
namespace mmvrgame {
void UpdateArmRun(const mmvr::TrackingFrame&);
void ClearArmRun();
float ArmRunMultiplier();
} // namespace mmvrgame
