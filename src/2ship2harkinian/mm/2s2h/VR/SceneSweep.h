#pragma once
#include "DebugLocations.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <chrono>
#include <fstream>
#include <iomanip>

// Opt-in isolated scene-load audit. The NativeTest gate and save-write guard
// are required; ordinary play and the player's selected location are untouched.
static mmvr::Pad NativeSceneSweep(PlayState* play, unsigned tick) {
    static int index = -1, wait = 0, dwell = 0;
    static bool entering = false;
    static std::ofstream log = [] {
        std::ofstream result("native-scene-sweep.log");
        const char* token = std::getenv("MMVR_SESSION_TOKEN");
        result << "session " << (token ? token : "") << "\n";
        result << "scope native-scene-entry-rendering-not-campaign-or-headset-fps\n" << std::flush;
        return result;
    }();
    mmvr::Pad pad;
    pad.active = true;
    const auto close = [&] { log.flush(); Ship::Context::GetRawInstance()->GetWindow()->Close(); };
    if (index < 0 && tick <= 60) return pad;
    if (index < 0 || dwell >= 120) {
        ++index;
        if (index == int(ARRAY_COUNT(debugLocations))) {
            log << "COMPLETE visits=" << index << "\n";
            close();
            return pad;
        }
        if(index>0 && debugLocations[index-1].scene==SCENE_PIRATE)GET_PLAYER(play)->currentMask=PLAYER_MASK_NONE;
        wait = dwell = 0;
        entering = true;
        gSaveContext.save.day = gSaveContext.save.eventDayCount = 2;
        gSaveContext.save.time = CLOCK_TIME(12, 0);
        gSaveContext.save.isNight = false;
        gSaveContext.nextCutsceneIndex = gSaveContext.cutsceneTrigger = gSaveContext.respawnFlag = 0;
        gSaveContext.save.cutsceneIndex = 0;
        // Actor_InitContext reads the ORIGINAL scene's saved switch bank.
        // Set it before actors initialize, not after the inverter has queued.
        const int targetScene=debugLocations[index].scene;
        if(targetScene==SCENE_F40 || targetScene==SCENE_F41){
            auto& flags=gSaveContext.cycleSceneFlags[Play_GetOriginalSceneId(targetScene)];
            if(targetScene==SCENE_F41)flags.switch0|=1u<<20;
            else flags.switch0&=~(1u<<20);
        }
        play->pauseCtx.state = PAUSE_STATE_OFF;
        play->nextEntrance = debugLocations[index].entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        log << "enter index=" << index << " expected=" << debugLocations[index].scene
            << " name=" << std::quoted(debugLocations[index].name) << "\n" << std::flush;
        return pad;
    }
    if (entering && play->sceneId == debugLocations[index].scene) {
        // Match the native inverter state to the scene under inspection. A
        // mismatched flag correctly causes Obj_Wturn to leave this scene.
        if(play->sceneId==SCENE_F40 || play->sceneId==SCENE_F41)
            for(Actor* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;actor;actor=actor->next)
                if(actor->id==ACTOR_OBJ_WTURN && actor->params>=0 && actor->params<128){
                    if(play->sceneId==SCENE_F41)Flags_SetSwitch(play,actor->params);
                    else Flags_UnsetSwitch(play,actor->params);
                }
    }
    // This is a scene-load/render dwell, not the guard-capture scenario. Use
    // the native Stone Mask prerequisite rather than suppressing transitions.
    if(index>=0 && debugLocations[index].scene==SCENE_PIRATE && play->sceneId==SCENE_PIRATE)
        GET_PLAYER(play)->currentMask=PLAYER_MASK_STONE;
    if (entering) {
        if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF ||
            play->roomCtx.status != 0 || play->sceneId != debugLocations[index].scene) {
            if (++wait > 900) {
                log << "ERROR scene-timeout index=" << index << " actual=" << play->sceneId
                    << " transition=" << int(play->transitionMode) << "\n";
                close();
            }
            return pad;
        }
        entering = false;
        log << "ready index=" << index << " scene=" << play->sceneId << " nativeFrame=" << play->gameplayFrames
            << "\n" << std::flush;
    }
    if (dwell < 120 && (play->sceneId != debugLocations[index].scene || play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        auto* player = GET_PLAYER(play);
        log << "unexpected-transition index=" << index << " scene=" << play->sceneId
            << " nextEntrance=" << play->nextEntrance << " respawn=" << int(gSaveContext.respawnFlag)
            << " position=" << player->actor.world.pos.x << ',' << player->actor.world.pos.y << ',' << player->actor.world.pos.z
            << " floor=" << player->actor.floorHeight << " nativeFrame=" << play->gameplayFrames << '\n' << std::flush;
    }
    ++dwell;
    gSaveContext.save.saveInfo.playerData.health = gSaveContext.save.saveInfo.playerData.healthCapacity;
    if (dwell == 120) {
        const auto memory = FrameInterpolation_GetRecordingMemory();
        log << "sample index=" << index << " scene=" << play->sceneId << " nativeFrame=" << play->gameplayFrames
            << " poolBytes=" << memory.retainedBytes << " poolPeak=" << memory.peakBytes
            << " poolAllocations=" << memory.allocations << "\n" << std::flush;
    }
    return pad;
}
