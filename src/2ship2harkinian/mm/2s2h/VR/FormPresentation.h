#pragma once
struct PlayState;struct Player;
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_FormAimStage(struct Player*);
int MMVR_DekuSpinning(struct Player*);
float MMVR_DekuTrailOpacity(const void* effect);
int MMVR_DekuFlowerStage(struct Player*);
int MMVR_EndFormShot(struct PlayState*,struct Player*);
void MMVR_DekuSpinTrail(struct PlayState*,struct Player*,float* tip,float* base);
int MMVR_GoronEffectMatrix(struct PlayState*,struct Player*);
void MMVR_RegisterFormEffect(const void* address);
const void* MMVR_FormEffectList(const char* path);
#ifdef __cplusplus
}
#include "first_person.h"
namespace mmvrgame {
bool FormReticleVisible(Player*);
void UpdateFormPresentation(mmvr::CameraFrame&);
void DrawFormFins(PlayState*);
void ProcessFormInput(PlayState*);
}
#endif
