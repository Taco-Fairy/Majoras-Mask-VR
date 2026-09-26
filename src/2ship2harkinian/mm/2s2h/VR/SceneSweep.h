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
        wait = dwell = 0;
        entering = true;
        gSaveContext.save.day = gSaveContext.save.eventDayCount = 2;
        gSaveContext.save.time = CLOCK_TIME(12, 0);
        gSaveContext.save.isNight = false;
        gSaveContext.nextCutsceneIndex = gSaveContext.cutsceneTrigger = gSaveContext.respawnFlag = 0;
        gSaveContext.save.cutsceneIndex = 0;
        play->pauseCtx.state = PAUSE_STATE_OFF;
        play->nextEntrance = debugLocations[index].entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        log << "enter index=" << index << " expected=" << debugLocations[index].scene
            << " name=" << std::quoted(debugLocations[index].name) << "\n" << std::flush;
        return pad;
    }
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
