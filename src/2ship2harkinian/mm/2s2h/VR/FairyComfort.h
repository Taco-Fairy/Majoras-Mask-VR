#pragma once
struct PlayState; struct Actor;
#ifdef __cplusplus
extern "C" {
#endif
#ifdef MMVR_ENABLE
int MMVR_FairyReplacedByCutscene(struct PlayState*, struct Actor*);
float MMVR_FairyOpacity(struct PlayState*, struct Actor*);
int MMVR_FairyTrailHidden(struct PlayState*, const float*);
void MMVR_FairyTrailBegin(struct PlayState*, struct Actor*);
void MMVR_FairyTrailEnd(void);
int MMVR_FairyTrailSpawning(void);
void* MMVR_FairyWingDisplayList(struct PlayState*, struct Actor*, int, void*);
#else
static inline int MMVR_FairyReplacedByCutscene(struct PlayState* p, struct Actor* a) { return 0; }
static inline float MMVR_FairyOpacity(struct PlayState* p, struct Actor* a) { return 1.f; }
static inline int MMVR_FairyTrailHidden(struct PlayState* p, const float* v) { return 0; }
static inline void MMVR_FairyTrailBegin(struct PlayState* p, struct Actor* a) {}
static inline void MMVR_FairyTrailEnd(void) {}
static inline int MMVR_FairyTrailSpawning(void) { return 0; }
static inline void* MMVR_FairyWingDisplayList(struct PlayState* p, struct Actor* a, int limb, void* dl) { return dl; }
#endif
#ifdef __cplusplus
}
#endif
