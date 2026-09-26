#pragma once
#include "item_smoothing.h"
inline void CheckItemSmoothing() {
    using namespace mmvr;
    TrackingFrame f;
    f.head.orientation.w = f.origin.orientation.w = 1;
    for (int i = 0; i < 2; ++i) {
        f.hands[i].orientation.w = f.aims[i].orientation.w = 1;
        f.handValid[i] = f.handTracked[i] = f.aimValid[i] = true;
        f.aims[i].position.z = -.08f;
    }
    ItemPoseSmoother smoother;
    float energy = 0;
    for (int sample = 0; sample < 180; ++sample) {
        f.timeSeconds = sample / 90.;
        f.hands[0].position.x = (sample % 2 ? 1 : -1) * .001f;
        f.aims[0].position.x = f.hands[0].position.x;
        auto out = smoother.Update(f, 12, true, 0);
        if (sample > 20)
            energy += out.hands[0].position.x * out.hands[0].position.x;
        check(close(out.aims[0].position.x, out.hands[0].position.x));
        check(close(out.aims[0].position.z - out.hands[0].position.z, -.08f));
        check(close(out.head.orientation.w, 1));
    }
    check(std::sqrt(energy / 159) < .00055f);
    // Movement catches up quickly at different headset rates; the same correction moves aim and grip.
    for (int hz : { 72, 90, 120 }) {
        smoother.Reset();
        f.hands[0].position = {};
        f.timeSeconds = 0;
        smoother.Update(f, 12, true, 0);
        for (int sample = 1; sample < 40; ++sample) {
            f.timeSeconds = double(sample) / hz;
            f.hands[0].position.x = float(f.timeSeconds) * 1.2f;
            auto out = smoother.Update(f, 12, true, 0);
            check(std::abs(out.hands[0].position.x - f.hands[0].position.x) < .003f);
        }
    }
    // Hemisphere flips describe the same rotation. Lost tracking, recenter, menu resume, context and gaps reset
    // immediately.
    smoother.Reset();
    f.timeSeconds = 0;
    f.hands[0] = { { 0, 0, 0, 1 }, {} };
    smoother.Update(f, 40, true, 0);
    f.timeSeconds = .01;
    f.hands[0].orientation.w = -1;
    auto out = smoother.Update(f, 40, true, 0);
    check(std::abs(out.hands[0].orientation.w) > .9999f);
    for (int mode = 0; mode < 6; ++mode) {
        smoother.Reset();
        f.epoch = 0;
        f.originEpoch = 0;
        f.timeSeconds = 0;
        f.hands[0].position.x = 0;
        f.handTracked[0] = true;
        smoother.Update(f, 12, true, 0);
        if (mode == 0)
            ++f.epoch;
        if (mode == 1)
            ++f.originEpoch;
        if (mode == 2) {
            f.handTracked[0] = false;
            f.timeSeconds = .005;
            smoother.Update(f, 12, true, 0);
            f.handTracked[0] = true;
        }
        if (mode == 3)
            smoother.Update(f, 12, false, 0);
        f.timeSeconds = mode == 4 ? .2 : .01;
        f.hands[0].position.x = .1f;
        out = smoother.Update(f, 12, true, mode == 5 ? 1 : 0);
        check(close(out.hands[0].position.x, .1f));
    }
    out = smoother.Update(f, 0, true, 0);
    check(close(out.hands[0].position.x, f.hands[0].position.x));
    // Replaying an eye must not advance the filter a second time.
    smoother.Reset();
    f.timeSeconds = 0;
    smoother.Update(f, 12, true, 0);
    f.timeSeconds = .01;
    f.hands[0].position.x += .001f;
    auto a = smoother.Update(f, 12, true, 0), b = smoother.Update(f, 12, true, 0);
    check(close(a.hands[0].position.x, b.hands[0].position.x));
    // Grip orientation and the runtime's aim offset undergo one rigid transform.
    f.timeSeconds = .02;
    f.hands[0].orientation = { 0, std::sin(.03f), 0, std::cos(.03f) };
    f.aims[0] = f.hands[0];
    f.aims[0].position.z -= .08f;
    out = smoother.Update(f, 12, true, 0);
    auto rel = Multiply(PoseMatrix(out.aims[0]), InversePose(PoseMatrix(out.hands[0])));
    auto nativeRel = Multiply(PoseMatrix(f.aims[0]), InversePose(PoseMatrix(f.hands[0])));
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            check(close(rel.m[i][j], nativeRel.m[i][j]));
}
