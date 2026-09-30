#pragma once
#include "lock_on_orbit.h"
inline void LockOnOrbitChecks() {
    constexpr float pi = 3.14159265358979323846f;
    auto delta = [](float a, float b) { return std::remainder(a - b, 2 * pi); };
    for (int hz : {72, 80, 90, 120, 200}) {
        for (float direction : {-1.f, 1.f}) {
            mmvr::LockOnOrbit orbit;
            float base = 0;
            orbit.Update(1, 0, -120, 0, 0, 10, base, 0, false);
            for (int i = 1; i <= hz * 2; ++i) {
                const float angle = direction * pi * i / hz;
                const float x = 120 * std::sin(angle), z = -120 * std::cos(angle);
                base += orbit.Update(1, x, z, 0, 0, 10 + double(i) / hz, base, 0, false);
                check(std::abs(delta(base, std::atan2(-x, -z))) < .00003f);
            }
            check(std::abs(base + direction * 2 * pi) < .0001f);
        }
        // Ease an off-center acquisition at every render rate, not on native ticks.
        mmvr::LockOnOrbit orbit;
        float base = 0;
        check(orbit.Update(1, 0, 0, 120, 0, 20, base, .2f, false) == 0);
        for (int i = 1; i <= hz; ++i) {
            const float turn = orbit.Update(1, 0, 0, 120, 0, 20 + double(i) / hz, base, .2f, false);
            check(std::abs(turn) <= 2 * pi / hz + .00001f);
            base += turn;
        }
        check(std::abs(delta(base + .2f, pi / 2)) < .0002f);
        // Head turns after acquisition are deliberately not counter-rotated.
        check(std::abs(orbit.Update(1, 0, 0, 120, 0, 21 + 1. / hz, base, .7f, false)) < .0002f);
        // Interpolate moving focus with the same native/render pair at 20 and 30 Hz.
        for (int nativeHz : {20, 30}) {
            mmvr::OrbitFocusInterpolation focus;
            focus.Sample(1, 0, {0, 120}, 1, true, false);
            for (int i = 1; i <= hz; ++i) {
                const double tick = double(i) * nativeHz / hz;
                const auto frame = uint64_t(std::floor(tick)) + 1;
                const float alpha = float(tick - std::floor(tick));
                const auto point = focus.Sample(1, frame, {float(frame), 120}, alpha, true, false);
                check(std::abs(point.x - float(tick)) < .00001f);
                const auto repeat = focus.Sample(1, frame, {float(frame), 120}, alpha, true, false);
                check(point.x == repeat.x && point.z == repeat.z);
            }
        }
    }
    mmvr::LockOnOrbit orbit;
    float base = 0;
    check(orbit.Update(1, 0, -100, 0, 0, 1, base, 0, false) == 0);
    base += orbit.Update(1, 0, -100, 2, 0, 1.01, base, 0, false);
    check(std::abs(base - std::atan2(2.f, 100.f)) < .00001f); // Moving target, stationary player.
    check(orbit.Update(1, 0, -100, 2, 0, 1.01, base, 0, false) == 0);
    check(orbit.Update(2, 20, -95, 0, 0, 1.02, base, 0, false) == 0);
    check(orbit.Update(2, 25, -95, 0, 0, 1.03, base, 0, true) == 0);
    check(orbit.Update(0, 30, -95, 0, 0, 1.04, base, 0, false) == 0);
    check(orbit.Update(2, 35, -95, 0, 0, 1.05, base, 0, false) == 0);
    check(orbit.Update(2, 40, -95, 0, 0, 3, base, 0, false) == 0);
    check(orbit.Update(2, 500, -95, 0, 0, 3.01, base, 0, false) == 0);
    check(orbit.Update(2, 505, -95, 500, 0, 3.02, base, 0, false) == 0);
    orbit.Reset();
    check(orbit.Update(1, 0, -10, 0, 0, 4, 0, 0, false) == 0);
    check(orbit.Update(1, NAN, 0, 0, 0, 4.01, 0, 0, false) == 0);
    check(orbit.Update(1, 0, -100, 0, 0, 4.02, 0, 0, false) == 0);
    // A through-target crossing is rate-limited rather than a half-turn in one frame.
    orbit.Reset(); orbit.Update(1, 20, 0, 0, 0, 5, -pi/2, 0, false);
    check(std::abs(orbit.Update(1, -20, 0, 0, 0, 5.01, -pi/2, 0, false)) <= .063f);
    mmvr::OrbitFocusInterpolation focus;
    check(focus.Sample(1, 10, {20, 100}, .5f, true, false).x == 20);
    check(focus.Sample(1, 11, {30, 100}, .5f, true, false).x == 25);
    check(focus.Sample(1, 13, {40, 100}, .5f, true, false).x == 40);
    check(focus.Sample(2, 14, {50, 100}, .5f, true, false).x == 50);
    check(focus.Sample(2, 15, {60, 100}, .5f, true, true).x == 60);
    check(focus.Sample(2, 16, {300, 100}, .5f, true, false).x == 300);
    check(focus.Sample(2, 17, {310, 100}, .5f, false, false).x == 310);
    focus.Reset();
    check(focus.Sample(2, 18, {320, 100}, .5f, true, false).x == 320);
    check(mmvr::Settings{}.Get(mmvr::Setting::LockOnOrbit) == 0);
    check(mmvr::SettingTab(int(mmvr::Setting::LockOnOrbit)) == mmvr::CombatTab);
}
