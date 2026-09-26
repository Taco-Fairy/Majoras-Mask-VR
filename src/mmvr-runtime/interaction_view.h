#pragma once
#include "projection.h"
namespace mmvr {
// Head-relative point in metres. Either eye may see the target; do not use
// the renderer's intentionally expanded culling frustum for interaction.
inline bool InteractionPointInEye(const Matrix& headToEye, const XrFovf& fov,
                                  float x, float y, float z) {
    float p[3]{};
    for (int i=0;i<3;++i) p[i]=x*headToEye.m[0][i]+y*headToEye.m[1][i]+z*headToEye.m[2][i]+headToEye.m[3][i];
    const float depth=-p[2];
    return depth>0 && p[0]>=depth*std::tan(fov.angleLeft) && p[0]<=depth*std::tan(fov.angleRight) &&
           p[1]>=depth*std::tan(fov.angleDown) && p[1]<=depth*std::tan(fov.angleUp);
}
}
