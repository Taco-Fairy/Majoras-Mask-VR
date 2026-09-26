#pragma once
#include "settings.h"
#include "controller_profiles.h"
#include <array>
#include <cmath>
namespace mmvr {
inline constexpr int ControlCount = 13;
inline bool StickControl(int action) {
    return action == 9 || action == 10;
}
inline Setting ControlSetting(int action) {
    return Setting(int(Setting::BindA) + action);
}
inline bool BindingSetting(int id) {
    return id >= int(Setting::BindA) && id <= int(Setting::BindRightTrigger);
}
inline int ControlSource(const Settings& settings, int action) {
    return int(settings.Get(ControlSetting(action)));
}
struct ControlVector {
    float x = 0, y = 0;
};
struct ControlSample {
    std::array<float, ControlCount> value{};
    ControlVector sticks[2]{};
    bool Neutral() const {
        for (int i = 0; i < ControlCount; ++i)
            if (!StickControl(i) && value[i] > .25f)
                return false;
        for (auto stick : sticks)
            if (std::abs(stick.x) > .3f || std::abs(stick.y) > .3f)
                return false;
        return true;
    }
};
inline ControlSample RemapControls(const Settings& settings, const ControlSample& physical) {
    ControlSample result;
    for (int i = 0; i < ControlCount; ++i) {
        int source = ControlSource(settings, i);
        if (StickControl(i))
            result.sticks[i - 9] = physical.sticks[source - 9];
        else
            result.value[i] = physical.value[source];
    }
    return result;
}
inline int BindingConflict(const Settings& settings, int action, int source) {
    for (int i = 0; i < ControlCount; ++i)
        if (i != action && StickControl(i) == StickControl(action) && ControlSource(settings, i) == source)
            return i;
    return -1;
}
template <class Change> inline void AssignControl(const Settings& settings, int action, int source, Change change) {
    const int previous = ControlSource(settings, action);
    if (source < 0 || source >= ControlCount || StickControl(action) != StickControl(source))
        return;
    // Swap occupied inputs rather than silently disabling the previous action.
    for (int i = 0; i < ControlCount; ++i)
        if (i != action && StickControl(i) == StickControl(action) && ControlSource(settings, i) == source)
            change(ControlSetting(i), float(previous));
    change(ControlSetting(action), float(source));
}
inline const char* ControlName(int source, const compat::Profile* left = nullptr,
                               const compat::Profile* right = nullptr) {
    static const char* standard[] = { "Right A",           "Right B",          "Left X",      "Left Y",
                                      "Left menu",         "Left stick click", "Left grip",   "Right grip",
                                      "Right stick click", "Left stick",       "Right stick", "Left trigger",
                                      "Right trigger" };
    if (source < 0 || source >= ControlCount)
        return "Unavailable";
    const bool isRight = source == 0 || source == 1 || source == 7 || source == 8 || source == 10 || source == 12;
    auto profile = isRight ? right : left;
    if (!profile)
        return standard[source];
    if (profile->layout == compat::Layout::Index) {
        if (source == 2)
            return "Left A";
        if (source == 3)
            return "Left B";
        if (source == 4)
            return "Left pad pressure";
    }
    if (profile->layout == compat::Layout::Wand || profile->layout == compat::Layout::Mixed) {
        if (source == 0)
            return "Right pad up/center";
        if (source == 1)
            return "Right pad down";
        if (source == 8)
            return "Right menu";
        if (source == 2)
            return profile->layout == compat::Layout::Wand ? "Left pad up" : "Left pad center/down";
        if (source == 3)
            return profile->layout == compat::Layout::Wand ? "Left menu" : "Left pad up";
        if (profile->layout == compat::Layout::Wand) {
            if (source == 4)
                return "Left pad down";
            if (source == 5)
                return "Left pad center";
            if (source == 9)
                return "Left trackpad";
            if (source == 10)
                return "Right trackpad";
        }
    }
    if (std::strstr(profile->path, "frame_controller")) {
        if (source == 2)
            return "Left D-pad down";
        if (source == 3)
            return "Left D-pad up";
        if (source == 4)
            return "Left view";
    }
    return standard[source];
}
// UI uses the original controls while rebinding, so remapping confirm/cancel or
// navigation cannot strand the player. No action is applied until confirmed.
struct BindingEditor {
    enum Phase { Closed, Release, Listen, ReviewRelease, Review };
    Phase phase = Closed;
    int action = -1, source = -1;
    float seconds = 0;
    void Begin(int selected) {
        action = selected;
        source = -1;
        seconds = 0;
        phase = Release;
    }
    void Cancel() {
        phase = Closed;
        action = source = -1;
        seconds = 0;
    }
    bool Active() const {
        return phase != Closed;
    }
    // Return 1 to commit, -1 on cancel/timeout, 0 while waiting.
    int Update(const ControlSample& input, float dt) {
        if (!Active())
            return 0;
        seconds += std::clamp(dt, 0.f, .1f);
        if (seconds > 20) {
            Cancel();
            return -1;
        }
        if (phase == Release || phase == ReviewRelease) {
            if (input.Neutral())
                phase = phase == Release ? Listen : Review;
            return 0;
        }
        if (phase == Listen) {
            if (StickControl(action) && input.value[1] > .75f) {
                Cancel();
                return -1;
            }
            int found = -1;
            for (int i = 0; i < ControlCount; ++i) {
                if (StickControl(i) != StickControl(action))
                    continue;
                const auto stick = StickControl(i) ? input.sticks[i - 9] : ControlVector{};
                bool pressed =
                    StickControl(i) ? std::max(std::abs(stick.x), std::abs(stick.y)) > .75f : input.value[i] > .75f;
                if (pressed) {
                    if (found >= 0)
                        return 0;
                    found = i;
                }
            }
            if (found >= 0) {
                source = found;
                phase = ReviewRelease;
                seconds = 0;
            }
            return 0;
        }
        if (phase == Review) {
            if (input.value[1] > .75f) {
                Cancel();
                return -1;
            }
            if (input.value[0] > .75f)
                return 1;
        }
        return 0;
    }
};
inline BindingEditor& GetBindingEditor() {
    static BindingEditor editor;
    return editor;
}
} // namespace mmvr
