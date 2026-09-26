#pragma once
extern "C" {
#include "overlays/actors/ovl_Eff_Stk/z_eff_stk.h"
#include "overlays/actors/ovl_Dm_Stk/z_dm_stk.h"
void EffStk_Update(Actor*, PlayState*);
void func_80BF0DE0(EffStk*, PlayState*);
}
static bool NativeIntroRepairChecks(PlayState* play) {
    auto* p=GET_PLAYER(play); auto saved=*p; auto cs=play->csCtx;
    auto settings=mmvr::GetSettings(); auto context=play->colChkCtx;
    auto list=play->actorCtx.actorLists[ACTORCAT_ITEMACTION];
    auto* camera=GET_ACTIVE_CAM(play); auto savedCamera=*camera;
    mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::VrCameraCutscenes,1);
    EffStk effect{}; effect.actionFunc=func_80BF0DE0;
    effect.actor.draw=[](Actor*,PlayState*){};effect.unk146=123;effect.unk148=-99;
    play->csCtx.state=CS_STATE_IDLE;
    EffStk_Update(&effect.actor,play);
    bool expired=!effect.actor.draw && effect.unk146==0 && effect.unk148==0;
    DmStk skull{};skull.actor.id=ACTOR_DM_STK;skull.actor.params=DM_STK_TYPE_SKULL_KID;
    skull.actor.update=skull.actor.draw=[](Actor*,PlayState*){};
    skull.headPos={50,100,150};skull.alpha=255;skull.shouldDraw=true;
    play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first=&skull.actor;
    float position[3]{};
    bool anchor=MMVR_SkullKidEffectAnchor(play,position)==1 && position[0]==50 && position[1]==100 && position[2]==150;
    camera->eye={-999,456,999};
    anchor &= MMVR_SkullKidEffectAnchor(play,position)==1 && position[0]==50 && position[1]==100 && position[2]==150;
    skull.headPos={75,130,190};
    anchor &= MMVR_SkullKidEffectAnchor(play,position)==1 && position[0]==75 && position[1]==130 && position[2]==190;
    skull.actor.update=nullptr;
    bool absent=MMVR_SkullKidEffectAnchor(play,position)==-1;
    mmvrgame::introPresentation.Begin(true);
    bool theater=MMVR_SkullKidEffectAnchor(play,position)==0;
    mmvrgame::introPresentation.Begin(false);
    play->actorCtx.actorLists[ACTORCAT_ITEMACTION]=list; *camera=savedCamera; play->csCtx=cs;

    // Exercise the actual XR camera callback at 120 Hz, with both controllers
    // unavailable. The torso/head must still track its view and crouch height.
    mmvrgame::ResetTestCamera();
    mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=98700;f.timeSeconds=98700;
    bool body=true;int samples=0;
    for(int step=0;step<120;++step) {
        f.timeSeconds+=1.0/120;f.head.position.x=step*.001f;f.head.position.y=-step*.001f;
        auto frame=mmvrgame::TestCameraFrame(f);
        auto view=mmvr::InversePose(frame.view);
        CollisionCheck_ClearContext(play,&play->colChkCtx);
        Collider_ResetCylinderAC(play,&p->cylinder.base);
        CollisionCheck_SetAC(play,&play->colChkCtx,&p->cylinder.base);
        MMVR_FilterAttackCollisions(play);
        bool headFound=false,torsoFound=false;
        for(int i=0;i<play->colChkCtx.colACCount;++i) {
            auto* collider=play->colChkCtx.colAC[i];
            if(collider->actor!=&p->actor)continue;
            if(collider->shape==COLSHAPE_SPHERE) {
                auto* sphere=reinterpret_cast<ColliderSphere*>(collider);
                if(sphere->dim.worldSphere.radius==4) {
                    auto center=sphere->dim.worldSphere.center;
                    headFound=std::abs(center.x-view.m[3][0])<1.1f && std::abs(center.z-view.m[3][2])<1.1f &&
                        std::abs(center.y-(view.m[3][1]+f.head.position.y*40-1))<1.1f;
                }
            } else if(collider->shape==COLSHAPE_CYLINDER && collider!=&p->cylinder.base) {
                auto* torso=reinterpret_cast<ColliderCylinder*>(collider);
                torsoFound=std::abs(torso->dim.pos.x-view.m[3][0])<1.1f && std::abs(torso->dim.pos.z-view.m[3][2])<1.1f;
            }
        }
        body &= frame.active&&headFound&&torsoFound;++samples;
    }
    std::ofstream log("native-intro-repairs.json");
    log<<"{\"cueExpired\":"<<expired<<",\"worldAnchor\":"<<anchor<<",\"missingActorHidden\":"<<absent
       <<",\"theaterUnchanged\":"<<theater<<",\"bodyAt120HzWithoutControllers\":"<<body<<",\"samples\":"<<samples<<"}";
    *p=saved;play->colChkCtx=context;mmvr::GetSettings()=settings;mmvrgame::ResetTestCamera();
    return expired&&anchor&&absent&&theater&&body;
}
