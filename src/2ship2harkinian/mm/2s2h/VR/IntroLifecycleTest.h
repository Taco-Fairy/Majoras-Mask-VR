#pragma once
// Protected new-save introduction: native timing, native transitions, no user save writes.
#include "IntroRepairTest.h"
#include "screen_fade.h"
static mmvr::Pad NativeIntroLifecycle(PlayState* play, unsigned tick) {
    static std::ofstream log("native-intro-lifecycle.log");
    static bool sawPrologue=false, sawWoods=false, routeOk=true, released=false;
    static unsigned playableFrames=0;
    mmvr::Pad pad; pad.active=true;
    if (play->msgCtx.msgLength && tick % 20 == 0) pad.buttons = BTN_A;
    if (std::getenv("MMVR_INTRO_STARTUP_TEST")) {
        static const auto start=std::chrono::steady_clock::now();
        static std::ofstream startup("native-intro-startup.csv");
        if(tick==1) startup<<"tick,seconds,scene,cutsceneFrame,message,worldAlpha,environmentFill,environmentAlpha,interfaceAlpha,compositeAlpha,fadeBehindTheater\n";
        mmvr::SetNativeTestTracking(true);
        if(tick%10==0) startup<<tick<<","<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<","<<play->sceneId<<","<<play->csCtx.curFrame<<","<<play->msgCtx.currentTextId<<","<<int(play->worldCoverAlpha)<<","<<int(play->envCtx.fillScreen)<<","<<int(play->envCtx.screenFillColor[3])<<","<<int(play->interfaceCtx.screenFillAlpha)<<","<<mmvr::ScreenFade()[3]<<","<<mmvr::FadeBehindTheater(false,MMVR_TheaterPresentation(play),mmvr::ScreenFade()[3]>0)<<"\n"<<std::flush;
        if(tick==100) mmvr::RequestNativeCapture("intro-first-narration");
        if(tick==220) { startup.flush(); Ship::Context::GetRawInstance()->GetWindow()->Close(); }
    }
    const auto facts=mmvrgame::SceneFacts(play);
    const auto view=mmvrgame::SceneView(play);
    const bool active=mmvrgame::introPresentation.active;
    if (tick==1) { const auto* token=std::getenv("MMVR_SESSION_TOKEN");log<<"session "<<(token?token:"")<<"\n"; }
    sawPrologue |= play->sceneId==SCENE_SPOT00 && active;
    sawWoods |= play->sceneId==SCENE_LOST_WOODS && active;
    if (active) routeOk &= view==mmvr::SceneView::Theater;
    if (sawWoods && !active && !facts.cinematic && !facts.transition) {
        released=true; ++playableFrames; routeOk &= view==mmvr::SceneView::Player;
    }
    if (tick%100==0 || playableFrames==1)
        log<<"tick="<<tick<<" scene="<<play->sceneId<<" frame="<<play->csCtx.curFrame<<" opening="<<active<<" view="<<int(view)<<" cinematic="<<facts.cinematic<<" locked="<<facts.playerLocked<<"\n"<<std::flush;
    if ((play->sceneId==SCENE_SPOT00 && play->csCtx.curFrame==150) ||
        (play->sceneId==SCENE_LOST_WOODS && play->csCtx.curFrame==150) || playableFrames==10)
        mmvr::RequestNativeCapture(playableFrames?"intro-playable":play->sceneId==SCENE_SPOT00?"intro-prologue":"intro-woods");
    if (playableFrames>=30 || tick>7000) {
        log<<"prologue="<<sawPrologue<<" woods="<<sawWoods<<" routes="<<routeOk<<" released="<<released<<"\n";
        bool repairs = released && NativeIntroRepairChecks(play);
        log<<"PASS="<<(sawPrologue&&sawWoods&&routeOk&&released&&repairs)<<"\n"<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}
