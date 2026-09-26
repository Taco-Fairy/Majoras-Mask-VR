#pragma once
#include "settings.h"
namespace mmvr {
inline unsigned FrameRateLimit(const Settings& settings) {
    constexpr unsigned limits[]{ 0, 90, 80, 72 };
    return limits[int(settings.Get(Setting::FrameRateCap))];
}
inline const char* FrameRateLimitLabel(const Settings& settings) {
    constexpr const char* labels[]{ "Uncapped", "90 FPS", "80 FPS", "72 FPS" };
    return labels[int(settings.Get(Setting::FrameRateCap))];
}
// A fallback for runtimes that cannot switch the display to the requested rate.
// Never accumulate catch-up frames, and reset across preference/clock changes.
struct RenderFrameLimit {
    double previous = 0;
    unsigned limit = 0;
    bool valid = false;
    void Reset() {
        valid = false;
        limit = 0;
    }
    double Delay(double now, unsigned cap) {
        if (!cap || !std::isfinite(now)) {
            Reset();
            return 0;
        }
        if (!valid || cap != limit || now < previous) {
            previous = now;
            limit = cap;
            valid = true;
            return 0;
        }
        return std::max(0.0, 1.0 / cap - (now - previous));
    }
    void Started(double now) {
        previous = now;
    }
};
} // namespace mmvr
