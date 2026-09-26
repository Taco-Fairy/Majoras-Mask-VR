#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;struct Player;
float MMVR_SwimEyeHeight(struct Player*,float modelHeight,float standingHeight);
float MMVR_SwimMovementScale(struct PlayState*,struct Player*,int vertical);
int MMVR_SwimAim(struct PlayState*,struct Player*,short* yaw,short* pitch);
int MMVR_SwimDive(struct PlayState*,struct Player*,float* horizontal,short* yaw,float* vertical);
#ifdef __cplusplus
}
#endif
