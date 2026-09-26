#pragma once
#include "DebugCutscenes.h"
extern "C" int MMVR_DebugCutsceneBegin(PlayState*,int);
static mmvr::Pad NativeLessonLifecycle(PlayState* play,unsigned tick) {
    mmvr::Pad pad;pad.active=true;
    static std::ofstream log("native-lesson-lifecycle.csv");
    static bool entered=false,captured=false,finished=false;
    static unsigned promptTick=0, returnedTick=0;
    static bool mountedCapture=false;
    if(tick==1)log<<"tick,scene,frame,mode,action,focusHeight,formHeight,worldFill,envFill,envAlpha,interfaceFill,compositeAlpha\n";
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    if(tick==30)for(int i=0;i<ARRAY_COUNT(debugCutscenes);++i)
        if(debugCutscenes[i].scene==SCENE_SPOT00 && debugCutscenes[i].cutsceneIndex==0xFFF2)
            entered=MMVR_DebugCutsceneBegin(play,i);
    auto* p=GET_PLAYER(play);
    if(tick%10==0)log<<tick<<','<<play->sceneId<<','<<play->csCtx.curFrame<<','<<int(play->msgCtx.msgMode)<<','<<play->msgCtx.ocarinaAction<<','<<p->actor.focus.pos.y-p->actor.world.pos.y<<','<<mmvrgame::FormEyeHeight(p)<<','<<int(play->worldCoverAlpha)<<','<<int(play->envCtx.fillScreen)<<','<<int(play->envCtx.screenFillColor[3])<<','<<int(play->interfaceCtx.screenFillAlpha)<<','<<mmvr::ScreenFade()[3]<<'\n'<<std::flush;
    if(entered && play->msgCtx.msgMode==MSGMODE_SONG_PROMPT) {
        if(!captured){mmvr::RequestNativeCapture("lesson-prompt");captured=true;}
        static const unsigned short notes[]={BTN_CRIGHT,BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_A,BTN_CDOWN};
        unsigned index=promptTick/20;
        if(index<6 && promptTick%20<8)pad.buttons=notes[index];
        ++promptTick;
    } else if(play->msgCtx.msgLength && tick%24==0)pad.buttons=BTN_A;
    if(promptTick && play->msgCtx.msgMode==MSGMODE_SONG_PROMPT_SUCCESS && !finished){finished=true;mmvr::RequestNativeCapture("lesson-success");}
    if(finished && play->sceneId==SCENE_SPOT00 && play->csCtx.curFrame>=700 && !mountedCapture) {
        mountedCapture=true;mmvr::RequestNativeCapture("lesson-departure");
    }
    if(finished && play->sceneId!=SCENE_SPOT00 && !returnedTick) returnedTick=tick;
    if(returnedTick && tick==returnedTick+30) mmvr::RequestNativeCapture("lesson-return");
    if(tick==1900 || (returnedTick && tick>=returnedTick+40)) {
        log<<"result,"<<entered<<','<<captured<<','<<finished<<'\n'<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}
