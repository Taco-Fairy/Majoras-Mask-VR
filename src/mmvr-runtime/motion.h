#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mmvr {
struct MotionPoint {
    double time = 0;
    float x = 0, y = 0, z = 0;
};
class MotionHistory {
    std::array<MotionPoint, 12> points{};
    unsigned count = 0;
    uint64_t generation = 0;

  public:
    void Reset() {
        count = 0;
    }
    void Push(MotionPoint point, uint64_t epoch) {
        if (!std::isfinite(point.time) || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            !std::isfinite(point.z)) {
            Reset();
            return;
        }
        if (epoch != generation) {
            Reset();
            generation = epoch;
        }
        if (count && point.time == points[count - 1].time)
            return;
        if (count) {
            auto previous = points[count - 1];
            float x = point.x - previous.x, y = point.y - previous.y, z = point.z - previous.z;
            if (point.time <= previous.time || point.time - previous.time > .15 || x * x + y * y + z * z > 400)
                Reset();
        }
        if (count == points.size()) {
            for (unsigned i = 1; i < count; ++i)
                points[i - 1] = points[i];
            --count;
        }
        points[count++] = point;
    }
    std::array<float, 3> Velocity(double window = .085) const {
        if (count < 2)
            return {};
        auto last = points[count - 1];
        unsigned first = count - 2;
        while (first && last.time - points[first - 1].time <= window)
            --first;
        double duration = last.time - points[first].time;
        if (duration < .02 || duration > .15)
            return {};
        return { float((last.x - points[first].x) / duration), float((last.y - points[first].y) / duration),
                 float((last.z - points[first].z) / duration) };
    }
};
inline std::array<float, 3> BoundedVelocity(std::array<float, 3> value, float gain, float maximum) {
    float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    if (!std::isfinite(length) || !std::isfinite(gain) || maximum <= 0)
        return {};
    float factor = length > 0 ? std::min(std::max(0.f, gain), maximum / length) : 0;
    for (float& component : value)
        component *= factor;
    return value;
}
} // namespace mmvr
