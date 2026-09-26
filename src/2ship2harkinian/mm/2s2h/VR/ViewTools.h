#pragma once
#ifdef __cplusplus
#include "first_person.h"
struct PlayState;struct Camera;
namespace mmvrgame {
bool ViewToolCamera(const mmvr::TrackingFrame&,mmvr::CameraFrame&);
bool PhotoRenderView(mmvr::CameraFrame&);
void RecordPhotoHead(PlayState*,const mmvr::Matrix&,const mmvr::Matrix&);
}
extern "C" {
#endif
struct PlayState;struct Camera;struct Player;
int MMVR_ViewToolCamera(struct PlayState*,struct Camera*,float*,float*,float*);
int MMVR_ViewToolOverlay(void);
int MMVR_ScopeEyeOverlay(void);
int MMVR_ScopeInput(struct PlayState*,struct Player*);
#ifdef __cplusplus
}
#endif
