#pragma once
struct PlayState;struct Player;
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_DisableAutoClimb(struct PlayState*,struct Player*);
int MMVR_PhysicalClimbEnabled(struct PlayState*,struct Player*);
int MMVR_ClimbAvailable(struct PlayState*,struct Player*);
int MMVR_ClimbingInputContext(struct PlayState*);
int MMVR_DirectClimbMode(struct PlayState*,struct Player*);
int MMVR_ClimbVerticalIntent(struct PlayState*,struct Player*);
#ifdef __cplusplus
}
#include "first_person.h"
#include <array>
namespace mmvrgame {
bool ClimbingContext(PlayState*);
void ClearClimbing();
bool ClimbDebugVolume(int hand,float* point,float* radius,bool* grabbed);
void UpdateClimbing(const mmvr::TrackingFrame&,const mmvr::Matrix& view,const mmvr::Matrix& relativeHead);
void ProcessClimbingInput(PlayState*);
std::array<float,3> ResolveClimbDisplacement(PlayState*,Player*,std::array<float,3> step);
}
#endif
