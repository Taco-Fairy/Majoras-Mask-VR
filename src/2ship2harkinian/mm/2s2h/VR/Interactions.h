#pragma once
struct PlayState;struct Player;
#ifdef __cplusplus
#include "first_person.h"
namespace mmvrgame {
void RecordTracking(const mmvr::TrackingFrame&,const mmvr::Matrix& viewPose,const mmvr::Matrix& relativeHead);
void ClearTracking();
void RecordPhysicalPushTracking(const mmvr::TrackingFrame&, const mmvr::Matrix& view,
                               const mmvr::Matrix& relativeHead, const mmvr::Matrix* hands);
void ApplyPhysicalPushHandLock(PlayState*, Player*, mmvr::Matrix* hands);
void ClearPhysicalPushTracking();
mmvr::Matrix CarryPalmPose(const mmvr::Matrix& gripPose,int hand);
bool TrackedMaskHand(PlayState*,mmvr::Matrix&,int hand=-1);
bool TrackedMuzzle(PlayState*,Player*,mmvr::Matrix&);
void OverrideTrackedItemHand(mmvr::Matrix& hand);
void FillHeldActorFrame(mmvr::CameraFrame&);
mmvr::Matrix DekuGuardPose(PlayState*,Player*);
mmvr::Matrix DekuGuardCorrection(PlayState*,Player*);
}
extern "C" {
#endif
struct PlayState;struct Player;struct Actor;
int MMVR_ButtonInteractionVisible(struct PlayState*,struct Actor*);
int MMVR_LookTrigger(struct PlayState*,struct Actor*,int nativeResult);
void MMVR_TrackedActorBegin(struct PlayState*,struct Actor*);
void MMVR_TrackedActorEnd(struct PlayState*,struct Actor*);
int MMVR_ItemPresentationActive(struct Player*);
void MMVR_RecordDekuGuard(struct PlayState*,struct Player*);
void MMVR_BindDekuGuard(const void*);
int MMVR_PhysicalPushReady(struct PlayState*, struct Player*);
int MMVR_BeginPhysicalPush(struct PlayState*, struct Player*);
int MMVR_PhysicalPushActive(struct Player*);
int MMVR_PhysicalPushHeld(struct PlayState*, struct Player*);
int MMVR_PhysicalPushTargetDraw(struct PlayState*, struct Player*, struct Actor*);
void MMVR_EndPhysicalPush(struct Player*);
void MMVR_PhysicalPushMovement(struct PlayState*, struct Player*, float* speed, short* yaw);
int MMVR_TakeDekuPhysicalSpinRequest(struct PlayState*, struct Player*);
int MMVR_TapTargeting(void);
int MMVR_HoldTargeting(void);
int MMVR_SwordControlActive(struct Player*);
void MMVR_PhysicalDeityBeam(struct PlayState*,struct Player*);
int MMVR_IndependentSword(struct Player*);
int MMVR_DisableButtonMelee(struct Player*);
int MMVR_DisableJumpAttack(struct Player*);
int MMVR_IndependentHookshot(struct Player*);
int MMVR_HookshotInFlight(struct Player*);
void MMVR_PlayerEquipHookshot(struct PlayState*,struct Player*);
int MMVR_UseHookshot(struct PlayState*,struct Player*);
void MMVR_InitSwordDamage(struct Player*);
void MMVR_SwordContact(struct PlayState*,struct Player*);
void MMVR_ProcessInteractions(struct PlayState*);
void MMVR_FilterAttackCollisions(struct PlayState*);
void MMVR_AfterAttackCollision(struct PlayState*);
int MMVR_ShieldTransform(struct PlayState*,struct Player*,const float*,float*);
int MMVR_TrackedShieldMode(struct Player*);
float MMVR_NativeSwordLength(struct Player*);
int MMVR_PhysicalSwordCollider(struct PlayState*,struct Player*);
void MMVR_ApplyThrowVelocity(struct PlayState*,struct Player*,struct Actor*);
void MMVR_NativeThrow(struct PlayState*,struct Player*);
void MMVR_RecordNativeRightHand(const float* matrix);
int MMVR_TrackedAimActive(struct PlayState*,struct Player*);
void MMVR_UpdateHeldItem(struct PlayState*,struct Player*);
const void* MMVR_TrackedRightHandMesh(struct Player*);
const void* MMVR_TrackedLeftHandMesh(struct Player*);
#ifdef __cplusplus
}
#endif
