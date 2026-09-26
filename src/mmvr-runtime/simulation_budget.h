#pragma once
#include <algorithm>
#include <cmath>
namespace mmvr {
// OpenXR alone waits for presentation. This clock bounds how many interpolated
// renders fit in a native game tick, so missed display frames cannot slow audio.
struct SimulationBudget {
    double deadline = 0, period = 0, estimate = 0, lastBegin = 0;
    int renders = 0, limit = 0;
    double presentationTime = 0, presentationPeriod = 0;
    void Reset() {
        deadline = period = estimate = lastBegin = 0;
        renders = limit = 0;
        presentationTime = presentationPeriod = 0;
    }
    void Begin(double now, int nativeHz, int displayHz) {
        double next = 1.0 / std::max(1, nativeHz);
        // Streaming runtimes can predict several native ticks ahead. That is not
        // a clock reset: restarting the deadline there makes 60 Hz menus run at
        // the headset rate and overproduces audio. Reset only on real clock/rate
        // discontinuities or a missed native-time budget.
        if (period != next || deadline == 0 || now - deadline > .1 || now < lastBegin) {
            deadline = now + next;
            estimate = 1.0 / std::max(1, displayHz);
        } else
            deadline += next;
        lastBegin = now;
        period = next;
        renders = 0;
        presentationTime = presentationPeriod = 0;
        limit = std::max(2, int(std::ceil(double(displayHz) / nativeHz)) + 2);
    }
    bool FollowingPresentationFits() const {
        return std::isfinite(presentationTime) && std::isfinite(presentationPeriod) &&
               presentationTime > 0 && presentationPeriod > 0 &&
               renders + 1 < limit && presentationTime + presentationPeriod < deadline;
    }
    bool More(double now) const {
        // Once a display target is known, stop this native tick on that same
        // timeline. Test the next full display interval: a half-period lookahead
        // admits targets past the tick end, clamping camera travel and causing
        // alternating short/long movement steps despite stable display FPS.
        // Using wall time here would keep replaying alpha=1 while
        // OpenXR is already presenting the following native interval.
        const double next = presentationTime > 0
            ? presentationTime + presentationPeriod : now + estimate * .5;
        return renders < limit && (renders == 0 || next < deadline);
    }
    float Alpha(double now) const {
        return float(std::clamp((now + estimate - (deadline - period)) / period, 0.0, 1.0));
    }
    float PresentationAlpha(double target, double displayPeriod, double now) {
        if (!std::isfinite(target) || !std::isfinite(displayPeriod) || target <= 0 || displayPeriod <= 0)
            return Alpha(now);
        presentationTime = target;
        presentationPeriod = displayPeriod;
        return float(std::clamp((target - (deadline - period)) / period, 0.0, 1.0));
    }
    void Rendered(double seconds) {
        ++renders;
        if (std::isfinite(seconds) && seconds > 0)
            estimate = std::clamp(estimate * .75 + seconds * .25, .001, .1);
    }
};
} // namespace mmvr
