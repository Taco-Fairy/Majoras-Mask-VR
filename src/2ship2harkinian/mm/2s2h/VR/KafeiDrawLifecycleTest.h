#pragma once
#include "DebugLocations.h"
extern "C" {
#include "overlays/actors/ovl_En_Test3/z_en_test3.h"
void func_80A3F0B0(EnTest3*, PlayState*);
void func_80A40824(EnTest3*, PlayState*);
int MMVR_KafeiDrawCheckPhase();
}
static mmvr::Pad NativeKafeiDrawLifecycle(PlayState* play,unsigned tick) {
    mmvr::Pad pad{};pad.active=true;
    static bool entered=false,returned=false,handedOff=false,roomRequested=false,roomFinished=false;
    static unsigned handoffFrames=0;
    if(tick%30==0) {
        std::ofstream probe("native-kafei-progress.txt",std::ios::app);
        probe << tick << " scene=" << play->sceneId << " transition=" << play->transitionMode
              << " room=" << int(play->roomCtx.status) << " actor=" << GET_PLAYER(play)->actor.id
              << " phase=" << MMVR_KafeiDrawCheckPhase() << "\n";
    }
    if(tick>800)throw std::runtime_error("Kafei handoff draw fixture timed out");
    if(tick==30) {
        for(int i=0;i<ARRAY_COUNT(debugLocations);++i)if(debugLocations[i].kafeiPreset==2) {
            entered=MMVR_DebugLocationBegin(play,i)!=0;break;
        }
        if(!entered)throw std::runtime_error("Kafei fixture could not enter native hideout");
    }
    if(!entered || play->sceneId!=SCENE_SECOM || play->transitionMode!=TRANS_MODE_OFF || play->roomCtx.status)return pad;
    if(!roomRequested) {
        roomRequested=Room_RequestNewRoom(play,&play->roomCtx,1)!=0;
        if(!roomRequested)throw std::runtime_error("Could not load Kafei puzzle room");
        return pad;
    }
    if(!roomFinished) { Room_FinishRoomChange(play,&play->roomCtx);roomFinished=true; }
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    auto* player=GET_PLAYER(play);
    if(MMVR_KafeiDrawCheckPhase()==2) {
        if(MMVR_ControlledKafei(player))throw std::runtime_error("Kafei fixture failed return to Link");
        Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;
    }
    if(MMVR_ControlledKafei(player)) {
        if(MMVR_KafeiDrawCheckPhase()==1 && !returned) {
            func_80A3F0B0((EnTest3*)player,play);returned=true;
        }
    } else if(!returned && !handedOff && ++handoffFrames>100) {
        // Invoke the native handoff on the real scene actor without waiting for
        // the puzzle dialogue. Keep its skeleton, model and update/draw intact.
        auto* kafei=(EnTest3*)SubS_FindActor(play,nullptr,ACTORCAT_NPC,ACTOR_EN_TEST3);
        if(!kafei)throw std::runtime_error("Native hideout Kafei missing");
        play->actorCtx.flags|=ACTORCTX_FLAG_4;
        func_80A40824(kafei,play);
        // Actor_ChangeCategory defers list relinking until the native update.
        handedOff=kafei->player.actor.category==ACTORCAT_PLAYER;
        if(!handedOff)throw std::runtime_error("Native Kafei handoff was rejected");
    }
    return pad;
}
