#pragma once
#include "motion.h"
#include "projection.h"
#include <cmath>
namespace mmvr {
// Small first-person reach aid for short swords; keep visible mesh, stick
// flame and native magic disk unchanged. Input/output are native world units.
inline float SwordCollisionLength(float bladeLength, float trackingScale, float hitboxPercent,
                                  bool magicExtended) {
    if (!std::isfinite(bladeLength) || bladeLength <= 0 || !std::isfinite(trackingScale) || trackingScale <= 0)
        return 0;
    const float assistance = magicExtended ? 0.f : std::min(bladeLength*.2f, 4.f*trackingScale);
    const float multiplier = std::isfinite(hitboxPercent) ? std::clamp(hitboxPercent,100.f,200.f)/100.f : 1.f;
    return (bladeLength+assistance)*multiplier;
}
class DoubleTap {
    double last = -100;

  public:
    void Reset() {
        last = -100;
    }
    bool Press(bool pressed, double now) {
        if (!pressed)
            return false;
        bool twice = now >= last && now - last <= .35;
        last = twice ? -100 : now;
        return twice;
    }
};
struct SwingTuning {
    float speed = .9f, distance = .12f, resetSpeed = .25f;
    double cooldown = .25;
    bool singleStroke = false;
};
// Input is tracking-space metres. Camera motion cannot create a swing.
class SwingGate {
    MotionHistory history;
    MotionPoint last{}, strokeStart{};
    uint64_t epoch = 0;
    bool have = false;
    double lastSwing = -100, movingSince = -1;
    unsigned movingSamples = 0;
    bool spent = false;
    MotionPoint strokeDirection{};

  public:
    void Reset() {
        history.Reset();
        have = false;
        movingSince = -1;
        movingSamples = 0;
        speed = 0;
        spent = false;
    }
    // A loaded world keeps its cooldown, but old controller samples must not
    // bridge across a restart and create a synthetic swing.
    void Rebase(double previousClock, double now, uint64_t generation) {
        Reset();epoch=generation;
        lastSwing=std::isfinite(previousClock)&&std::isfinite(now)?now+(lastSwing-previousClock):-100;
    }
    float speed = 0;
    unsigned serial = 0;
    bool Update(MotionPoint point, uint64_t generation, bool valid, const SwingTuning& tuning) {
        if (!valid || !std::isfinite(point.time) || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) {
            Reset();
            return false;
        }
        if (have && epoch == generation && point.time == last.time)
            return false;
        // A new tracking clock cannot inherit a cooldown from a later timestamp.
        if (point.time < lastSwing)
            lastSwing = -100;
        float delta = have ? std::sqrt(SQ_DISTANCE(point, last)) : 0;
        if (epoch != generation || (have && (point.time <= last.time || point.time - last.time > .15 || delta > .5f))) {
            Reset();
            delta = 0;
        }
        epoch = generation;
        if (!have)
            strokeStart = point;
        const float stepSpeed = have && point.time > last.time ? delta / float(point.time - last.time) : 0.f;
        history.Push(point, generation);
        auto v = history.Velocity();
        speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        // A stroke needs real displacement and several moving samples, never an
        // animation or a compulsory rest. Continuous/reversing swings rearm on cooldown.
        if (speed < std::min(tuning.resetSpeed, tuning.speed * .5f)) {
            strokeStart = point;
            movingSince = -1;
            movingSamples = 0;
            spent = false;
        } else if (stepSpeed >= tuning.speed * .5f) {
            if (movingSince < 0)
                movingSince = point.time;
            ++movingSamples;
        }
        float travel = std::sqrt(SQ_DISTANCE(point, strokeStart));
        last = point;
        have = true;
        if (spent && tuning.singleStroke) {
            float dot = (point.x - strokeStart.x) * strokeDirection.x + (point.y - strokeStart.y) * strokeDirection.y +
                        (point.z - strokeStart.z) * strokeDirection.z;
            if (dot < 0 && travel >= .08f)
                spent = false;
        }
        if ((!tuning.singleStroke || !spent) && movingSamples >= 3 && movingSince >= 0 &&
            point.time - movingSince >= .025 && stepSpeed >= tuning.speed * .5f && speed >= tuning.speed &&
            travel >= tuning.distance && point.time - lastSwing >= tuning.cooldown) {
            strokeDirection = { 0, point.x - strokeStart.x, point.y - strokeStart.y, point.z - strokeStart.z };
            spent = true;
            strokeStart = point;
            movingSince = -1;
            movingSamples = 0;
            lastSwing = point.time;
            ++serial;
            return true;
        }
        return false;
    }

  private:
    static float SQ_DISTANCE(const MotionPoint& a, const MotionPoint& b) {
        return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    }
};
class ContactPose {
    Matrix safe{};
    bool have = false;

  public:
    void Reset() {
        have = false;
    }
    Matrix Resolve(const Matrix& desired, bool blocked, float maximumOffset) {
        if (!blocked) {
            safe = desired;
            have = true;
            return desired;
        }
        if (!have)
            return {};
        float x = desired.m[3][0] - safe.m[3][0], y = desired.m[3][1] - safe.m[3][1],
              z = desired.m[3][2] - safe.m[3][2];
        if (!std::isfinite(maximumOffset) || x * x + y * y + z * z > maximumOffset * maximumOffset)
            return {};
        return safe;
    }
};
class ContactWindow {
    double from = 0, until = -1;
    bool consumed = true;

  public:
    void Cancel() {
        consumed = true;
        until = -1;
    }
    void Arm(double now, double duration) {
        from = now;
        until = now + std::clamp(duration, .05, .6);
        consumed = false;
    }
    bool Active(double now) const {
        return !consumed && std::isfinite(now) && now <= until && now >= from;
    }
    void Contact() {
        consumed = true;
    }
    void Rebase(double previousClock,double now) {
        if(!std::isfinite(now)||!Active(previousClock)){Cancel();return;}
        from=now+(from-previousClock);until=now+(until-previousClock);
    }
};
template <class T, class Count> void RemoveQueued(T** queue, Count& count, T* first, T* second = nullptr) {
    Count write = 0;
    auto old = count;
    for (Count i = 0; i < old; ++i)
        if (queue[i] != first && queue[i] != second)
            queue[write++] = queue[i];
    count = write;
    while (write < old)
        queue[write++] = nullptr;
}
struct CombatDiagnostics {
    bool active = false, blocked = false;
    float speed = 0, reach = 0;
    unsigned swings = 0;
};
CombatDiagnostics& GetCombatDiagnostics() noexcept;
} // namespace mmvr
