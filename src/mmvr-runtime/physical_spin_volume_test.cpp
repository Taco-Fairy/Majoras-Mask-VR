#include "physical_spin_volume.h"
#include "spin_attack.h"

#include <array>
#include <cmath>
#include <iostream>

int main() {
    const auto volume = mmvr::MakePhysicalSpinVolume({0, 0, 0}, {8, 12, 0}, {30, 12, 0}, 2, 60);
    if (!volume.valid || std::abs(volume.radius - 32.f) > .001f || std::abs(volume.centerY - 12.f) > .001f ||
        std::abs(volume.Bottom() + 18.f) > .001f || std::abs(volume.Top() - 42.f) > .001f)
        return 1;

    // A physical spin covers the complete circumference and the full Link-height
    // band, while a point beyond measured sword reach remains outside.
    constexpr std::array<mmvr::SpinPoint, 6> points = {
        mmvr::SpinPoint{31, 12, 0}, mmvr::SpinPoint{-31, 12, 0}, mmvr::SpinPoint{0, 12, 31},
        mmvr::SpinPoint{0, 12, -31}, mmvr::SpinPoint{0, -18, 0}, mmvr::SpinPoint{0, 42, 0},
    };
    for (const mmvr::SpinPoint& point : points) {
        if (volume.DistanceSquared(point) != 0.f)
            return 2;
    }
    if (volume.DistanceSquared({33, 12, 0}) <= 0.f || volume.DistanceSquared({0, 42.1f, 0}) <= 0.f)
        return 3;

    auto magicVolume = volume;
    if (!magicVolume.ExtendRadius(60.f) || std::abs(magicVolume.radius - 60.f) > .001f ||
        magicVolume.DistanceSquared({-60, 12, 0}) != 0.f || magicVolume.DistanceSquared({-60.1f, 12, 0}) <= 0.f)
        return 4;
    if (magicVolume.ExtendRadius(INFINITY) || std::abs(magicVolume.radius - 60.f) > .001f)
        return 5;

    if (mmvr::MakePhysicalSpinVolume({0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0, 0).valid)
        return 6;
    if (mmvr::MakePhysicalSpinVolume({0, 0, 0}, {0, 0, 0}, {INFINITY, 0, 0}, 0, 60).valid)
        return 7;

    // A physical turn must be same-direction, exceed the angular threshold,
    // and re-arm only after the native attack cooldown.
    mmvr::SpinAttack attack;
    float yaw = 0.f;
    for (int frame = 1; frame <= 100; ++frame) {
        const double time = frame / 90.0;
        yaw = static_cast<float>(time * 6.0);
        attack.Update(time, 1, true, 0.f, yaw, .5f, false, 2.f);
    }
    if (attack.TakeTier() < 0)
        return 8;
    for (int frame = 101; frame <= 125; ++frame) {
        const double time = frame / 90.0;
        yaw = static_cast<float>(time * 6.0);
        attack.Update(time, 1, true, 0.f, yaw, .5f, false, 2.f);
    }
    if (attack.TakeTier() >= 0)
        return 9;

    std::cout << "physical spin volume checks passed\n";
    return 0;
}
