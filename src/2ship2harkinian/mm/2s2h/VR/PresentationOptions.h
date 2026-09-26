#pragma once
/* Presentation preferences never suppress actor updates or quest logic. */
struct Actor;
struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
#ifdef MMVR_ENABLE
int MMVR_HideCompanionFairy(struct PlayState* play, struct Actor* actor);
int MMVR_HideFairyArrow(void);
int MMVR_MuteCompanionFairy(struct Actor* actor);
int MMVR_MuteFairySound(unsigned short sfxId);
int MMVR_IntroMaskSkipTarget(struct PlayState* play, const void* script, int frame);
int MMVR_SkipIntroMaskVisuals(struct PlayState* play);
#else
static inline int MMVR_HideCompanionFairy(struct PlayState* p, struct Actor* a) { return 0; }
static inline int MMVR_HideFairyArrow(void) { return 0; }
static inline int MMVR_MuteCompanionFairy(struct Actor* a) { return 0; }
static inline int MMVR_MuteFairySound(unsigned short id) { return 0; }
static inline int MMVR_IntroMaskSkipTarget(struct PlayState* p, const void* s, int f) { return 0; }
static inline int MMVR_SkipIntroMaskVisuals(struct PlayState* p) { return 0; }
#endif
#ifdef __cplusplus
}
#endif
