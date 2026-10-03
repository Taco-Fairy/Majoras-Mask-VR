#pragma once
#include "Carry.h"
#include "HandGeometry.h"
#include "GoronCombat.h"
extern "C" {
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_Obj_Bombiwa/z_obj_bombiwa.h"
void EnIshi_SetupIdle(EnIshi*);
void MMVR_ClearACOverflow(CollisionCheckContext*);
int MMVR_CollisionACCount(CollisionCheckContext*);
Collider* MMVR_CollisionACAt(CollisionCheckContext*,int);
}
static bool NativeRockPickupChecks(PlayState* play,std::ofstream& log) {
    auto* p=GET_PLAYER(play); const auto originalPlayer=*p;
    const auto originalSave=gSaveContext; const auto settings=mmvr::GetSettings();
    const auto collision=play->colChkCtx;
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&play->state);
    const auto input=*sPlayerControlInput;
    auto* rock=reinterpret_cast<EnIshi*>(Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ISHI,
        p->actor.world.pos.x,p->actor.world.pos.y,p->actor.world.pos.z+100,0,0,0,1));
    if(!rock || rock->actor.init) {
        *sPlayerControlInput=input;sPlayerControlInput=oldInput;
        if(rock) Actor_Kill(&rock->actor);
        log<<"FAIL rock pickup reason=actor-not-ready"<<std::endl;return false;
    }
    const auto originalRock=*rock;
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::GetSettings().Set(mmvr::Setting::CarryGrabDistance,.25f);
    int failures=0,cases=0;
    // The real actor remains in the native actor list. Place it above geometry
    // to isolate object contact from room walls; preserve the live loaded mesh.
    for(int form:{PLAYER_FORM_GORON,PLAYER_FORM_HUMAN,PLAYER_FORM_ZORA})
    for(int hand=0;hand<2;++hand) for(float factor:{.5f,1.f,2.f}) for(float handSize:{1.f,1.75f}) {
        *p=originalPlayer; gSaveContext=originalSave; *rock=originalRock;
        p->actor.world.pos=p->actor.prevPos={0,2000,0};p->actor.velocity={};p->speedXZ=p->actor.speed=0;
        p->actor.shape.rot=p->actor.world.rot={};p->transformation=form;gSaveContext.save.playerForm=form;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
        p->heldActor=p->actor.child=nullptr;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;
        p->actionFunc=Player_Action_Idle;*sPlayerControlInput={};
        rock->actor.world.pos=rock->actor.prevPos={0,2000,74};rock->actor.parent=nullptr;
        rock->actor.bgCheckFlags=BGCHECKFLAG_GROUND;rock->actor.xzDistToPlayer=74;rock->actor.playerHeightRel=0;
        EnIshi_SetupIdle(rock);Collider_UpdateCylinder(&rock->actor,&rock->collider);
        CollisionCheck_ClearContext(play,&play->colChkCtx);
        mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();mmvrgame::ClearItemSelection();
        mmvrgame::ResetHandGeometry();mmvr::GetSettings().Set(mmvr::Setting::HandScale,handSize);
        rock->actor.update(&rock->actor,play);
        Vec3f surface{};bool inside=false;
        const bool mesh=mmvrgame::CarryPropSurface(&rock->actor,{0,2035,0},surface,inside);
        mmvr::TrackingFrame f{}; f.epoch=700000+cases;f.timeSeconds=1000+cases;
        f.origin.orientation.w=f.head.orientation.w=1;f.trackingScale=factor;
        for(int h=0;h<2;++h) {f.hands[h].orientation.w=f.aims[h].orientation.w=1;
            f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;f.hands[h].position={h?-.5f:.5f,-.3f,0};}
        const auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
        mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);
        auto sample=mmvrgame::SampleHandThrow(play,p,hand);
        auto palm=mmvrgame::CarryPalmPose(sample.pose,hand);
        const Vec3f desired{surface.x,surface.y,surface.z+4.f};
        for(int k=0;k<3;++k) (&f.hands[hand].position.x)[k]+=((&desired.x)[k]-palm.m[3][k])/40.f;
        f.aims[hand]=f.hands[hand];
        auto resolved=mmvrgame::ResolveHandGeometry(play,p,f,f,view,head);
        mmvrgame::RecordFormTracking(resolved,view,head);mmvrgame::RecordTracking(resolved,view,head);
        sample=mmvrgame::SampleHandThrow(play,p,hand);palm=mmvrgame::CarryPalmPose(sample.pose,hand);
        float separation=mmvrgame::CarryGrabSeparation(&rock->actor,{palm.m[3][0],palm.m[3][1],palm.m[3][2]});
        const bool grabbed=mmvrgame::TryGrabCarry(play,p,hand);
        bool released=!grabbed;
        if(grabbed) {
            ++play->gameplayFrames;
            sample=mmvrgame::SampleHandThrow(play,p,hand);sample.valid=sample.moving=true;sample.velocity={0,80,-60};
            released=mmvrgame::ReleaseThrowable(play,p,sample,false)&&!p->heldActor&&!rock->actor.parent;
        }
        const bool okay=mesh&&sample.valid&&released&&(grabbed==(form==PLAYER_FORM_GORON));
        failures+=!okay;++cases;
        log<<"pickup form="<<form<<" hand="<<hand<<" factor="<<factor<<" size="<<handSize
           <<" separation="<<separation<<" grabbed="<<grabbed<<" released="<<released<<" okay="<<okay<<'\n';
    }
    *p=originalPlayer;gSaveContext=originalSave;mmvr::GetSettings()=settings;
    MMVR_ClearACOverflow(&play->colChkCtx);play->colChkCtx=collision;
    *sPlayerControlInput=input;sPlayerControlInput=oldInput;
    mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();mmvrgame::ResetHandGeometry();
    *rock=originalRock;Actor_Kill(&rock->actor);
    log<<(failures?"FAIL":"PASS")<<" rock pickup cases="<<cases<<" failures="<<failures<<std::endl;
    return !failures;
}
static bool NativeRockPunchChecks(PlayState* play,std::ofstream& log) {
    auto* p=GET_PLAYER(play);const auto originalPlayer=*p;const auto originalSave=gSaveContext;
    const auto settings=mmvr::GetSettings();const auto collision=play->colChkCtx;
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&play->state);
    const auto input=*sPlayerControlInput;
    int failures=0;
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::GetSettings().Set(mmvr::Setting::FistHitboxScale,100);
    auto& objects=play->objectCtx;
    if(Object_GetSlot(&objects,OBJECT_BOMBIWA)<=OBJECT_SLOT_NONE) {
        const size_t bytes=gObjectTable[OBJECT_BOMBIWA].vromEnd-gObjectTable[OBJECT_BOMBIWA].vromStart;
        if(objects.numEntries>=ARRAY_COUNT(objects.slots)-1 ||
           uintptr_t(objects.slots[objects.numEntries].segment)+bytes>uintptr_t(objects.spaceEnd)) {
            *sPlayerControlInput=input;sPlayerControlInput=oldInput;mmvr::GetSettings()=settings;
            log<<"FAIL rock punch reason=object-capacity"<<std::endl;return false;
        }
        Object_SpawnPersistent(&objects,OBJECT_BOMBIWA);
    }
    for(int hand=0;hand<2;++hand) {
        *p=originalPlayer;gSaveContext=originalSave;
        p->actor.world.pos=p->actor.prevPos={0,2000,100};p->actor.velocity={};p->actor.speed=p->speedXZ=0;
        p->heldActor=p->actor.child=nullptr;p->currentMask=PLAYER_MASK_NONE;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
        p->itemAction=p->heldItemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;
        p->actionFunc=Player_Action_Idle;*sPlayerControlInput={};
        mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvrgame::ClearFormTracking();
        p->transformation=PLAYER_FORM_HUMAN;mmvrgame::ProcessGoronInput(play);
        p->transformation=PLAYER_FORM_GORON;gSaveContext.save.playerForm=PLAYER_FORM_GORON;
        Flags_UnsetSwitch(play,63);
        auto* rock=reinterpret_cast<ObjBombiwa*>(Actor_Spawn(&play->actorCtx,play,ACTOR_OBJ_BOMBIWA,0,2000,0,0,0,0,63));
        bool hit=false,fire=false;
        if(rock&&!rock->actor.init&&rock->actor.update) {
            mmvr::TrackingFrame f{};f.epoch=710000+hand;f.head.orientation.w=f.origin.orientation.w=1;
            for(int h=0;h<2;++h) {f.hands[h].orientation.w=f.aims[h].orientation.w=1;
                f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;}
            const auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
            mmvrgame::RecordFormTracking(f,view,head);
            sPlayerControlInput->press.button=BTN_B;mmvrgame::ProcessGoronInput(play);
            std::array<ColliderCylinder,60> fillers{};
            for(auto& col:fillers)col.base.shape=COLSHAPE_CYLINDER;
            for(int sample=0;sample<70&&rock->actor.update;++sample) {
                f.timeSeconds=1000+hand*10+sample/90.;
                f.hands[hand].position={0,-.5f,std::max(-.6f,.6f-std::max(0,sample-20)*.025f)};
                f.hands[1-hand].position={.7f,-.5f,0};
                mmvrgame::RecordFormTracking(f,view,head);mmvrgame::UpdateGoronCombat(f,view,head);
                fire|=mmvrgame::GoronPunchFire(hand)>0;
                if(sample%3==0) {
                    CollisionCheck_ClearContext(play,&play->colChkCtx);
                    for(auto& col:fillers)CollisionCheck_SetAC(play,&play->colChkCtx,&col.base);
                    rock->actor.xzDistToPlayer=100;
                    rock->actor.update(&rock->actor,play);
                    mmvrgame::ResolveGoronCombat(play);CollisionCheck_Damage(play,&play->colChkCtx);
                    hit|=(rock->collider.base.acFlags&AC_HIT)!=0;
                }
            }
        }
        const bool opened=rock&&!rock->actor.update;
        const bool okay=hit&&fire&&opened;failures+=!okay;
        log<<"punch hand="<<hand<<" spawned="<<(rock!=nullptr)<<" hit="<<hit<<" fire="<<fire<<" opened="<<opened<<" okay="<<okay<<'\n';
        MMVR_ClearACOverflow(&play->colChkCtx);
        if(rock&&rock->actor.update)Actor_Kill(&rock->actor);
    }
    *p=originalPlayer;gSaveContext=originalSave;mmvr::GetSettings()=settings;play->colChkCtx=collision;
    *sPlayerControlInput=input;sPlayerControlInput=oldInput;
    mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();
    log<<(failures?"FAIL":"PASS")<<" rock punch cases=2 failures="<<failures<<std::endl;
    return !failures;
}
