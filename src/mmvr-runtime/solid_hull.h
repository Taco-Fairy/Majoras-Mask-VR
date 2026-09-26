#pragma once
#include "hand_collision.h"
#include <vector>
namespace mmvr {
// Small convex prop surfaces built from the player's loaded mesh vertices.
// Construction runs once per scene; contact queries reuse the resulting faces.
class SolidHull {
  struct Face {
    HandPoint a, b, c, normal;
  };
  std::vector<Face> faces;
  HandPoint low{}, high{};

public:
  void Build(const std::vector<HandPoint> &input) {
    faces.clear();
    std::vector<HandPoint> points;
    for (auto p : input) {
      bool duplicate = false;
      for (auto q : points)
        if (HandLength(HandSub(p, q)) < .01f) {
          duplicate = true;
          break;
        }
      if (!duplicate)
        points.push_back(p);
    }
    if (points.size() < 4)
      return;
    low = high = points.front();
    for (auto p : points)
      for (int c = 0; c < 3; ++c) {
        low[c] = std::min(low[c], p[c]);
        high[c] = std::max(high[c], p[c]);
      }
    if (points.size() > 64)
      return; // Reject unexpected topology rather than unbounded work.
    for (size_t a = 0; a < points.size(); ++a)
      for (size_t b = a + 1; b < points.size(); ++b)
        for (size_t c = b + 1; c < points.size(); ++c) {
          auto n = HandCross(HandSub(points[b], points[a]),
                             HandSub(points[c], points[a]));
          float length = HandLength(n);
          if (length < .001f)
            continue;
          n = HandScale(n, 1 / length);
          bool positive = false, negative = false;
          for (auto p : points) {
            float d = HandDot(HandSub(p, points[a]), n);
            positive |= d > .01f;
            negative |= d < -.01f;
          }
          if (positive && negative)
            continue;
          if (positive)
            n = HandScale(n, -1);
          faces.push_back({points[a], points[b], points[c], n});
        }
  }
  bool Closest(HandPoint p, HandPoint &closest, bool &inside,
               float maximumDistance = -1) const {
    if (faces.empty())
      return false;
    // Cheap conservative rejection for distant hands; exact triangle work
    // is only needed inside the contact sphere expanded mesh bounds.
    if (maximumDistance >= 0) {
      for (int c = 0; c < 3; ++c)
        closest[c] = std::clamp(p[c], low[c], high[c]);
      if (HandLength(HandSub(p, closest)) > maximumDistance) {
        inside = false;
        return true;
      }
    }
    inside = true;
    float best = INFINITY;
    for (auto &f : faces) {
      float distance = HandDot(HandSub(p, f.a), f.normal);
      if (distance > .001f)
        inside = false;
      auto q = HandSub(p, HandScale(f.normal, distance));
      const auto ab = HandSub(f.b, f.a), ac = HandSub(f.c, f.a),
                 ap = HandSub(q, f.a);
      float aa = HandDot(ab, ab), bb = HandDot(ac, ac), xy = HandDot(ab, ac);
      float denominator = aa * bb - xy * xy;
      float u = (bb * HandDot(ap, ab) - xy * HandDot(ap, ac)) / denominator;
      float v = (aa * HandDot(ap, ac) - xy * HandDot(ap, ab)) / denominator;
      if (u < 0 || v < 0 || u + v > 1) {
        auto q0 = HandClosestSegment(p, f.a, f.b),
             q1 = HandClosestSegment(p, f.b, f.c),
             q2 = HandClosestSegment(p, f.c, f.a);
        q = q0;
        for (auto edge : {q1, q2})
          if (HandLength(HandSub(p, edge)) < HandLength(HandSub(p, q)))
            q = edge;
      }
      float d = HandLength(HandSub(p, q));
      if (d < best) {
        best = d;
        closest = q;
      }
    }
    return true;
  }
};
} // namespace mmvr
