#pragma once

#include "include/z64math.h"

#ifdef __cplusplus

#include <cstdint>
#include <unordered_map>

std::unordered_map<Mtx*, MtxF> FrameInterpolation_Interpolate(float step);

// Keep this scratch within one native tick. Its map remains valid until its next
// evaluation; native, eye and HUD consumers must finish before it is reused.
class FrameInterpolationScratch {
  public:
    FrameInterpolationScratch() = default;

  private:
    std::unordered_map<Mtx*, MtxF> replacements;
    uint64_t recordingGeneration = 0;
    bool valid = false;
    friend const std::unordered_map<Mtx*, MtxF>& FrameInterpolation_Interpolate(
        float step, FrameInterpolationScratch& scratch, bool compiled);
};

const std::unordered_map<Mtx*, MtxF>& FrameInterpolation_Interpolate(
    float step, FrameInterpolationScratch& scratch, bool compiled = true);

#ifdef MMVR_ENABLE
struct FrameInterpolationRecordingMemory { std::size_t allocations, retainedBytes, peakBytes; };
FrameInterpolationRecordingMemory FrameInterpolation_GetRecordingMemory();
bool FrameInterpolation_VerifyScratch();
#endif

extern "C" {

#endif

void FrameInterpolation_ShouldInterpolateFrame(bool shouldInterpolate);
// Preserve world motion when only the authored camera cuts in first-person VR.
void FrameInterpolation_SetCameraPolicy(bool interpolateCamera, bool continuousWorld);

// Drop the old coordinate system at the next recording, independently of camera policy.
void FrameInterpolation_ResetHistory(void);

void FrameInterpolation_StartRecord(void);

void FrameInterpolation_StopRecord(void);

void FrameInterpolation_RecordOpenChild(const void* a, int b);

void FrameInterpolation_RecordCloseChild(void);

void FrameInterpolation_DontInterpolateCamera(void);

int FrameInterpolation_GetCameraEpoch(void);

void FrameInterpolation_IgnoreActorMtx(void);

void FrameInterpolation_InterpolateWiderAngles();

void FrameInterpolation_RecordActorPosRotMatrix(void);

void FrameInterpolation_RecordMatrixPush(void);

void FrameInterpolation_RecordMatrixPop(void);

void FrameInterpolation_RecordMatrixPut(MtxF* src);

void FrameInterpolation_RecordMatrixMult(MtxF* mf, u8 mode);

void FrameInterpolation_RecordMatrixTranslate(f32 x, f32 y, f32 z, u8 mode);

void FrameInterpolation_RecordMatrixScale(f32 x, f32 y, f32 z, u8 mode);

void FrameInterpolation_RecordMatrixRotate1Coord(u32 coord, f32 value, u8 mode);

void FrameInterpolation_RecordMatrixRotateZYX(s16 x, s16 y, s16 z, u8 mode);

void FrameInterpolation_RecordMatrixTranslateRotateZYX(Vec3f* translation, Vec3s* rotation);

void FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(f32 translateX, f32 translateY, f32 translateZ, Vec3s* rot);

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dest);

void FrameInterpolation_RecordMatrixToMtx(Mtx* dest, char* file, s32 line);

void FrameInterpolation_RecordMatrixReplaceRotation(MtxF* mf);

void FrameInterpolation_RecordMatrixRotateAxis(f32 angle, Vec3f* axis, u8 mode);

void FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF* src, Mtx* dest);

#ifdef __cplusplus
}
#endif
