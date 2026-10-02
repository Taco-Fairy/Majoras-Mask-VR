#pragma once
// Private native fixture. Uses the runner's disposable save/config copies only.
#include "PlayerBody.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include <filesystem>
extern "C" {
#include "overlays/actors/ovl_Obj_Warpstone/z_obj_warpstone.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_Obj_Hamishi/z_obj_hamishi.h"
#include "overlays/actors/ovl_Obj_Bombiwa/z_obj_bombiwa.h"
#include "overlays/actors/ovl_Obj_Hugebombiwa/z_obj_hugebombiwa.h"
void ObjWarpstone_Init(Actor*, PlayState*);
void ObjWarpstone_Update(Actor*, PlayState*);
void ObjWarpstone_Destroy(Actor*, PlayState*);
s32 ObjWarpstone_ClosedIdle(ObjWarpstone*, PlayState*);
s32 ObjWarpstone_BeginOpeningCutscene(ObjWarpstone*, PlayState*);
s32 ObjWarpstone_PlayOpeningCutscene(ObjWarpstone*, PlayState*);
s32 ObjWarpstone_OpenedIdle(ObjWarpstone*, PlayState*);
void EnIshi_InitCollider(Actor*, PlayState*);
void func_809A0F20(Actor*, PlayState*);
void ObjHamishi_Update(Actor*, PlayState*);
void ObjBombiwa_Init(Actor*, PlayState*);
void ObjHugebombiwa_Init(Actor*, PlayState*);
}
static void NativeSavePropOwlTest(PlayState* play) {
    auto* p = GET_PLAYER(play);
    const auto player = *p;
    const auto save = gSaveContext;
    const auto settings = mmvr::GetSettings();
    const auto collision = play->colChkCtx;
    const auto msg = play->msgCtx.msgMode;
    const auto cs = play->csCtx.state;
    auto* previousInput = sPlayerControlInput;
    sPlayerControlInput = CONTROLLER1(&play->state);
    mmvr::ApplyViewMode(2);
    mmvr::SetNativeTestTracking(true);
    std::ofstream log("native-save-prop-owl.log");
    int failures = 0, rocks = 0, owls = 0;
    auto check = [&](bool ok, const char* name) {
        log << name << "=" << ok << "\n";
        failures += !ok;
    };
    p->actor.world.pos = {0, 2000, 0};
    p->actor.shape.rot = {};
    p->stateFlags1 = p->stateFlags2 = p->stateFlags3 = 0;
    p->transformation = PLAYER_FORM_HUMAN;
    p->heldActor = p->rideActor = nullptr;
    p->csAction = PLAYER_CSACTION_NONE;
    play->msgCtx.msgMode = MSGMODE_NONE;
    play->csCtx.state = CS_STATE_IDLE;
    p->cylinder.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL;
    p->cylinder.base.ocFlags2 = OC2_TYPE_1;
    p->cylinder.elem.ocElemFlags = OCELEM_ON;
    p->cylinder.dim.radius = 12;
    p->cylinder.dim.height = 50;
    p->cylinder.dim.yShift = 0;
    p->actor.colChkInfo.mass = 50;
    auto rockCheck = [&](Actor* actor, ColliderCylinder* collider) {
        actor->colChkInfo.mass = MASS_IMMOVABLE;
        actor->update = ObjHamishi_Update;
        Collider_UpdateCylinder(actor, collider);
        const float r = collider->dim.radius + p->cylinder.dim.radius;
        p->actor.world.pos = {-r - 10, 2000, 0};
        CollisionCheck_ClearContext(play, &play->colChkCtx);
        CollisionCheck_SetOC(play, &play->colChkCtx, &collider->base);
        check(mmvrgame::RoomScalePropBlocked(play, p, {r + 10, 2000, 0}), "prop crossing blocked");
        p->actor.world.pos.z = r + 20;
        check(!mmvrgame::RoomScalePropBlocked(play, p, {r + 10, 2000, r + 20}), "prop lateral clearance");
        p->actor.world.pos.z = 0;
        p->heldActor = actor;
        check(!mmvrgame::RoomScalePropBlocked(play, p, {r + 10, 2000, 0}), "held prop excluded");
        p->heldActor = nullptr;
        actor->update = nullptr;
        check(!mmvrgame::RoomScalePropBlocked(play, p, {r + 10, 2000, 0}), "dead prop excluded");
        actor->update = ObjHamishi_Update;
        p->actor.world.pos.x = -r + 1;
        Collider_UpdateCylinder(&p->actor, &p->cylinder);
        CollisionCheck_ResetDamage(&p->actor.colChkInfo);
        p->actor.colChkInfo.displacement = {};
        CollisionCheck_SetOC(play, &play->colChkCtx, &p->cylinder.base);
        CollisionCheck_OC(play, &play->colChkCtx);
        check(p->actor.colChkInfo.displacement.x < 0, "native stick body separation retained");
        Collider_DestroyCylinder(play, collider);
        ++rocks;
    };
    Flags_UnsetSwitch(play, 63);
    for (int variant=0; variant<2; ++variant) {
        EnIshi rock{};
        rock.actor.id=ACTOR_EN_ISHI; rock.actor.params=variant;
        rock.actor.world.pos=rock.actor.home.pos={0,2000,0};
        EnIshi_InitCollider(&rock.actor,play);
        rockCheck(&rock.actor,&rock.collider);
    }
    {
        ObjHamishi rock{}; rock.actor.id=ACTOR_OBJ_HAMISHI;
        rock.actor.world.pos=rock.actor.home.pos={0,2000,0};
        func_809A0F20(&rock.actor,play);
        rockCheck(&rock.actor,&rock.collider);
    }
    for(int variant=0;variant<2;++variant) {
        ObjBombiwa rock{};rock.actor.id=ACTOR_OBJ_BOMBIWA;rock.actor.params=63|(variant<<8);
        rock.actor.world.pos=rock.actor.home.pos={0,2000,0};
        ObjBombiwa_Init(&rock.actor,play);
        rockCheck(&rock.actor,&rock.collider);
        ObjHugebombiwa huge{};huge.actor.id=ACTOR_OBJ_HUGEBOMBIWA;huge.actor.params=63|(variant<<8);
        huge.actor.world.pos=huge.actor.home.pos={0,2000,0};
        ObjHugebombiwa_Init(&huge.actor,play);
        rockCheck(&huge.actor,&huge.collider);
    }
    const ItemId weapons[]={ITEM_SWORD_KOKIRI,ITEM_SWORD_RAZOR,ITEM_SWORD_GILDED,ITEM_SWORD_GREAT_FAIRY};
    mmvr::GetSettings().Set(mmvr::Setting::SwordHitboxScale,100);
    auto frame=mmvr::TrackingFrame{};
    frame.origin.orientation.w=frame.head.orientation.w=1;frame.epoch=1600;
    for(int hand=0;hand<2;++hand) {
        frame.handValid[hand]=frame.handTracked[hand]=frame.aimValid[hand]=true;
        frame.hands[hand].orientation.w=frame.aims[hand].orientation.w=1;
    }
    for(int hand=0;hand<2;++hand) for(int weapon=0;weapon<4;++weapon) for(int owlId=0;owlId<OWL_WARP_ENTRANCE;++owlId) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,hand);
        mmvrgame::ClearTracking();mmvrgame::ClearCombat();
        p->actor.world.pos={0,2000,0};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
        p->csAction=PLAYER_CSACTION_NONE;
        MMVR_PlayerEquipSword(play,p,weapons[weapon]);
        gSaveContext.save.saveInfo.playerData.owlActivationFlags=0;
        ObjWarpstone owl{};owl.dyna.actor.id=ACTOR_OBJ_WARPSTONE;
        owl.dyna.actor.params=owlId;owl.dyna.actor.csId=CS_ID_NONE;
        owl.dyna.actor.update=ObjWarpstone_Update;
        owl.dyna.actor.world.pos={MMVR_NativeSwordLength(p)*.01f-3,2000,0};
        ObjWarpstone_Init(&owl.dyna.actor,play);
        bool hit=false,stationary=false;
        frame.epoch++;
        for(int sample=0;sample<105&&!hit;++sample) {
            frame.timeSeconds=100+hand*20+weapon*4+sample/90.0;
            float z=sample<30?-35:std::min(35.f,-35+(sample-30)*2.f);
            frame.hands[mmvr::SwordController(mmvr::GetSettings())].position={0,-.5f,z/40};
            mmvrgame::RecordTracking(frame,mmvr::YawPose(0,0,2045,0),mmvr::YawPose(0));
            auto model=mmvr::YawPose(0,0,2020,z);
            for(int k=0;k<3;++k)model.m[k][k]=.01f;
            mmvrgame::UpdateSwordDiagnostics(frame,model);
            if(sample%3)continue;
            CollisionCheck_ClearContext(play,&play->colChkCtx);
            Collider_ResetCylinderAC(play,&owl.collider.base);
            Collider_UpdateCylinder(&owl.dyna.actor,&owl.collider);
            CollisionCheck_SetAC(play,&play->colChkCtx,&owl.collider.base);
            mmvrgame::ProcessCombatInput(play);MMVR_FilterAttackCollisions(play);
            CollisionCheck_AT(play,&play->colChkCtx);MMVR_AfterAttackCollision(play);
            if(owl.collider.base.acFlags&AC_HIT){hit=true;stationary=sample<30;}
        }
        check(hit&&!stationary,"tracked owl sword hit");
        if(hit) {
            ObjWarpstone_ClosedIdle(&owl,play);
            check(owl.actionFunc==ObjWarpstone_BeginOpeningCutscene,"owl native response");
            ObjWarpstone_BeginOpeningCutscene(&owl,play);
            for(int tick=0;tick<=OBJ_WARPSTONE_TIMER_ACTIVATE_THRESHOLD;++tick)
                ObjWarpstone_PlayOpeningCutscene(&owl,play);
            check(GET_OWL_STATUE_ACTIVATED(owlId)&&owl.actionFunc==ObjWarpstone_OpenedIdle,"owl activated");
            ++owls;
        }
        ObjWarpstone_Destroy(&owl.dyna.actor,play);
    }
    const char* keys[]={"gEnhancements.Saving.PersistentOwlSaves","gEnhancements.Saving.PauseSave",
                        "gEnhancements.Saving.RememberSaveLocation"};
    for(const char* key:keys) {
        const bool existed=CVarGet(key)!=nullptr;
        const int original=CVarGetInteger(key,0);
        CVarClear(key);SavingEnhancements_SetVRDefaults();
        check(CVarGetInteger(key,0)==1,"missing save default enabled");
        CVarSetInteger(key,0);SavingEnhancements_SetVRDefaults();
        check(CVarGetInteger(key,1)==0,"explicit save choice preserved");
        if(existed)CVarSetInteger(key,original);else CVarClear(key);
    }
    // The runner allowlists input copies and creates this unique artifacts path.
    // Never relax the save guard outside that disposable test folder.
    auto cwd=std::filesystem::current_path();
    bool isolated=cwd.parent_path().filename().string().rfind("game-audit-",0)==0 &&
                  cwd.parent_path().parent_path().filename()=="artifacts" &&
                  std::getenv("MMVR_SESSION_TOKEN") && std::getenv("MMVR_PROTECT_SAVES");
    check(isolated,"isolated native save directory");
    if(isolated) {
        const auto scene=play->sceneId;
        const auto trigger=play->transitionTrigger;
        const auto mode=play->transitionMode;
        const auto sram=play->sramCtx;
        play->sceneId=SCENE_BACKTOWN;play->transitionTrigger=TRANS_TRIGGER_OFF;play->transitionMode=TRANS_MODE_OFF;
        play->sramCtx.status=0;play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
        gSaveContext=save;gSaveContext.fileNum=2;gSaveContext.flashSaveAvailable=1;
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance=ENTRANCE(NORTH_CLOCK_TOWN,0);
        const int remember=CVarGetInteger(keys[2],0);CVarSetInteger(keys[2],1);
        ShipInit::Init(keys[2]);
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnSaveLoad>(2);
        gSaveContext.save.day=2;gSaveContext.save.time=CLOCK_TIME(15,30);
        gSaveContext.save.saveInfo.playerData.rupees=17;
        const auto inventory=gSaveContext.save.saveInfo.inventory;
#ifdef _WIN32
        _putenv_s("MMVR_PROTECT_SAVES","0");
#else
        setenv("MMVR_PROTECT_SAVES","0",1);
#endif
        check(SavingEnhancements_SaveGame(),"regular VR game save accepted");
#ifdef _WIN32
        _putenv_s("MMVR_PROTECT_SAVES","1");
#else
        setenv("MMVR_PROTECT_SAVES","1",1);
#endif
        auto loaded=std::make_unique<SaveContext>();
        check(SysFlashrom_ReadData(loaded.get(),gFlashOwlSaveStartPages[2*FLASH_SAVE_MAIN_MULTIPLIER],
              gFlashOwlSaveNumPages[2*FLASH_SAVE_MAIN_MULTIPLIER])==0,"ordinary save disk read");
        check(loaded->save.shipSaveInfo.pauseSaveEntrance==ENTRANCE(NORTH_CLOCK_TOWN,0),"remembered entrance saved");
        check(loaded->save.day==2&&loaded->save.time==CLOCK_TIME(15,30)&&loaded->save.saveInfo.playerData.rupees==17&&
              std::memcmp(&loaded->save.saveInfo.inventory,&inventory,sizeof(inventory))==0,"ordinary progress preserved");
        // Exercise the same reader/entrance selection used by file select.
        check(!GameInteractor_Should(VB_DELETE_OWL_SAVE,true),"persistent owl continuation");
        auto file=std::make_unique<FileSelectState>();
        file->isOwlSave[2+FILE_NUM_OWL_SAVE_OFFSET]=true;
        Sram_OpenSave(file.get(),&play->sramCtx);
        check(gSaveContext.save.entrance==ENTRANCE(NORTH_CLOCK_TOWN,0)&&
              gSaveContext.save.day==2&&gSaveContext.save.time==CLOCK_TIME(15,30)&&
              gSaveContext.save.saveInfo.playerData.rupees==17&&
              std::memcmp(&gSaveContext.save.saveInfo.inventory,&inventory,sizeof(inventory))==0,
              "native file continue restores entrance and progress");
        play->sramCtx.status=0;play->msgCtx.msgMode=MSGMODE_TEXT_DISPLAYING;
        check(!SavingEnhancements_SaveGame(),"dialogue save rejected");play->msgCtx.msgMode=MSGMODE_NONE;
        SET_EVENTINF(EVENTINF_34);check(!SavingEnhancements_SaveGame(),"minigame save rejected");CLEAR_EVENTINF(EVENTINF_34);
        play->transitionTrigger=TRANS_TRIGGER_START;check(!SavingEnhancements_SaveGame(),"transition save rejected");
        CVarSetInteger(keys[2],remember);ShipInit::Init(keys[2]);play->sceneId=scene;play->transitionTrigger=trigger;play->transitionMode=mode;
        play->sramCtx=sram;
    }
    mmvrgame::ClearTracking();mmvrgame::ClearCombat();mmvr::SetNativeTestTracking(false);
    *p=player;gSaveContext=save;mmvr::GetSettings()=settings;play->colChkCtx=collision;
    play->msgCtx.msgMode=msg;play->csCtx.state=cs;sPlayerControlInput=previousInput;
    log<<"rocks="<<rocks<<" owls="<<owls<<" failures="<<failures<<"\n";log.flush();
    std::_Exit(failures||rocks!=7||owls!=8*OWL_WARP_ENTRANCE?2:0);
}
