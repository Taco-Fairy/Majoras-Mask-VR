#pragma once
#include "fixed_history.h"
#include <deque>
inline void FixedHistoryTests() {
    struct Sample {
        double time;
        float x, y, z;
    };
    mmvr::FixedHistory<Sample> actual;
    std::deque<Sample> expected;
    auto compare = [&] {
        check(actual.size() == expected.size() && actual.empty() == expected.empty());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto& a = actual[i];
            const auto& b = expected[i];
            check(a.time == b.time && a.x == b.x && a.y == b.y && a.z == b.z);
        }
    };
    // Wrap, overflow, duplicate replacement, keep-last contact, reset and copy.
    for (unsigned tick = 0; tick < 20000; ++tick) {
        if (tick % 97 == 0) {
            actual.clear();
            expected.clear();
        }
        if (!expected.empty() && tick % 7 == 0) {
            actual.pop_back();
            expected.pop_back();
        }
        Sample value{ tick / 120., float(tick), float(tick * 2), -float(tick) };
        actual.push_back(value);
        if (expected.size() == 64)
            expected.pop_front();
        expected.push_back(value);
        if (tick % 5 == 0) {
            actual.pop_front();
            expected.pop_front();
        }
        compare();
        if (tick % 131 == 0 && !expected.empty()) {
            auto last = actual.back();
            actual.clear();
            actual.push_back(last);
            auto reference = expected.back();
            expected.clear();
            expected.push_back(reference);
            compare();
        }
        auto copy = actual;
        check(copy.size() == actual.size());
        if (!actual.empty())
            check(copy.front().time == actual.front().time && copy.back().time == actual.back().time);
    }
    // Exactly the live render sample/time pruning at representative headset rates.
    for (int hz : { 72, 80, 90, 120, 144 }) {
        actual.clear();
        expected.clear();
        for (int tick = 0; tick < 1000; ++tick) {
            double time = tick / double(hz);
            Sample value{ time, float(tick), 0, 0 };
            actual.push_back(value);
            expected.push_back(value);
            while (actual.size() > 32 || time - actual.front().time > .18)
                actual.pop_front();
            while (expected.size() > 32 || time - expected.front().time > .18)
                expected.pop_front();
            compare();
        }
    }
}
