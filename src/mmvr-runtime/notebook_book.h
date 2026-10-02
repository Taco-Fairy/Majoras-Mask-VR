#pragma once
#include "first_person.h"
#include <algorithm>
#include <cmath>
namespace mmvr {
// Folded page surfaces shared by composition, solid binding and touch hit testing.
// Raw OpenXR metres keep its size independent of the selected world scale.
// Native notebook uses a cropped 576x454 canvas inside the authored 640x480 page.
// Keep the same crop, aspect and coordinate offset for drawing and touch.
inline constexpr float NotebookWidth = .64f, NotebookHeight = NotebookWidth * 454.f / 576.f;
inline Matrix NotebookPose(const XrPosef& hand) {
    constexpr float s = .70710678118f;
    const auto offset = PoseMatrix({{-s, 0, 0, s}, {0, .055f, -.12f}});
    return Multiply(offset, PoseMatrix(hand));
}
inline XrPosef NotebookPagePose(const XrPosef& hand) {
    constexpr float s = .70710678118f;
    const auto& q = hand.orientation;
    auto m = NotebookPose(hand);
    return {{s*(q.x-q.w), s*(q.y-q.z), s*(q.z+q.y), s*(q.w+q.x)},
            {m.m[3][0], m.m[3][1], m.m[3][2]}};
}
// A 155-degree open book: each leaf rises 12.5 degrees toward the reader.
inline constexpr float NotebookFold = 12.5f * 3.14159265358979323846f / 180.f;
inline XrPosef NotebookLeafPose(const XrPosef& spine, int leaf) {
    const float side = leaf ? 1.f : -1.f, angle = -side * NotebookFold;
    const float a = std::sin(angle/2), b = std::cos(angle/2);
    const auto& q = spine.orientation;
    auto center = Multiply(YawPose(angle, side * NotebookWidth * .25f * std::cos(NotebookFold),
                                  0, NotebookWidth * .25f * std::sin(NotebookFold)), PoseMatrix(spine));
    return {{q.x*b-q.z*a, q.w*a+q.y*b, q.x*a+q.z*b, q.w*b-q.y*a},
            {center.m[3][0],center.m[3][1],center.m[3][2]}};
}
inline Matrix NotebookLeafLocal(int leaf) {
    return PoseMatrix(NotebookLeafPose({{0,0,0,1},{0,0,0}},leaf));
}
struct NotebookTouch { bool active = false; float x = 0, y = 0; };
class NotebookContact {
    bool armed = false, touching = false;
    uint64_t origin = ~uint64_t{};
public:
    void Reset() { armed = touching = false; origin = ~uint64_t{}; }
    NotebookTouch Update(const TrackingFrame& frame, int holdingHand, bool enabled) {
        NotebookTouch out;
        if (!enabled || holdingHand < 0 || holdingHand > 1 ||
            !frame.handTracked[holdingHand] || !frame.handTracked[1-holdingHand]) {
            Reset(); return out;
        }
        if (origin != frame.originEpoch) { Reset(); origin = frame.originEpoch; }
        // Index-tip estimate in the free controller's local space.
        auto tip = Multiply(YawPose(0, 0, .015f, -.09f), PoseMatrix(frame.hands[1-holdingHand]));
        // Test each physical leaf, then unfold its coordinates into the unchanged native canvas.
        bool within = false, contact = false;
        float x = 0, y = 0, nearest = 1000.f;
        const auto spine = NotebookPagePose(frame.hands[holdingHand]);
        for (int leaf=0;leaf<2;++leaf) {
            const auto local = Multiply(tip, InversePose(PoseMatrix(NotebookLeafPose(spine,leaf))));
            const float lx=local.m[3][0], ly=local.m[3][1], z=local.m[3][2];
            const bool inside=std::isfinite(lx)&&std::isfinite(ly)&&std::isfinite(z)&&
                std::abs(lx)<=NotebookWidth*.25f+.0001f && std::abs(ly)<=NotebookHeight*.5f;
            if (!inside) continue;
            within = true;
            nearest = std::min(nearest,std::abs(z));
            if (z>=-.015f && z<=.025f && !contact) {
                contact=true; x=lx+(leaf ? 1.f : -1.f)*NotebookWidth*.25f; y=ly;
            }
        }
        if (!contact && (!within || nearest > .045f)) { armed = true; touching = false; }
        if (armed && contact && !touching) {
            out = {true, 30.f+(x/NotebookWidth+.5f)*576.f, 10.f+(.5f-y/NotebookHeight)*454.f};
            touching = true; armed = false;
        }
        return out;
    }
};
} // namespace mmvr
