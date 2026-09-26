#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState; struct Player; struct Actor;
int MMVR_FormProjectilePose(struct PlayState*,struct Player*,int hand,float* position,short* rotation);
int MMVR_PhysicalGrabEscape(struct Player*);
void MMVR_EndFinTargeting(struct Player*);
int MMVR_BindDekuBubble(struct PlayState*,struct Actor*,const void*);
void MMVR_DekuBubblePose(struct PlayState*,struct Player*,struct Actor*);
#ifdef __cplusplus
}
#include "first_person.h"
namespace mmvrgame {void RecordGrabShake(const mmvr::TrackingFrame&);void RecordFormTracking(const mmvr::TrackingFrame&,const mmvr::Matrix&,const mmvr::Matrix&);void ClearFormTracking();mmvr::Matrix FormHeadPose();
double FormTrackingTime();mmvr::Matrix FormHandPose(int hand);bool FormTrackingReady(Player*);}
#endif
