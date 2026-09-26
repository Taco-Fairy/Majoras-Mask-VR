#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr {
// Distances are measured from the offhand string anchor in tracking space.
class BowDraw {
    bool ready = false;
    uint64_t epoch = 0;
    double lastShot = -100, lastTime = -1;

  public:
    double SampleTime() const { return lastTime; }
    bool drawing = false;
    float pull = 0;
    void Cancel() {
        ready = false;
        drawing = false;
        pull = 0;
        lastTime = -1;
    }
    void Rebase(double previousClock,double now,uint64_t generation,bool triggerHeld) {
        if(!std::isfinite(previousClock)||!std::isfinite(now)){Cancel();return;}
        lastShot=now+(lastShot-previousClock);lastTime=now;epoch=generation;
        ready=false;
        // Loading with an already released trigger cannot fire the saved arrow.
        drawing=drawing&&triggerHeld;
        if(!drawing)pull=0;
    }
    bool Update(double now, uint64_t generation, bool valid, float trigger, float distance, float backwards, float grab,
                float minimum, float full) {
        if (valid && generation == epoch && now == lastTime && std::isfinite(distance) && std::isfinite(backwards))
            return false;
        if (!valid || generation != epoch || !std::isfinite(distance) || !std::isfinite(backwards) ||
            (lastTime >= 0 && (now <= lastTime || now - lastTime > .15))) {
            Cancel();
            epoch = generation;
            return false;
        }
        lastTime = now;
        if (trigger < .25f) {
            bool fire = drawing && backwards >= minimum && now - lastShot >= .3;
            pull = std::clamp(backwards / std::max(full, minimum), 0.f, 1.f);
            drawing = false;
            ready = true;
            if (fire)
                lastShot = now;
            return fire;
        }
        if (trigger > .65f && ready) {
            drawing = distance <= grab;
            ready = false;
        }
        if (drawing) {
            pull = std::clamp(backwards / std::max(full, minimum), 0.f, 1.f);
            if (distance > 1.2f)
                Cancel();
        }
        return false;
    }
};
} // namespace mmvr
