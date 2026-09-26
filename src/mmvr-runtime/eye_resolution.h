#pragma once
#include <algorithm>
#include <cmath>
#include <string_view>
namespace mmvr {
struct EyeResolution {
    unsigned width, height;
};
// Title/file-select/theater framing is independent of the desktop window.
inline constexpr EyeResolution TheaterResolution() { return {1920u, 1080u}; }
// OpenXR recommends render sizes but does not expose physical panel dimensions.
// Exact known headset only: never apply Quest 3 dimensions to Quest 3S/Pro or others.
inline EyeResolution ResolveEyeResolution(std::string_view headset, unsigned width, unsigned height, unsigned maxWidth,
                                          unsigned maxHeight, float scale, bool nativePanel) {
    if (nativePanel && headset == "Meta Quest 3") {
        width = std::max(width, 2064u);
        height = std::max(height, 2208u);
    }
    if (!std::isfinite(scale))
        scale = 1;
    scale = std::clamp(scale, .5f, 1.5f);
    return { std::min(maxWidth, std::max(1u, unsigned(width * scale))),
             std::min(maxHeight, std::max(1u, unsigned(height * scale))) };
}
} // namespace mmvr
