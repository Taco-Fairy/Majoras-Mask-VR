#pragma once
struct PlayState;struct Player;
#ifdef __cplusplus
#include "first_person.h"
namespace mmvrgame {
void UpdateBow(const mmvr::TrackingFrame&,const mmvr::Matrix& view,const mmvr::Matrix& head,const mmvr::Matrix& model);
void ProcessBowInput(struct PlayState*);
void ClearBow();
mmvr::Matrix BowStringPose();
mmvr::Matrix BowArrowPose();
mmvr::Matrix ItemReticlePose();
void UpdateHookshotReticle();
bool BowHeld();
void DrawTrackedItems(struct PlayState*);
}
extern "C" {
#endif
struct PlayState;struct Player;
int MMVR_IndependentBow(struct Player*);
int MMVR_BowHasNockedArrow(struct Player*);
void MMVR_PlayerEmptyHands(struct PlayState*,struct Player*);
void MMVR_PlayerEquipBow(struct PlayState*,struct Player*,int item);
int MMVR_FireBow(struct PlayState*,struct Player*,const float* position,const short* rotation,float power);
#ifdef __cplusplus
}
#endif
