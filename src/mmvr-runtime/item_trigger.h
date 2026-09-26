#pragma once
#include <cmath>
#include <cstdint>
namespace mmvr {
// Hysteresis plus release-to-arm. Edges survive the render/native tick boundary.
class ItemTrigger {
    bool ready = false, held = false;
    uint64_t epoch = 0;
    double time = -1;

  public:
    void Reset() {
        ready = held = false;
        time = -1;
    }
    // Resume an already-held object without replaying a press. A released
    // controller only rearms; it never manufactures a saved release edge.
    void Rebase(double now, uint64_t generation, bool continueHeld, float value) {
        Reset();epoch=generation;time=now;
        if (!std::isfinite(now)||!std::isfinite(value)) return;
        held=continueHeld&&value>=.25f;ready=value<.25f;
    }
    int Update(double now, uint64_t generation, bool valid, float value) {
        if (!valid || !std::isfinite(now) || !std::isfinite(value)) {
            Reset();
            return 0;
        }
        if (generation != epoch || (time >= 0 && (now < time || now - time > .15))) {
            Reset();
            epoch = generation;
        }
        if (now == time)
            return 0;
        time = now;
        if (value < .25f) {
            bool release = held;
            held = false;
            ready = true;
            return release ? -1 : 0;
        }
        if (value > .65f && ready) {
            ready = false;
            held = true;
            return 1;
        }
        return 0;
    }
};
} // namespace mmvr
