#pragma once
#include <array>
#include <cstdint>
namespace mmvr {
// A theater image already contains its native fades in the correct order relative
// to text. Extend those colors outside the screen, never paint over that image.
inline bool FadeBehindTheater(bool stereo, bool nativeTheaterFades, bool fadeActive) noexcept {
    return !stereo && nativeTheaterFades && fadeActive;
}
// Native commands execute opaque, translucent, then overlay. Their recording
// order differs, so preserve separate streams for full-headset composition.
class ScreenFadeLayers {
  public:
    using Color = std::array<float, 4>;
    void Reset() noexcept {
        opaque = {};
        translucent = {};
        overlay = {};
    }
    void AddWorld(uint8_t r, uint8_t g, uint8_t b, uint8_t a, unsigned passes) noexcept {
        const Color color{ r / 255.f, g / 255.f, b / 255.f, a / 255.f };
        if (passes & 1)
            opaque = Over(color, opaque);
        if (passes & 2)
            translucent = Over(color, translucent);
    }
    void AddOverlay(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
        overlay = Over({ r / 255.f, g / 255.f, b / 255.f, a / 255.f }, overlay);
    }
    Color Composite() const noexcept {
        return Over(overlay, Over(translucent, opaque));
    }
    static Color Over(const Color& front, const Color& back) noexcept {
        const float alpha = front[3] + back[3] * (1 - front[3]);
        if (alpha <= 0)
            return {};
        Color result{};
        for (unsigned i = 0; i < 3; ++i)
            result[i] = (front[i] * front[3] + back[i] * back[3] * (1 - front[3])) / alpha;
        result[3] = alpha;
        return result;
    }

  private:
    Color opaque{}, translucent{}, overlay{};
};
} // namespace mmvr
