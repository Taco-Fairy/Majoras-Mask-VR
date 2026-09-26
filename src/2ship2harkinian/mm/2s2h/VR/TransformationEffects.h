#pragma once
struct Player;struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_TransformationStyle(struct Player*);
void MMVR_UpdateTransformationEffects(struct PlayState*);
void MMVR_DrawTransformationEffects(struct PlayState*);
#ifdef __cplusplus
}
#endif
