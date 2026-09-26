#pragma once
#include "first_person.h"
struct PlayState;
namespace mmvrgame {
mmvr::Matrix HeldMaskPose(const mmvr::TrackingFrame&,const mmvr::Matrix&,const mmvr::Matrix&);
void DrawHeldMask(PlayState*);
void DrawMaskModel(PlayState*,int item);
void UpdateMaskContext(PlayState*);
void ProcessMasks(PlayState*);
}
