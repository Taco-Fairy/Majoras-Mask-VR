#pragma once
#include "settings.h"
#include <array>
#include <cmath>
namespace mmvr {
inline int StickSlot(float x, float y, float threshold = .55f, int count = 4) {
    if (std::max(std::abs(x), std::abs(y)) < threshold)
        return -1;
    if (std::min(std::abs(x), std::abs(y)) > std::max(std::abs(x), std::abs(y)) * .414213562f) {
        int corner = y > 0 ? (x < 0 ? 4 : 5) : (x < 0 ? 6 : 7);
        if (corner < count)
            return corner;
    }
    return std::abs(x) > std::abs(y) ? (x > 0 ? 1 : 3) : (y > 0 ? 0 : 2);
}
inline std::array<int, MaxItemSlots> AssignPreview(std::array<int, MaxItemSlots> original, int item, int target,
                                                   int count = 4) {
    if (item < 0 || target < 0 || target >= count)
        return original;
    if (original[target] == item)
        return original;
    int source = -1;
    for (int i = 0; i < count; ++i)
        if (original[i] == item) {
            source = i;
            original[i] = -1;
        }
    int displaced = original[target];
    original[target] = item;
    if (displaced >= 0) {
        int empty = source;
        if (empty < 0)
            for (int i = 0; i < count; ++i)
                if (original[i] < 0) {
                    empty = i;
                    break;
                }
        if (empty >= 0)
            original[empty] = displaced;
    }
    return original;
}
struct AssignmentState {
    bool open = false, armed = false;
    int item = -1, hover = -1, controller = -1;
    std::array<int, MaxItemSlots> original{}, preview{};
    void Cancel() {
        open = false;
        armed = false;
        item = hover = -1;
    }
    template <class Stick>
    bool UpdateHands(bool allowed, int selected, Stick left, Stick right, const std::array<int, MaxItemSlots>& slots,
                     const Settings& settings) {
        const auto axis = MenuAdjustInput(left, right);
        return Update(allowed, selected, axis.x, axis.y, slots, 1, ActiveItemSlots(settings));
    }
    bool Update(bool allowed, int selected, float x, float y, const std::array<int, MaxItemSlots>& slots, int hand = 1,
                int count = 4) {
        if (controller != hand) {
            Cancel();
            controller = hand;
        }
        if (!allowed || selected < 0) {
            Cancel();
            return false;
        }
        const bool neutral = std::max(std::abs(x), std::abs(y)) < .25f;
        if (neutral && !open) {
            armed = true;
            return false;
        }
        int direction = StickSlot(x, y, .55f, count);
        if (!open && armed && direction >= 0) {
            open = true;
            armed = false;
            item = selected;
            original = slots;
        }
        if (!open)
            return false;
        if (selected != item) {
            Cancel();
            return false;
        }
        if (direction >= 0) {
            hover = direction;
            preview = AssignPreview(original, item, hover, count);
        }
        if (neutral) {
            open = false;
            armed = true;
            return hover >= 0;
        }
        return false;
    }
};
} // namespace mmvr
