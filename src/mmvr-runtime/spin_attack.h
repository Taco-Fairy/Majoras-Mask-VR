#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr {
// Gesture state is measured in tracking space: stick turns cannot fake a physical spin.
struct SpinAttack {
    double time = 0, pressedAt = 0, lastPhysical = -10, chargeUntil = 0;
    uint64_t epoch = ~uint64_t{};
    float previousYaw = 0, arc = 0, arcTime = 0, charge = 0, turnProgress = 0;
    int direction = 0, pendingTier = -1;
    bool ready = false, held = false, turning = false;
    float autoDuration = .85f;
    void Reset() {
        *this = SpinAttack{};
    }
    void Rebase(double previousClock,double now,uint64_t generation,float yaw,bool triggerHeld) {
        if(!std::isfinite(previousClock)||!std::isfinite(now)||!std::isfinite(yaw)){Reset();return;}
        pressedAt=now+(pressedAt-previousClock);lastPhysical=now+(lastPhysical-previousClock);
        chargeUntil=now+(chargeUntil-previousClock);time=now;epoch=generation;previousYaw=yaw;
        arc=arcTime=0;direction=0;ready=false;
        held=held&&triggerHeld;
        // Already issued attacks/automatic-turn progress remain native actions.
        // An unissued charge cannot be released merely by loading a state.
        if(!held&&!turning&&pendingTier<0)charge=0;
    }
    void Update(double now, uint64_t generation, bool valid, float trigger, float yaw, float reach, bool autoTurn,
                float fullCharge, bool physicalGreatSpin = false) {
        float dt = float(now - time);
        if (!valid || generation != epoch || dt < 0 || dt > .15f) {
            Reset();
            epoch = generation;
            time = now;
            previousYaw = yaw;
            return;
        }
        if (dt <= 0)
            return;
        time = now;
        if (trigger < .25f && !held)
            ready = true;
        if (trigger > .65f && ready && !held && !turning) {
            held = true;
            ready = false;
            pressedAt = now;
        }
        if (held)
            charge = std::clamp(float((now - pressedAt) / std::max(.5f, fullCharge)), 0.f, 1.f);
        if (held && trigger < .25f) {
            held = false;
            ready = true;
            if (now - pressedAt >= .2 && reach >= .25f) {
                pendingTier = charge >= .99f ? 2 : charge >= .33f ? 1 : 0;
                chargeUntil = now + 1.1;
                if (autoTurn) {
                    turning = true;
                    turnProgress = 0;
                }
            } else
                charge = 0;
        }
        float delta = std::remainder(yaw - previousYaw, 6.28318530718f);
        previousYaw = yaw;
        int sign = delta > 0 ? 1 : delta < 0 ? -1 : 0;
        if (!turning && reach >= .25f && std::abs(delta) / dt > .9f) {
            if (direction && sign != direction) {
                arc = 0;
                arcTime = 0;
            }
            direction = sign;
            arc += std::abs(delta);
            arcTime += dt;
            if (arc >= 5.23598775598f && now - lastPhysical > .8) {
                pendingTier = arc / std::max(.01f, arcTime) > 4.7f ? (physicalGreatSpin ? 2 : 1) : 0;
                chargeUntil = now + 1.1;
                lastPhysical = now;
                arc = arcTime = 0;
            }
        } else {
            arc = arcTime = 0;
            direction = 0;
        }
        if (!held && now > chargeUntil)
            charge = 0;
    }
    float Turn(float dt, bool allowed) {
        if (!allowed) {
            turning = false;
            turnProgress = 0;
            return 0;
        }
        if (!turning || dt <= 0)
            return 0;
        float old = turnProgress;
        turnProgress = std::min(1.f, turnProgress + std::min(dt, .05f) / autoDuration);
        auto ease = [](float t) { return t * t * (3 - 2 * t); };
        float delta = (ease(turnProgress) - ease(old)) * 6.28318530718f;
        if (turnProgress >= 1)
            turning = false;
        return delta;
    }
    int TakeTier() {
        int tier = pendingTier;
        pendingTier = -1;
        return tier;
    }
};
} // namespace mmvr
