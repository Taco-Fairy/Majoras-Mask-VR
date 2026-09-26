#pragma once
#include "first_person.h"
namespace mmvr {
struct ShoulderHolster {
    bool armed = false, held = false, pulling = false;
    uint64_t epoch = 0;
    double time = -1;
    XrVector3f previous{};
    void Reset() {
        armed = held = pulling = false;
        time = -1;
    }
    int Update(const TrackingFrame& frame, int hand, bool allowed, bool sword, float reach) {
        auto p = frame.hands[hand].position;
        if (!allowed || !frame.handTracked[hand] || epoch != frame.epoch) {
            Reset();
            epoch = frame.epoch;
            return 0;
        }
        if (time == frame.timeSeconds)
            return 0;
        float dx = p.x - previous.x, dy = p.y - previous.y, dz = p.z - previous.z;
        if (!std::isfinite(frame.timeSeconds) || !std::isfinite(dx + dy + dz) ||
            (time >= 0 && (frame.timeSeconds < time || frame.timeSeconds - time > .15 ||
                           dx * dx + dy * dy + dz * dz > .25f * .25f))) {
            Reset();
            return 0;
        }
        time = frame.timeSeconds;
        previous = p;
        auto head = YawPose(PoseYaw(PoseMatrix(frame.head)), frame.head.position.x, frame.head.position.y,
                            frame.head.position.z);
        auto local = Multiply(PoseMatrix(frame.hands[hand]), InversePose(head));
        float x = local.m[3][0] * (hand ? 1 : -1), y = local.m[3][1], z = local.m[3][2];
        bool zone = x > -.12f && x < reach && y > -.65f && y < .18f && z > .04f && z < reach;
        float trigger = frame.triggers[hand];
        if (trigger < .25f) {
            armed = true;
            held = pulling = false;
            return 0;
        }
        if (trigger > .7f && armed && !held) {
            held = true;
            armed = false;
            if (zone) {
                if (sword)
                    return -1;
                pulling = true;
            }
        }
        // Crossing the front of the shoulder draws; sideways drift is not a draw.
        if (pulling && trigger > .25f && z < -.05f) {
            pulling = false;
            return 1;
        }
        return 0;
    }
};
} // namespace mmvr
