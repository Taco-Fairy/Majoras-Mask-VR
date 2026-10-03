#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mmvr {
struct SwordChargeVisual {
    float nativeCharge = 0, pulse = 1;
    uint8_t alpha = 0;
    bool great = false;
};

inline SwordChargeVisual SwordChargeEffect(float charge, bool greatUnlocked, unsigned nativeFrame) {
    SwordChargeVisual result;
    if (!std::isfinite(charge) || charge <= .1f) return result;
    // The physical gesture's existing full-charge threshold is .99. Do not
    // advertise a great spin before that release would actually receive one.
    result.nativeCharge = greatUnlocked && charge >= .99f ? 1.f : std::min(charge, .5f);
    result.great = result.nativeCharge >= .85f;
    // Native EnMThunder_Charge fade and EnMThunder_Draw pulse, unchanged assets.
    result.alpha = result.nativeCharge > .15f ? 255 : static_cast<uint8_t>(
        std::clamp((result.nativeCharge - .1f) * 255.f * 20.f, 0.f, 255.f));
    constexpr float scales[] = {.1f, .15f, .2f, .25f, .3f, .25f, .2f, .15f};
    result.pulse = 1.f + scales[nativeFrame & 7] * (result.great ? 6.f : 2.f);
    return result;
}
} // namespace mmvr
