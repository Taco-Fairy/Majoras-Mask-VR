#pragma once
#include "combat.h"
extern "C" {
#include "overlays/actors/ovl_En_St/z_en_st.h"
void EnSt_Init(Actor*,PlayState*);
void EnSt_Destroy(Actor*,PlayState*);
void EnSt_Update(Actor*,PlayState*);
void func_808A6A78(EnSt*,PlayState*);
}
static void NativeWeaponReachTest(PlayState* play) {
    auto* p=GET_PLAYER(play);auto player=*p;auto save=gSaveContext;
    auto settings=mmvr::GetSettings();auto context=play->colChkCtx;
    auto* previousInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&play->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    const ItemId weapons[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,
                            ITEM_SWORD_GREAT_FAIRY,ITEM_SWORD_DEITY,ITEM_DEKU_STICK};
    bool passed=true;unsigned cases=0;
    std::ofstream log("native-weapon-reach.log");
    for(int weapon=0;weapon<6;++weapon)for(int left=0;left<2;++left)
        for(int percent : {100,150,200})for(int miss=0;miss<2;++miss) {
            *p=player;gSaveContext=save;
            p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->actor.world.rot={};
            p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
            p->heldActor=p->actor.child=nullptr;p->getItemDrawIdPlusOne=0;
            p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;
            p->transformation=weapon==4?PLAYER_FORM_FIERCE_DEITY:PLAYER_FORM_HUMAN;
            mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
            mmvr::GetSettings().Set(mmvr::Setting::SwordHitboxScale,percent);
            mmvr::SetInputContext(true,false);mmvrgame::ClearTracking();
            if(weapon==5) {
                AMMO(ITEM_DEKU_STICK)=10;
                p->heldItemAction=p->itemAction=PLAYER_IA_DEKU_STICK;p->heldItemId=ITEM_DEKU_STICK;p->unk_B0C=1;
            } else MMVR_PlayerEquipSword(play,p,weapons[weapon]);
            EnSt spider{};spider.actor.id=ACTOR_EN_ST;spider.actor.update=EnSt_Update;
            spider.actor.params=0x3F;spider.actor.world.pos={0,2000,0};spider.actor.home.pos=spider.actor.world.pos;
            EnSt_Init(&spider.actor,play);func_808A6A78(&spider,play);
            bool initialized=spider.collider2.base.actor==&spider.actor&&spider.collider2.dim.radius==18;
            float nativeTip=MMVR_NativeSwordLength(p),base=weapon==5?-428.26f:weapon==4?700.f:350.f;
            float length=(nativeTip-base)*.01f;
            float collision=weapon==5?length*percent/100.f:mmvr::SwordCollisionLength(length,1,percent,false);
            // Target's near surface is beyond the visible blade (or within it
            // for the unchanged stick at100), and a separate target is beyond
            // all assistance. Use the actual native Skulltula weak-side cylinder.
            float surfaceDistance=miss?base*.01f+collision+5.f:base*.01f+(length+collision)*.5f-.05f;
            // Native cylinder positions are integers. Round hit targets inward
            // and miss targets outward, so rounding cannot turn the unchanged
            // 100% stick's inside control into a tangent/outside target.
            spider.collider2.dim.pos={(s16)(miss?std::ceil(surfaceDistance+18):std::floor(surfaceDistance+18)),2020,0};
            spider.actor.colChkInfo.health=10;
            mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=50000+cases;
            for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
            auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
            int contacts=0;bool visualTip=true;
            for(int tick=0;tick<105;++tick) {
                f.timeSeconds=50000+cases*2+tick/90.;
                float z=tick<25?-25:std::min(25.f,-25+(tick-25)*2.f);
                f.hands[1-left].position={0,-.5f,z/40};
                mmvrgame::RecordTracking(f,view,head);
                auto model=mmvr::YawPose(0,0,2020,z);for(int k=0;k<3;++k)model.m[k][k]=.01f;
                mmvrgame::UpdateSwordDiagnostics(f,model);
                visualTip&=std::abs(p->meleeWeaponInfo[0].tip.x-nativeTip*.01f)<.01f;
                if(tick%3==0&&initialized) {
                    CollisionCheck_ClearContext(play,&play->colChkCtx);
                    Collider_ResetCylinderAC(play,&spider.collider2.base);
                    CollisionCheck_ResetDamage(&spider.actor.colChkInfo);
                    CollisionCheck_SetAC(play,&play->colChkCtx,&spider.collider2.base);
                    mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);
                    CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);
                    CollisionCheck_Damage(play,&play->colChkCtx);
                    contacts+=bool(spider.collider2.base.acFlags&AC_HIT);
                }
            }
            bool ok=initialized&&visualTip&&(miss?contacts==0:contacts>0);
            passed&=ok;++cases;
            if(!ok)log<<"FAIL weapon="<<weapon<<" left="<<left<<" scale="<<percent<<" miss="<<miss
                <<" initialized="<<initialized<<" contacts="<<contacts<<" visual="<<visualTip<<"\n";
            EnSt_Destroy(&spider.actor,play);
        }
    mmvrgame::ClearTracking();*p=player;gSaveContext=save;play->colChkCtx=context;
    mmvr::GetSettings()=settings;sPlayerControlInput=previousInput;
    log<<(passed?"PASS":"FAIL")<<" weapon-reach cases="<<cases<<"\n";log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
