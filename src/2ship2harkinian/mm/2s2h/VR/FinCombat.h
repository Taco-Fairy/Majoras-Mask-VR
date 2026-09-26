#pragma once
#include "first_person.h"
extern "C" {
#include "global.h"
}
namespace mmvrgame {
void ClearFinCombat();
void UpdateFinCombat(const mmvr::TrackingFrame&,const mmvr::Matrix& view,const mmvr::Matrix& head,const mmvr::Matrix* fins);
void ResolveFinCombat(PlayState*);
bool PhysicalFinMode(Player*);
Collider* FinDebugCollider(int hand);
}
