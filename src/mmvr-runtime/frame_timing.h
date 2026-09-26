#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace mmvr {
struct FrameTimingWindow {
    std::array<double, 2048> work{};
    size_t count = 0, overBudget = 0, over2xBudget = 0, over50Ms = 0;
    double sum = 0, maximum = 0;
    void Reset() {
        count = overBudget = over2xBudget = over50Ms = 0;
        sum = maximum = 0;
    }
    void Add(double ms, double budget) {
        if (!std::isfinite(ms) || ms < 0 || !std::isfinite(budget) || budget <= 0 || count == work.size())
            return;
        // All aggregates describe the same accepted, bounded sample population.
        work[count++] = ms;
        sum += ms;
        maximum = std::max(maximum, ms);
        overBudget += ms > budget;
        over2xBudget += ms > 2 * budget;
        over50Ms += ms > 50;
    }
    double Mean() const {
        return count ? sum / count : 0;
    }
    double Max() const {
        return maximum;
    }
    double P95() const {
        if (!count)
            return 0;
        auto sorted = work;
        std::sort(sorted.begin(), sorted.begin() + count);
        return sorted[size_t(std::ceil(count * .95)) - 1];
    }
};
} // namespace mmvr
