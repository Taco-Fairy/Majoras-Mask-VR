#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace mmvr {
// Physical input only: two deliberate opposing strokes qualify. Common-mode
// controller travel, small tracking noise and a single reposition do not.
class ArmRunGesture {
  using Vec = std::array<float, 3>;
  using Hands = std::array<Vec, 2>;
  Hands previous{}, velocity{}, start{};
  Vec axis{};
  double time = -1, strokeTime = 0, lastMotion = -100, lastStroke = -100;
  uint64_t epoch = 0, origin = 0;
  unsigned strokes = 0;
  bool credited = false, haveAxis = false;
  static float Dot(Vec a, Vec b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }
  static Vec Sub(Vec a, Vec b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  }
  static float Length(Vec v) { return std::sqrt(Dot(v, v)); }
  void Begin(Hands position, Vec direction, double now) {
    start = position;
    axis = direction;
    strokeTime = now;
    credited = false;
    haveAxis = true;
  }

public:
  void Reset() { *this = ArmRunGesture{}; }
  void Update(double now, uint64_t frameEpoch, uint64_t originEpoch,
              bool eligible, const Hands &position) {
    bool finite = std::isfinite(now);
    for (auto hand : position)
      for (float value : hand)
        finite &= std::isfinite(value);
    if (!eligible || !finite) {
      Reset();
      return;
    }
    if (time < 0 || frameEpoch != epoch || originEpoch != origin ||
        now < time || now - time > .15) {
      Reset();
      previous = position;
      time = now;
      epoch = frameEpoch;
      origin = originEpoch;
      return;
    }
    const double dt = now - time;
    if (dt <= 0)
      return; // Replaying an eye cannot count the same sample twice.
    const auto before = previous;
    previous = position;
    time = now;
    const float smoothing = float(dt / (.035 + dt));
    for (int hand = 0; hand < 2; ++hand) {
      auto delta = Sub(position[hand], before[hand]);
      if (Length(delta) > float(dt) * 6.f) {
        Reset();
        return;
      }
      for (int k = 0; k < 3; ++k)
        velocity[hand][k] +=
            (delta[k] / float(dt) - velocity[hand][k]) * smoothing;
    }
    const float speed0 = Length(velocity[0]), speed1 = Length(velocity[1]);
    const bool opposed =
        speed0 > .08f && speed1 > .08f &&
        Dot(velocity[0], velocity[1]) < -.15f * speed0 * speed1;
    if (!opposed) {
      if (now - lastMotion > .4) {
        strokes = 0;
        haveAxis = false;
        credited = false;
      }
      return;
    }
    lastMotion = now;
    auto direction = Sub(velocity[0], velocity[1]);
    const float length = Length(direction);
    for (auto &v : direction)
      v /= length;
    if (!haveAxis || now - strokeTime > 1.0) {
      strokes = 0;
      Begin(before, direction, now - dt);
    } else if (Dot(direction, axis) < -.45f) {
      if (!credited)
        strokes = 0;
      Begin(before, direction, now - dt);
    }
    // Require actual displacement in both hands, not accumulated jitter.
    if (!credited && Dot(direction, axis) > .3f && now - strokeTime >= .075 &&
        Length(Sub(position[0], start[0])) >= .025f &&
        Length(Sub(position[1], start[1])) >= .025f) {
      credited = true;
      strokes = std::min(2u, strokes + 1);
      lastStroke = now;
    }
  }
  float Multiplier(double now) const {
    return strokes >= 2 && now >= time && now - lastMotion <= .35 &&
                   now - lastStroke <= 1.0
               ? 1.15f
               : 1.f;
  }
};
} // namespace mmvr
