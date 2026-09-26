#pragma once
#include <openxr/openxr.h>
#include <cmath>
namespace mmvr {
// The midpoint is the cyclopean camera pose. Average shortest-arc orientations
// to support canted displays too; do not take either individual eye as the head.
inline XrPosef StereoHeadPose(const XrPosef& left, const XrPosef& right) {
    const auto& a = left.orientation;
    const auto& b = right.orientation;
    const float sign = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w < 0 ? -1.f : 1.f;
    XrQuaternionf q{a.x + sign*b.x, a.y + sign*b.y, a.z + sign*b.z, a.w + sign*b.w};
    const float length = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (length > .00001f) { q.x /= length; q.y /= length; q.z /= length; q.w /= length; }
    else q = a;
    return {q, {(left.position.x + right.position.x)*.5f,
                (left.position.y + right.position.y)*.5f,
                (left.position.z + right.position.z)*.5f}};
}
} // namespace mmvr
