#pragma once
#include "first_person.h"
#include <algorithm>
namespace mmvr {
// Small adaptive pose filter in tracking space. Head pose, buttons and measured velocity remain raw.
class ItemPoseSmoother {
    struct Hand {
        XrPosef raw{}, filtered{};
        bool valid = false;
    };
    Hand hands[2];
    double time = 0;
    uint64_t epoch = 0, origin = 0, key = 0;
    bool valid = false;
    float amount = -1;
    static float Dot(XrQuaternionf a, XrQuaternionf b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    static XrQuaternionf Normalize(XrQuaternionf q) {
        float n = std::sqrt(Dot(q, q));
        return n > .0001f ? XrQuaternionf{ q.x / n, q.y / n, q.z / n, q.w / n } : XrQuaternionf{ 0, 0, 0, 1 };
    }
    static XrQuaternionf Mix(XrQuaternionf a, XrQuaternionf b, float t) {
        if (Dot(a, b) < 0)
            b = { -b.x, -b.y, -b.z, -b.w };
        return Normalize(
            { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t });
    }
    static XrQuaternionf MultiplyQ(XrQuaternionf a, XrQuaternionf b) {
        return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                 a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
    }
    static XrVector3f Rotate(XrQuaternionf q, XrVector3f v) {
        auto p = MultiplyQ(MultiplyQ(q, { v.x, v.y, v.z, 0 }), { -q.x, -q.y, -q.z, q.w });
        return { p.x, p.y, p.z };
    }
    static float Distance(XrVector3f a, XrVector3f b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }
    static bool Finite(const XrPosef& p) {
        auto q = p.orientation;
        return std::isfinite(p.position.x) && std::isfinite(p.position.y) && std::isfinite(p.position.z) &&
               std::isfinite(Dot(q, q)) && Dot(q, q) > .5f && Dot(q, q) < 1.5f;
    }

  public:
    void Reset() {
        valid = false;
        for (auto& hand : hands)
            hand.valid = false;
    }
    TrackingFrame Update(const TrackingFrame& frame, float milliseconds, bool enabled, uint64_t context) {
        auto out = frame;
        milliseconds = std::clamp(milliseconds, 0.f, 40.f);
        if (!enabled || milliseconds <= 0 || !std::isfinite(frame.timeSeconds)) {
            Reset();
            return out;
        }
        double dt = frame.timeSeconds - time;
        bool reset = !valid || epoch != frame.epoch || origin != frame.originEpoch || key != context ||
                     amount != milliseconds || dt < 0 || dt > .1;
        for (int i = 0; i < 2; ++i) {
            auto& hand = hands[i];
            const auto& raw = frame.hands[i];
            if (!frame.handValid[i] || !frame.handTracked[i] || !Finite(raw)) {
                hand.valid = false;
                continue;
            }
            if (reset || !hand.valid || Distance(raw.position, hand.raw.position) > .25f) {
                hand.raw = hand.filtered = raw;
                hand.filtered.orientation = Normalize(raw.orientation);
                hand.valid = true;
            } else if (dt > 0) {
                float speed = Distance(raw.position, hand.raw.position) / float(dt);
                float angle =
                    2 * std::acos(std::clamp(std::abs(Dot(Normalize(raw.orientation), Normalize(hand.raw.orientation))),
                                             0.f, 1.f));
                float tau = milliseconds * .001f /
                            (1 + 8 * std::max(0.f, speed - .3f) + .5f * std::max(0.f, angle / float(dt) - 2.f));
                float alpha = 1 - std::exp(-float(dt) / tau);
                float gap = Distance(raw.position, hand.filtered.position);
                if (gap > .015f)
                    alpha = std::max(alpha, 1 - .015f / gap);
                hand.filtered.position = {
                    hand.filtered.position.x + (raw.position.x - hand.filtered.position.x) * alpha,
                    hand.filtered.position.y + (raw.position.y - hand.filtered.position.y) * alpha,
                    hand.filtered.position.z + (raw.position.z - hand.filtered.position.z) * alpha
                };
                float angularGap =
                    2 * std::acos(
                            std::clamp(std::abs(Dot(Normalize(raw.orientation), hand.filtered.orientation)), 0.f, 1.f));
                float rotationAlpha = angularGap > .07f ? std::max(alpha, 1 - .07f / angularGap) : alpha;
                hand.filtered.orientation = Mix(hand.filtered.orientation, Normalize(raw.orientation), rotationAlpha);
                hand.raw = raw;
            }
            out.hands[i] = hand.filtered;
            if (frame.aimValid[i] && Finite(frame.aims[i])) {
                // Apply the same rigid correction to aim and grip so muzzle/reticle/hand cannot separate.
                auto q = Normalize(raw.orientation);
                auto correction = MultiplyQ(hand.filtered.orientation, { -q.x, -q.y, -q.z, q.w });
                auto delta = Rotate(correction, { frame.aims[i].position.x - raw.position.x,
                                                  frame.aims[i].position.y - raw.position.y,
                                                  frame.aims[i].position.z - raw.position.z });
                out.aims[i].position = { hand.filtered.position.x + delta.x, hand.filtered.position.y + delta.y,
                                         hand.filtered.position.z + delta.z };
                out.aims[i].orientation = Normalize(MultiplyQ(correction, frame.aims[i].orientation));
            }
        }
        valid = true;
        time = frame.timeSeconds;
        epoch = frame.epoch;
        origin = frame.originEpoch;
        key = context;
        amount = milliseconds;
        return out;
    }
};
} // namespace mmvr
