#pragma once
#include "NativeCombat.h"
namespace mmvrgame {
void ProcessGoronInput(PlayState*);
bool GoronFists(Player*);
void UpdateGoronCombat(const mmvr::TrackingFrame&,const mmvr::Matrix&,const mmvr::Matrix&);
void ResolveGoronCombat(PlayState*);
Collider* GoronDebugCollider(int hand);
float GoronPunchFire(int hand);
#ifdef MMVR_LOCAL_TEST_TOOLS
void SetGoronRayReview(bool legacy);
unsigned GoronRayReviewCount();
#endif
}
