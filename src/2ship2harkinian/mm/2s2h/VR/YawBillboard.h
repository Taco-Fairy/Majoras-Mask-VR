#pragma once
#include "global.h"
#ifdef MMVR_ENABLE
#include "visibility.h"
#endif

// Only use for native camera-facing sprites. Keep their mesh, scale, animation,
// material and world position; replace the camera yaw during each eye replay.
// nativeYaw is the camera basis, including its authored 180-degree adjustment,
// not an actor-relative angle or an extra decorative rotation.
static inline Mtx* MMVR_FinalizeYawBillboard(GraphicsContext* gfxCtx, s16 nativeYaw) {
    Mtx* matrix = Matrix_Finalize(gfxCtx);
#ifdef MMVR_ENABLE
    MtxF current;
    Matrix_Get(&current);
    MMVR_SetYawBillboardMatrix(matrix, nativeYaw * (3.14159265358979323846f / 32768.f),
                              current.mf[3][0], current.mf[3][1], current.mf[3][2]);
#endif
    return matrix;
}
#define MMVR_FINALIZE_YAW_BILLBOARD_AND_LOAD(packet, gfxCtx, nativeYaw) \
    gSPMatrix(packet, MMVR_FinalizeYawBillboard(gfxCtx, (s16)(nativeYaw)), \
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
