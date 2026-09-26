#pragma once
#include <cstdint>
namespace mmvr {
struct HudCadence {
    uint64_t previous = ~uint64_t{};
    unsigned width = 0, height = 0;
    bool valid = false;
    bool Refresh(uint64_t tick, unsigned w, unsigned h, bool allow) {
        bool render = !allow || !valid || tick != previous || w != width || h != height;
        valid = allow;
        previous = tick;
        width = w;
        height = h;
        return render;
    }
};
} // namespace mmvr
