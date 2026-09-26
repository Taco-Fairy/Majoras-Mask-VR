#pragma once
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
namespace mmvr {
// Four independent clip coordinates. On ARM64 this keeps the existing Clang
// multiply/FMA/add order in each lane; lighting, clipping and fog stay unchanged.
inline void TransformClipVertex(float x, float y, float z, const float m[4][4], float out[4]) {
#if defined(__aarch64__)
    auto clip = vmulq_n_f32(vld1q_f32(m[1]), y);
    clip = vfmaq_n_f32(clip, vld1q_f32(m[0]), x);
    clip = vfmaq_n_f32(clip, vld1q_f32(m[2]), z);
    vst1q_f32(out, vaddq_f32(clip, vld1q_f32(m[3])));
#else
    for (int i = 0; i < 4; ++i)
        out[i] = x * m[0][i] + y * m[1][i] + z * m[2][i] + m[3][i];
#endif
}
} // namespace mmvr
