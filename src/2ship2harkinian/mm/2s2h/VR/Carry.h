#pragma once
struct PlayState;struct Player;struct Actor;
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_CarryActorSupported(struct Actor*);
void MMVR_RecordCarryOffer(struct PlayState*,struct Actor*);
int MMVR_AttachCarryActor(struct PlayState*,struct Player*,struct Actor*);
#ifdef __cplusplus
}
#include "first_person.h"
extern "C" {
#include "z64math.h"
}
namespace mmvrgame {
bool CarryPropSurface(Actor*,const Vec3f& point,Vec3f& surface,bool& inside,float contactRadius=-1);
float CarryGrabSeparation(Actor*,const Vec3f& hand);
bool CarriedObject(Player*);
bool CarryReady(PlayState*,Player*);
int CarryHand(Player*);
void AdoptNativeCarry(PlayState*,Player*);
bool TryGrabCarry(PlayState*,Player*,int hand=-1, bool preview=false);
bool CarryPose(PlayState*,Player*,const mmvr::Matrix& grip,mmvr::Matrix& target);
}
#endif
