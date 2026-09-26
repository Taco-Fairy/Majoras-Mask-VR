#pragma once
#include <array>
#include <algorithm>
namespace mmvr {
// 1 Goron, 2 Zora, 3 Deku, 4 Deity, 5 Giant. Removal uses a world-space swirl.
inline std::array<float, 4> MaskEffectTint(int style, float amount) {
    constexpr float colors[6][3] = { { 0, 0, 0 },           { .22f, .008f, .008f }, { .006f, .025f, .24f },
                                     { .008f, .16f, .02f }, { 1.f, .9f, .12f },     { 1, 1, 1 } };
    if (style < 1 || style > 5)
        return {};
    return { colors[style][0], colors[style][1], colors[style][2], std::clamp(amount, 0.f, 1.f) };
}
} // namespace mmvr
