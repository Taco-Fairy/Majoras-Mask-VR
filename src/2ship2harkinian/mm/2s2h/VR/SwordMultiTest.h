#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Mnk/z_en_mnk.h"
void EnMnk_Init(Actor*, PlayState*);
void EnMnk_Destroy(Actor*, PlayState*);
void EnMnk_Update(Actor*, PlayState*);
void EnMnk_MonkeyTiedUp_Draw(Actor*, PlayState*);
void EnMnk_MonkeyTiedUp_WaitForCutRope(EnMnk*, PlayState*);
}
#include "ScriptedMelee.h"
extern "C" {
#include "overlays/actors/ovl_En_Kendo_Js/z_en_kendo_js.h"
#include "overlays/actors/ovl_En_Maruta/z_en_maruta.h"
void func_80B372CC(EnMaruta*, PlayState*);
void func_80B37CA0(EnMaruta*, PlayState*);
#include "overlays/actors/ovl_Obj_Kendo_Kanban/z_obj_kendo_kanban.h"
s32 func_80B26BF8(EnKendoJs*, PlayState*);
void func_80B274BC(EnKendoJs*, PlayState*);
void ObjKendoKanban_SetupTumble(ObjKendoKanban*, PlayState*);
}
// Isolated native diagnostic: real actor hurtboxes, one buffered tracked sweep.
static void NativeSwordMultiTest(PlayState* play) {
    auto* p=GET_PLAYER(play);const Player saved=*p;const auto save=gSaveContext;
    const auto input=*CONTROLLER1(&play->state);const auto context=play->colChkCtx;
    const auto settings=mmvr::GetSettings();auto* previous=sPlayerControlInput;
    sPlayerControlInput=CONTROLLER1(&play->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword,1);
    p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->transformation=PLAYER_FORM_HUMAN;
    p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;
    const auto msg=play->msgCtx.msgMode;const auto cs=play->csCtx.state;
    play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
    std::ofstream log("native-sword-multi.log");int failures=0;
    auto frame=mmvr::TrackingFrame{};frame.origin.orientation.w=frame.head.orientation.w=1;frame.epoch=800;
    for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
    const auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
    const ItemId weapons[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY};
    for(int size:{100,200}) for(int weapon=0;weapon<4;++weapon) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordHitboxScale,float(size));
        mmvrgame::ClearTracking();mmvrgame::ClearCombat();MMVR_PlayerEquipSword(play,p,weapons[weapon]);
        EnDekubaba enemies[3]{};int contacts[3]{};
        for(int i=0;i<3;++i) {
            auto& e=enemies[i];e.actor.id=ACTOR_EN_DEKUBABA;e.actor.update=EnDekubaba_Update;
            e.actor.world.pos={0,2000,0};e.actor.home.pos=e.actor.world.pos;
            EnDekubaba_Init(&e.actor,play);e.actor.colChkInfo.health=32;
            e.collider.base.colMaterial=COL_MATERIAL_HIT0;e.collider.base.acFlags&=~AC_HARD;
            for(auto& element:e.colliderElements) {
                element.dim.worldSphere.center={(s16)(MMVR_NativeSwordLength(p)*.01f-3),2024,(s16)(i?8:-8)};
                if(i==2)element.dim.worldSphere.center.x=static_cast<s16>(3.5f+(MMVR_NativeSwordLength(p)*.01f-3.5f)*1.65f);
                element.dim.worldSphere.radius=i==2?2:5;element.base.acElemFlags|=ACELEM_ON;
            }
        }
        for(int sample=0;sample<105;++sample) {
            frame.timeSeconds=10+weapon*4+sample/90.0;
            float z=sample<30?-25:std::min(25.f,-25+(sample-30)*2.0f);
            frame.hands[mmvr::SwordController(mmvr::GetSettings())].position={0,-.5f,z/40};
            mmvrgame::RecordTracking(frame,view,head);
            auto model=mmvr::YawPose(0,0,2020,z);for(int k=0;k<3;++k)model.m[k][k]=.01f;
            mmvrgame::UpdateSwordDiagnostics(frame,model);
            if(sample%3==0) {
                CollisionCheck_ClearContext(play,&play->colChkCtx);
                for(auto& e:enemies) {
                    Collider_ResetJntSphAC(play,&e.collider.base);CollisionCheck_ResetDamage(&e.actor.colChkInfo);
                    e.collider.base.acFlags|=AC_ON;CollisionCheck_SetAC(play,&play->colChkCtx,&e.collider.base);
                }
                mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);
                CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);CollisionCheck_Damage(play,&play->colChkCtx);
                for(int i=0;i<3;++i) if(enemies[i].collider.base.acFlags&AC_HIT) ++contacts[i];
            }
        }
        log<<"size="<<size<<" extended="<<contacts[2]<<" weapon="<<weapon<<" first="<<contacts[0]<<" second="<<contacts[1]<<"\n";
        failures+=(contacts[0]!=1 || contacts[1]!=1 || contacts[2]!=(size==200?1:0));
        for(auto& e:enemies) EnDekubaba_Destroy(&e.actor,play);
    }
    mmvr::GetSettings().Set(mmvr::Setting::SwordHitboxScale,100);
    // Actual monkey collider and native rope-response action; the grip remains
    // stationary while wrist rotation moves the blade through its hurtbox.
    for (int hand=0; hand<2; ++hand) for (int weapon=0; weapon<4; ++weapon) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,hand);
        mmvrgame::ClearTracking();mmvrgame::ClearCombat();
        MMVR_PlayerEquipSword(play,p,weapons[weapon]);
        p->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
        gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_09_80);CLEAR_WEEKEVENTREG(WEEKEVENTREG_23_20);
        EnMnk monkey{};
        monkey.picto.actor.id=ACTOR_EN_MNK;
        monkey.picto.actor.params=(MONKEY_PATH_INDEX_NONE<<11)|(MONKEY_TIED_UP<<7)|MONKEY_SWITCH_FLAG_NONE;
        monkey.picto.actor.csId=CS_ID_NONE;
        monkey.picto.actor.update=EnMnk_Update;
        monkey.picto.actor.world.pos={MMVR_NativeSwordLength(p)*.007071f-3.f,2010,0};
        EnMnk_Init(&monkey.picto.actor,play);
        Matrix_Push();
        Matrix_Translate(monkey.picto.actor.world.pos.x,monkey.picto.actor.world.pos.y,monkey.picto.actor.world.pos.z,MTXMODE_NEW);
        Matrix_Scale(.012f,.012f,.012f,MTXMODE_APPLY);
        EnMnk_MonkeyTiedUp_Draw(&monkey.picto.actor,play);
        Matrix_Pop();
        const float visibleY=monkey.picto.actor.focus.pos.y;
        log<<"monkey placement bodyY="<<visibleY<<" colliderY="<<monkey.collider.dim.pos.y<<" height="<<monkey.collider.dim.height<<" rootY="<<monkey.unk_36C.yw<<"\n";
        monkey.actionFunc=EnMnk_MonkeyTiedUp_WaitForCutRope;
        bool responded=false, stationaryHit=false, intent=false;
        frame.epoch++;
        for(int sample=0;sample<105;++sample) {
            frame.timeSeconds=50+hand*20+weapon*4+sample/90.0;
            float angle=sample<30?-1.4f:std::min(1.4f,-1.4f+(sample-30)*.08f);
            int controller=mmvr::SwordController(mmvr::GetSettings());
            // Aim through the visible lower body/rope band with the grip inside normal reach.
            const float gripY=visibleY-15.f-MMVR_NativeSwordLength(p)*.007071f;
            frame.hands[controller].position={0,(gripY-2045)/40,0};
            frame.hands[controller].orientation={0,std::sin(angle*.5f),0,std::cos(angle*.5f)};
            mmvrgame::RecordTracking(frame,view,head);
            auto tilt=mmvr::YawPose(0);
            tilt.m[0][0]=tilt.m[0][1]=tilt.m[1][1]=.70710678f;tilt.m[1][0]=-.70710678f;
            auto model=mmvr::Multiply(tilt,mmvr::YawPose(angle,0,gripY,0));
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)model.m[row][col]*=.01f;
            mmvrgame::UpdateSwordDiagnostics(frame,model);
            if(sample%3==0) {
                CollisionCheck_ClearContext(play,&play->colChkCtx);
                Collider_ResetCylinderAC(play,&monkey.collider.base);
                EnMnk_Update(&monkey.picto.actor,play);
                if(std::getenv("MMVR_MONKEY_LEGACY_HITBOX"))monkey.collider.dim.height=30;
                mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);
                CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);
                intent |= MMVR_ScriptedMeleeState(p)!=0;
                if(monkey.collider.base.acFlags&AC_HIT) {
                    if(sample<30) stationaryHit=true;
                    EnMnk_MonkeyTiedUp_WaitForCutRope(&monkey,play);
                    responded |= monkey.picto.actor.textId==0x8D2;
                }
            }
        }
        bool expired=MMVR_ScriptedMeleeState(p)==0;
        log<<"monkey wrist hand="<<hand<<" weapon="<<weapon<<" response="<<responded
           <<" stationary="<<stationaryHit<<" intent="<<intent<<" expired="<<expired<<"\n";
        failures+=!responded || stationaryHit || !intent || !expired;
        // Physical sword is mandatory in first person. Test native fallback
        // in the reachable third-person mode, not with its removed switch.
        mmvr::ApplyViewMode(1);
        EnMnk_Update(&monkey.picto.actor,play);
        bool nativeHeight=monkey.collider.dim.height==30;
        failures+=!nativeHeight;
        log<<"monkey native height restored="<<nativeHeight<<"\n";
        mmvr::ApplyViewMode(2);
        EnMnk_Destroy(&monkey.picto.actor,play);
    }
    // Exercise the native dojo lesson and score consumers using tracked strokes.
    for(int hand=0;hand<2;++hand) for(int kind=0;kind<4;++kind) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,hand);
        mmvrgame::ClearTracking();mmvrgame::ClearCombat();
        MMVR_PlayerEquipSword(play,p,ITEM_SWORD_KOKIRI);
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
        p->actor.floorHeight=1980;
        p->actor.bgCheckFlags=kind==3?0:BGCHECKFLAG_GROUND;
        frame.epoch++;bool checked=false;int animation=-1,lesson=-1,points=-1,landed=-1,fragment=-1,logCut=0;
        for(int sample=0;sample<75&&!checked;++sample) {
            frame.timeSeconds=200+hand*30+kind*4+sample/90.0;
            const float travel=sample<30?0.f:std::min(50.f,(sample-30)*2.f);
            const float x=kind==2?travel:0, y=kind==1?-travel:0, z=(kind==0||kind==3)?travel:0;
            const int controller=mmvr::SwordController(mmvr::GetSettings());
            frame.hands[controller].orientation={0,0,0,1};
            frame.hands[controller].position={x/40,(-20+y)/40,z/40};
            mmvrgame::RecordTracking(frame,view,head);
            auto model=mmvr::YawPose(0,x,2020+y,z);
            for(int k=0;k<3;++k)model.m[k][k]=.01f;
            mmvrgame::UpdateSwordDiagnostics(frame,model);
            if(sample%3)continue;
            mmvrgame::ProcessCombatInput(play);
            if(!MMVR_ScriptedMeleeState(p))continue;
            checked=true;animation=MMVR_ScriptedMeleeAnimation(p);
            EnKendoJs teacher{};teacher.actor.csId=CS_ID_NONE;
            teacher.actor.id=ACTOR_EN_KENDO_JS;teacher.unk_28C=1;
            EnMaruta target{};target.actor.parent=&teacher.actor;
            target.actor.world.pos={0,2000,0};target.actionFunc=func_80B372CC;
            target.collider.base.acFlags=AC_HIT;
            func_80B37CA0(&target,play);
            logCut=target.unk_210!=0&&teacher.unk_28E==1&&teacher.unk_28C==0;
            teacher.unk_284=3+kind;
            lesson=func_80B26BF8(&teacher,play);
            teacher.unk_290=0;teacher.unk_284=0;teacher.unk_28E=1;
            gSaveContext.minigameScore=0;
            func_80B274BC(&teacher,play);points=play->interfaceCtx.minigamePoints;
            p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;
            landed=MMVR_ScriptedMeleeAnimation(p);
            ObjKendoKanban board{};board.actor.world.pos={0,2000,0};
            ObjKendoKanban_SetupTumble(&board,play);fragment=board.boardFragments;
        }
        const int expected[]={PLAYER_MWA_RIGHT_SLASH_1H,PLAYER_MWA_FORWARD_SLASH_1H,PLAYER_MWA_STAB_1H,PLAYER_MWA_JUMPSLASH_FINISH};
        const bool ok=checked&&logCut&&animation==expected[kind]&&lesson==0&&points==(kind==3?3:kind==2?2:1)&&
            landed==(kind==3?PLAYER_MWA_JUMPSLASH_FINISH:expected[kind])&&fragment==((kind==1||kind==3)?5:12);
        failures+=!ok;
        log<<"dojo hand="<<hand<<" kind="<<kind<<" armed="<<checked<<" animation="<<animation<<" lesson="<<lesson
           <<" logCut="<<logCut<<" points="<<points<<" landed="<<landed<<" fragment="<<fragment<<" pass="<<ok<<"\n";
    }
    mmvrgame::ClearTracking();mmvrgame::ClearCombat();*p=saved;gSaveContext=save;
    *CONTROLLER1(&play->state)=input;play->colChkCtx=context;mmvr::GetSettings()=settings;
    sPlayerControlInput=previous;play->msgCtx.msgMode=msg;play->csCtx.state=cs;
    log<<"failures="<<failures<<"\n";log.flush();std::_Exit(failures?2:0);
}
