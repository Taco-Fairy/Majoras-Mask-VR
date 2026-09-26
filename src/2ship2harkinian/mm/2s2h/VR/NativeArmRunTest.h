#pragma once
#include "ArmRun.h"
extern "C" {
extern f32 sControlStickMagnitude;
extern s16 sControlStickAngle;
s32 Player_GetMovementSpeedAndYaw(Player*, f32*, s16*, f32, PlayState*);
}
static void NativeArmRunTest(PlayState* play,const Player& baseline,std::ostream& log) {
    auto* p=GET_PLAYER(play);auto saved=*p;auto settings=mmvr::GetSettings();
    const auto oldMagnitude=sControlStickMagnitude;const auto oldAngle=sControlStickAngle;
    sControlStickMagnitude=60;sControlStickAngle=0;
    auto cs=play->csCtx.state;auto msg=play->msgCtx.msgMode;auto pause=play->pauseCtx.state;auto transition=play->transitionTrigger;
    play->csCtx.state=CS_STATE_IDLE;play->msgCtx.msgMode=MSGMODE_NONE;play->pauseCtx.state=PAUSE_STATE_OFF;play->transitionTrigger=TRANS_TRIGGER_OFF;
    mmvr::GetSettings().Set(mmvr::Setting::MovementSpeed,1.2f);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    log<<",\"armRunForms\":[";
    for(int form=0;form<PLAYER_FORM_MAX;++form) {
        *p=baseline;p->transformation=form;p->currentMask=PLAYER_MASK_NONE;p->csAction=PLAYER_CSACTION_NONE;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->rideActor=nullptr;p->actor.scale={.01f,.01f,.01f};p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
        mmvrgame::ClearArmRun();p->unk_AA5=PLAYER_UNKAA5_0;p->unk_B50=6.f;p->unk_AB8=0;p->floorPitch=0;
        float normalTarget=0;short targetYaw=0;Player_GetMovementSpeedAndYaw(p,&normalTarget,&targetYaw,.018f,play);
        mmvr::TrackingFrame frame{};frame.epoch=form+1;
        frame.handValid[0]=frame.handValid[1]=frame.handTracked[0]=frame.handTracked[1]=true;
        for(int i=0;i<180;++i) {
            frame.timeSeconds=10+i/90.;float s=.025f*std::sin(i/90.f*11.3f);
            frame.hands[0].position={-.3f,1,s};frame.hands[1].position={.3f,1,-s};
            mmvrgame::UpdateArmRun(frame);
        }
        float running=MMVR_MovementScale(play,p);
        float runningTarget=0;Player_GetMovementSpeedAndYaw(p,&runningTarget,&targetYaw,.018f,play);
        frame.handTracked[0]=false;frame.timeSeconds+=1./90;mmvrgame::UpdateArmRun(frame);
        float stopped=MMVR_MovementScale(play,p);
        if(form)log<<",";
        log<<"{\"form\":"<<form<<",\"running\":"<<running<<",\"stopped\":"<<stopped
           <<",\"normalTarget\":"<<normalTarget<<",\"runningTarget\":"<<runningTarget
           <<",\"correct\":"<<(std::abs(running-1.44f)<.0001f && std::abs(stopped-1.2f)<.0001f && normalTarget>0 && std::abs(runningTarget-normalTarget*1.2f)<.0001f)<<"}";
    }
    log<<"]";mmvrgame::ClearArmRun();*p=saved;mmvr::GetSettings()=settings;
    sControlStickMagnitude=oldMagnitude;sControlStickAngle=oldAngle;
    play->csCtx.state=cs;play->msgCtx.msgMode=msg;play->pauseCtx.state=pause;play->transitionTrigger=transition;
}
