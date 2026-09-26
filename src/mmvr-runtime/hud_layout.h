#pragma once
#include <algorithm>
namespace mmvr {
enum class HudGroup { TopLeft, Buttons, BottomLeft, Minimap, Clock, TopCenter, BottomCenter, None };
struct HudOffset {
    float x = 0, y = 0;
};
inline HudOffset CornerSpread(HudGroup group, float horizontal, float vertical = 100.f) {
    const float h = std::clamp(horizontal, 0.f, 300.f) / 100.f;
    const float v = std::clamp(vertical, 0.f, 300.f) / 100.f;
    switch (group) {
        case HudGroup::TopLeft:
            return { -70 * h, -10 * v };
        case HudGroup::Buttons:
            return { 70 * h, -10 * v };
        case HudGroup::BottomLeft:
            return { -70 * h, 10 * v };
        case HudGroup::Minimap:
            return { 12 * h, 10 * v };
        case HudGroup::Clock:
            return { 0, 10 * v }; // The native clock stays on the bottom-center axis.
        case HudGroup::TopCenter:
            return { 0, -10 * v };
        case HudGroup::BottomCenter:
            return { 0, 10 * v };
        default:
            return {};
    }
}
inline HudOffset GroupAnchor(HudGroup group) {
    switch (group) {
        case HudGroup::TopLeft:
            return { 30, 26 };
        case HudGroup::Buttons:
            return { 200, 25 };
        case HudGroup::BottomLeft:
            return { 26, 206 };
        case HudGroup::Minimap:
            return { 295, 220 };
        case HudGroup::Clock:
            return { 160, 206 };
        case HudGroup::TopCenter:
            return { 160, 26 };
        case HudGroup::BottomCenter:
            return { 160, 200 };
        default:
            return { 160, 120 };
    }
}
inline float HudElementScale(float size) {
    return size * 1.6f / 3.f;
}
inline HudOffset HudPosition(HudGroup group, float x, float y, float width, float size, float horizontal, float vertical = 100.f) {
    auto anchor = GroupAnchor(group), offset = CornerSpread(group, horizontal, vertical);
    float scale = HudElementScale(size);
    float groupX = 160 + (anchor.x - 160 + offset.x) * width / 3;
    float groupY = 120 + (anchor.y - 120 + offset.y) * 1.6f / 3;
    // Preserve the whole prompt cluster at the canvas edge. Independent viewport
    // clamping would separate A from B/mask at extreme width/spread combinations.
    if (group == HudGroup::Buttons) {
        groupX = std::clamp(groupX, 2.f + 57.f * scale, 318.f - 41.f * scale);
        groupY = std::clamp(groupY, 2.f + 16.f * scale, 238.f - 80.f * scale);
    } else if (group == HudGroup::TopLeft || group == HudGroup::BottomLeft) {
        groupX = std::max(groupX, 2.f + 12.f * scale);
    } else if (group == HudGroup::Minimap) {
        groupX = std::min(groupX, 318.f - 25.f * scale);
    } else if (group == HudGroup::Clock) {
        groupX = std::clamp(groupX, 2.f + 90.f * scale, 318.f - 90.f * scale);
    }
    // Clamp group anchors, never individual pieces: joints and labels retain
    // their offsets and dimensions at the extremes of either spread axis.
    if (group == HudGroup::TopLeft || group == HudGroup::TopCenter)
        groupY = std::max(groupY, 2.f + 12.f * scale);
    if (group == HudGroup::BottomLeft || group == HudGroup::Clock || group == HudGroup::BottomCenter)
        groupY = std::min(groupY, 238.f - 30.f * scale);
    if (group == HudGroup::Minimap)
        groupY = std::min(groupY, 238.f - 20.f * scale);
    return { groupX + (x - anchor.x) * scale, groupY + (y - anchor.y) * scale };
}

} // namespace mmvr
