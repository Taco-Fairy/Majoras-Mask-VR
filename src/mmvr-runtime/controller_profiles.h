#pragma once
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr::compat {
enum class Layout { Standard, Index, Wand, Mixed, Generic };
struct Binding {
    int action;
    const char* path;
};
struct Profile {
    const char* path;
    const char* extension;
    Layout layout;
    bool forceGrip;
    std::vector<Binding> bindings;
};
inline const std::vector<Profile>& Profiles() {
    static const std::vector<Profile> profiles = {
        { "/interaction_profiles/oculus/touch_controller",
          "",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/valve/index_controller",
          "",
          Layout::Index,
          true,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/force" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/force" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 21, "/user/hand/left/input/trackpad" },        { 23, "/user/hand/left/input/trackpad/touch" },
              { 27, "/user/hand/left/input/trackpad/force" },  { 22, "/user/hand/right/input/trackpad" },
              { 24, "/user/hand/right/input/trackpad/touch" }, { 28, "/user/hand/right/input/trackpad/force" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/a/click" },          { 3, "/user/hand/left/input/b/click" },
          } },
        { "/interaction_profiles/htc/vive_controller",
          "",
          Layout::Wand,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },
              { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },
              { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },
              { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },
              { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },
              { 20, "/user/hand/right/input/squeeze/click" },
              { 21, "/user/hand/left/input/trackpad" },
              { 23, "/user/hand/left/input/trackpad/touch" },
              { 25, "/user/hand/left/input/trackpad/click" },
              { 22, "/user/hand/right/input/trackpad" },
              { 24, "/user/hand/right/input/trackpad/touch" },
              { 26, "/user/hand/right/input/trackpad/click" },
              { 3, "/user/hand/left/input/menu/click" },
              { 8, "/user/hand/right/input/menu/click" },
          } },
        { "/interaction_profiles/microsoft/motion_controller",
          "",
          Layout::Mixed,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },   { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 20, "/user/hand/right/input/squeeze/click" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 21, "/user/hand/left/input/trackpad" },        { 23, "/user/hand/left/input/trackpad/touch" },
              { 25, "/user/hand/left/input/trackpad/click" },  { 22, "/user/hand/right/input/trackpad" },
              { 24, "/user/hand/right/input/trackpad/touch" }, { 26, "/user/hand/right/input/trackpad/click" },
              { 4, "/user/hand/left/input/menu/click" },       { 8, "/user/hand/right/input/menu/click" },
          } },
        { "/interaction_profiles/samsung/odyssey_controller",
          "XR_EXT_samsung_odyssey_controller",
          Layout::Mixed,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },   { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 20, "/user/hand/right/input/squeeze/click" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 21, "/user/hand/left/input/trackpad" },        { 23, "/user/hand/left/input/trackpad/touch" },
              { 25, "/user/hand/left/input/trackpad/click" },  { 22, "/user/hand/right/input/trackpad" },
              { 24, "/user/hand/right/input/trackpad/touch" }, { 26, "/user/hand/right/input/trackpad/click" },
              { 4, "/user/hand/left/input/menu/click" },       { 8, "/user/hand/right/input/menu/click" },
          } },
        { "/interaction_profiles/hp/mixed_reality_controller",
          "XR_EXT_hp_mixed_reality_controller",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/htc/vive_cosmos_controller",
          "XR_HTC_vive_cosmos_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },   { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 20, "/user/hand/right/input/squeeze/click" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/htc/vive_focus3_controller",
          "XR_HTC_vive_focus3_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/facebook/touch_controller_pro",
          "XR_FB_touch_controller_pro",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/meta/touch_controller_plus",
          "XR_META_touch_controller_plus",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/bytedance/pico_neo3_controller",
          "XR_BD_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/bytedance/pico4_controller",
          "XR_BD_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/bytedance/pico_ultra_controller_bd",
          "XR_BD_ultra_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/yvr/touch_controller_yvr",
          "XR_YVR_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },   { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 20, "/user/hand/right/input/squeeze/click" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/x/click" },          { 3, "/user/hand/left/input/y/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/varjo/xr-4_controller",
          "XR_VARJO_xr4_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 19, "/user/hand/left/input/squeeze/click" },   { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 20, "/user/hand/right/input/squeeze/click" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/a/click" },          { 3, "/user/hand/left/input/b/click" },
              { 4, "/user/hand/left/input/menu/click" },
          } },
        { "/interaction_profiles/khr/generic_controller",
          "XR_KHR_generic_controller",
          Layout::Generic,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },
              { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },
              { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },
              { 9, "/user/hand/left/input/thumbstick" },
              { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },
              { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },
              { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },
              { 0, "/user/hand/right/input/primary/click" },
              { 1, "/user/hand/right/input/secondary/click" },
              { 2, "/user/hand/left/input/primary/click" },
              { 3, "/user/hand/left/input/secondary/click" },
              { 4, "/user/hand/left/input/thumbstick/click" },
              { 8, "/user/hand/right/input/thumbstick/click" },
          } },
        { "/interaction_profiles/valve/frame_controller_valve",
          "XR_VALVE_frame_controller_interaction",
          Layout::Standard,
          false,
          {
              { 13, "/user/hand/left/input/grip/pose" },       { 15, "/user/hand/left/input/aim/pose" },
              { 17, "/user/hand/left/output/haptic" },         { 11, "/user/hand/left/input/trigger/value" },
              { 6, "/user/hand/left/input/squeeze/value" },    { 9, "/user/hand/left/input/thumbstick" },
              { 5, "/user/hand/left/input/thumbstick/click" }, { 14, "/user/hand/right/input/grip/pose" },
              { 16, "/user/hand/right/input/aim/pose" },       { 18, "/user/hand/right/output/haptic" },
              { 12, "/user/hand/right/input/trigger/value" },  { 7, "/user/hand/right/input/squeeze/value" },
              { 10, "/user/hand/right/input/thumbstick" },     { 8, "/user/hand/right/input/thumbstick/click" },
              { 0, "/user/hand/right/input/a/click" },         { 1, "/user/hand/right/input/b/click" },
              { 2, "/user/hand/left/input/dpad_down/click" },  { 3, "/user/hand/left/input/dpad_up/click" },
              { 4, "/user/hand/left/input/view/click" },
          } },
    };
    return profiles;
}
inline const Profile* Find(const char* path) {
    for (const auto& p : Profiles())
        if (std::strcmp(path, p.path) == 0)
            return &p;
    return nullptr;
}
// A pad click owns its original sector until release, even if the thumb slides.
struct PadClick {
    int action = -1;
    int Update(Layout layout, int hand, bool pressed, float x, float y) {
        if (!pressed) {
            action = -1;
            return -1;
        }
        if (action < 0) {
            if (hand)
                action = y < -.3f ? 1 : 0;
            else if (layout == Layout::Wand)
                action = y > .35f ? 2 : y < -.35f ? 4 : 5;
            else
                action = y > .3f ? 3 : 2;
        }
        return action;
    }
};
struct Threshold {
    bool held = false;
    bool Update(float v) {
        if (v > .65f)
            held = true;
        else if (v < .25f)
            held = false;
        return held;
    }
};
inline float Grip(float analog, bool digital, bool force) {
    return std::max(digital ? 1.f : 0.f, std::clamp(analog * (force ? 4.f : 1.f), 0.f, 1.f));
}
// App cadence can differ from panel Hz under reprojection. Do not label it display refresh.
struct Cadence {
    unsigned hz = 0, candidate = 0, count = 0;
    void Reset() {
        hz = candidate = count = 0;
    }
    bool Update(int64_t period) {
        if (period <= 0)
            return false;
        double rate = 1e9 / double(period);
        if (rate < 20 || rate > 500)
            return false;
        unsigned value = unsigned(std::lround(rate));
        if (value == hz) {
            candidate = count = 0;
            return false;
        }
        if (value != candidate) {
            candidate = value;
            count = 1;
        } else
            ++count;
        if (hz && count < 3)
            return false;
        hz = value;
        candidate = count = 0;
        return true;
    }
};
} // namespace mmvr::compat
