#pragma once
struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
// 0: native fallback, -1: physical mode without a valid mirror shield, 1: active.
int MMVR_MirrorShieldPose(struct PlayState*);
void MMVR_RegisterShieldEffect(struct PlayState*,const void* matrix);
int MMVR_ShieldBeamHit(struct PlayState*,const float* start,const float* end,float radius,float* hit);
#ifdef __cplusplus
}
#endif
