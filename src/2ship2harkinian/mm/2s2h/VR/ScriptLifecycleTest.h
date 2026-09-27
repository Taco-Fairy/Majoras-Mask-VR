#pragma once
#include "DebugCutscenes.h"
#include "2s2h/ShipInit.hpp"
extern "C" int MMVR_DebugCutsceneBegin(PlayState*,int);
extern "C" s16 sSceneCutsceneCount;
static bool nativeScriptLaunched=false;
static unsigned nativeScriptProgress=0;
static bool NativeScriptTerminalBoundary() {
    if(!nativeScriptLaunched || gSaveContext.gameMode!=GAMEMODE_FILE_SELECT)return false;
    static bool finished=false;
    if(!finished){
        finished=true;
        const int index=std::atoi(std::getenv("MMVR_SCRIPT_CASE"));
        std::ofstream log("native-script-lifecycle.log",std::ios::app);
        log<<"BLOCKED script="<<index<<" progress="<<nativeScriptProgress
           <<" reason=unexpected-file-select-boundary\n";
        log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return true;
}
// A direct script recipe tests entry/progression/completion, not every dialogue
// branch or visual composition. Interactive lessons use dedicated fixtures.
static mmvr::Pad NativeScriptLifecycle(PlayState* play,unsigned tick) {
    static const int index=[] {const char* v=std::getenv("MMVR_SCRIPT_CASE");return v?std::atoi(v):-1;}();
    static bool launched=false,started=false;
    static unsigned progressed=0,settled=0,lastFrame=0;
    static bool described=false;
    static unsigned budget=5000;
    static std::ofstream log("native-script-lifecycle.log");
    mmvr::Pad pad;pad.active=true;
    auto finish=[&](const char* status,const char* why){log<<status<<" script="<<index<<" progress="<<progressed<<" reason="<<why<<'\n'<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
    if(index<0||index>=scriptedCutsceneCount){finish("FAIL","invalid-index");return pad;}
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    if(tick==30){
        // The seed skips the boot movie for fast startup; disable that shortcut
        // only after entering gameplay so the movie itself can be exercised.
        CVarSetInteger("gEnhancements.Cutscenes.SkipToFileSelect",0);
        ShipInit::Init("gEnhancements.Cutscenes.SkipToFileSelect");
        launched=MMVR_DebugCutsceneBegin(play,index);if(!launched){finish("FAIL","launch-rejected");return pad;}}
    nativeScriptLaunched=launched;
    if(!launched)return pad;
    const auto& entry=debugCutscenes[index];
    if (std::getenv("MMVR_SCRIPT_DIRECT") && !started && play->sceneId==entry.scene && tick>90 &&
        play->transitionMode==TRANS_MODE_OFF && play->csCtx.state==CS_STATE_IDLE) {
        // Exercise the real scene's manager entry, not a fabricated script/camera.
        // This does not certify the actor interaction that normally queues it.
        for (s16 id=0;id<sSceneCutsceneCount;++id) {
            if (CutsceneManager_GetCutsceneScriptIndex(id)!=0) continue;
            CutsceneManager_Queue(id);
            if (CutsceneManager_IsNext(id)) {
                CutsceneManager_Start(id, &GET_PLAYER(play)->actor);
                log << "explicit-manager-start id=" << id << '\n' << std::flush;
            }
            break;
        }
    }
    if (!described && play->sceneId == entry.scene && play->transitionMode == TRANS_MODE_OFF) {
        described = true;
        log << "script-metadata count=" << unsigned(play->csCtx.scriptListCount)
            << " selected=" << play->csCtx.scriptIndex << " layer=" << unsigned(gSaveContext.sceneLayer) << '\n';
        for (unsigned i=0;i<play->csCtx.scriptListCount;++i) {
            const auto& script=play->csCtx.scriptList[i];
            log << "script-entry index=" << i << " spawnFlags=" << unsigned(script.spawnFlags)
                << " nextEntrance=" << script.nextEntrance << '\n';
        }
        log << std::flush;
    }
    if (play->csCtx.script && play->csCtx.state != CS_STATE_IDLE) {
        const auto* commands = (const s32*)play->csCtx.script;
        const unsigned endFrame = unsigned(commands[1]);
        if (endFrame < 60000) budget = std::max(budget, endFrame + 1000);
    }
    if(play->sceneId==entry.scene && play->csCtx.state!=CS_STATE_IDLE) {
        started=true;settled=0;
        if(play->csCtx.curFrame!=lastFrame)++progressed;
        lastFrame=play->csCtx.curFrame;nativeScriptProgress=progressed;
    } else if(started && play->transitionTrigger==TRANS_TRIGGER_OFF && play->transitionMode==TRANS_MODE_OFF)++settled;
    auto* p=GET_PLAYER(play);
    if(!std::isfinite(p->actor.world.pos.x)||!std::isfinite(p->actor.world.pos.y)||!std::isfinite(p->actor.world.pos.z)){finish("FAIL","nonfinite-player-position");return pad;}
    if(play->msgCtx.msgLength && tick%24==0)pad.buttons=BTN_A;
    if(tick%60==0)log<<"tick="<<tick<<" scene="<<play->sceneId<<" state="<<int(play->csCtx.state)<<" frame="<<play->csCtx.curFrame<<" message="<<int(play->msgCtx.msgMode)<<" textId="<<play->msgCtx.currentTextId<<" textState="<<int(Message_GetState(&play->msgCtx))<<" choice="<<int(play->msgCtx.choiceIndex)<<" endType="<<int(play->msgCtx.textboxEndType)<<" csIndex="<<gSaveContext.save.cutsceneIndex<<'\n'<<std::flush;
    if(settled>=30 && progressed>=2)finish("PASS","native-script-completed");
    else if(tick>=budget || (!started&&tick>=600))finish("BLOCKED","needs-authored-prerequisite-or-input");
    return pad;
}
