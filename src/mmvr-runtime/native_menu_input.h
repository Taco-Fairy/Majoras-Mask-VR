#pragma once
#include <cstdint>
namespace mmvr {
// Physical menu controls are intentionally independent of gameplay rebinding.
struct NativeMenuInput {
    uint64_t frame = 0;
    float delta = 1.f / 90.f;
    float navigateX = 0, navigateY = 0, pointerX = 0, pointerY = 0;
    bool confirm = false, back = false, collapse = false;
    float scrollY = 0;
    bool click = false;
    bool captureTabSwitch = false;
};
} // namespace mmvr
