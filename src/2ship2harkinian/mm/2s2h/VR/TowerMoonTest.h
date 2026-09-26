#pragma once
#include "DebugCutscenes.h"
extern "C" {
#include "overlays/actors/ovl_En_Fall/z_en_fall.h"
}

// Isolated, opt-in PC diagnostic for the native Final Day tower-opening shot.
// Resolve the authored 0xFFF1 Clock Tower entry; generated catalog order can change.
static mmvr::Pad NativeTowerMoonTest(PlayState* play, unsigned tick) {
    static bool started = false;
    static unsigned samples = 0;
    static std::ofstream log("native-tower-moon.log");
    mmvr::Pad pad;
    pad.active = true;
    if (!started && tick >= 60) {
        int openingIndex = -1;
        for (int i = 0; i < ARRAY_COUNT(debugCutscenes); ++i)
            if (debugCutscenes[i].scene == SCENE_CLOCKTOWER && debugCutscenes[i].cutsceneIndex == 0xFFF1) {
                openingIndex = i;
                break;
            }
        started = openingIndex >= 0 && MMVR_DebugCutsceneBegin(play, openingIndex) != 0;
        log << "portal=" << started << " sourceScene=" << play->sceneId << "\n" << std::flush;
        if (!started) Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    if (!started || play->sceneId != SCENE_CLOCKTOWER || gSaveContext.sceneLayer != 2 ||
        play->transitionTrigger != TRANS_TRIGGER_OFF)
        return pad;

    if (++samples % 10 == 1) {
        int moonCount = 0, readyCount = 0, drawnCount = 0, fireworksCount = 0, towerCount = 0;
        for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next) {
            if (actor->id != ACTOR_EN_FALL)
                continue;
            ++moonCount;
            readyCount += actor->init == nullptr && actor->draw != nullptr;
            drawnCount += actor->isDrawn != 0;
        }
        for (int category = 0; category < ACTORCAT_MAX; ++category)
            for (Actor* actor = play->actorCtx.actorLists[category].first; actor; actor = actor->next) {
                fireworksCount += actor->id == ACTOR_EN_HANABI;
                towerCount += actor->id == ACTOR_OBJ_TOKEIDAI;
            }
        const auto facts = mmvrgame::SceneFacts(play);
        log << "frame=" << play->gameplayFrames << " script=" << play->csCtx.scriptIndex
            << " cutsceneFrame=" << play->csCtx.curFrame << " state=" << int(play->csCtx.state)
            << " moon=" << moonCount << " ready=" << readyCount << " drawn=" << drawnCount
            << " lodSlot=" << Object_GetSlot(&play->objectCtx, OBJECT_LODMOON)
            << " fireworks=" << fireworksCount << " tower=" << towerCount
            << " fallSlot=" << Object_GetSlot(&play->objectCtx, OBJECT_FALL)
            << " theater=" << (mmvr::ResolveSceneView(facts, true) == mmvr::SceneView::Theater)
            << " nativeFar=" << play->view.zFar << "\n" << std::flush;
    }
    if (samples >= 180) Ship::Context::GetRawInstance()->GetWindow()->Close();
    return pad;
}
