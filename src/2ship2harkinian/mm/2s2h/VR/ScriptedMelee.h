#pragma once
// Native scripted consumers need attack intent, not an animated first-person body.
// Include after the actor header (which declares Player).
#ifdef MMVR_ENABLE
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_ScriptedMeleeState(struct Player* player);
int MMVR_ScriptedMeleeAnimation(struct Player* player);
#ifdef __cplusplus
}
#endif
#else
#define MMVR_ScriptedMeleeState(player) ((player)->meleeWeaponState)
#define MMVR_ScriptedMeleeAnimation(player) ((player)->meleeWeaponAnimation)
#endif
