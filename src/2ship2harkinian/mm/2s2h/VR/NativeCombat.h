#pragma once
#include "first_person.h"
#include "NativeClimbing.h"
#include "FormPresentation.h"
#include "sword_charge.h"
struct PlayState;struct Player;
extern "C" {
#include "z64collision_check.h"
}
namespace mmvrgame {
bool InteractionsEligible(PlayState*,Player*);
XrVector3f InteractionHead();
void ClearCombat();
float AdvanceSpinTurn(const mmvr::TrackingFrame&);
void ClearBodyTracking();
void RecordBodyTracking(const mmvr::TrackingFrame&,const mmvr::Matrix&,const mmvr::Matrix&);
mmvr::Matrix ShieldModelPose();
void UpdateDeityTrigger(const mmvr::TrackingFrame&,const mmvr::Matrix&,const mmvr::Matrix&);
void ProcessCombatInput(PlayState*);
void ProcessSwordEquip(PlayState*,bool enabled);
bool ShieldRaised();
Collider* MeleeDebugCollider();
void QueuePhysicalCombat(PlayState*,Player*);
void UpdateShield(const mmvr::TrackingFrame&,const mmvr::Matrix& rightHand);
const void* TrackedShieldMesh(Player*);
void UpdateSwordDiagnostics(const mmvr::TrackingFrame&,mmvr::Matrix& leftHand);
mmvr::SwordChargeVisual TrackedSwordCharge(PlayState*);
bool DrawTrackedSwordCharge(PlayState*, void* handMatrix, bool mirrored);
}
extern "C" int MMVR_TakeDekuPhysicalSpinRequest(PlayState*,Player*);
