#pragma once
#include "KoumePotionTest.h"
#include "ShiroHandoffTest.h"
#include "GibdoHandoffTest.h"
#include "PaperHandoffTest.h"
extern "C" {
#include "overlays/actors/ovl_En_Trt/z_en_trt.h"
void EnTrt_GiveRedPotionForKoume(EnTrt*, PlayState*);
void EnTrt_BuyItemWithFanfare(EnTrt*, PlayState*);
}
// Private, isolated fixture. Native simulation and reward animations retain
// normal cadence; only dialogue advance input is automated.
static mmvr::Pad NativePotionShopLifecycle(PlayState* play, unsigned tick) {
    if(std::getenv("MMVR_PAPER_HANDOFF"))return NativePaperHandoff(play,tick);
    if(std::getenv("MMVR_GIBDO_HANDOFF"))return NativeGibdoHandoff(play,tick);
    if(std::getenv("MMVR_SHIRO_HANDOFF"))return NativeShiroHandoff(play,tick);
    if(std::getenv("MMVR_KOUME_POTION_TEST"))return NativeKoumePotionTest(play,tick);
    static int phase=0, age=0;
    static const int scenario=[] { const char* v=std::getenv("MMVR_POTION_CASE"); return v ? std::clamp(std::atoi(v),0,3) : 0; }();
    static std::ofstream log("native-potion-shop.log");
    mmvr::Pad pad; pad.active=true;
    if (!phase) {
        if (tick<30) return pad;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=1;
        gSaveContext.save.time=CLOCK_TIME(12,0);
        gSaveContext.save.isNight=false;
        gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;
        gSaveContext.save.cutsceneIndex=0;
        SET_WEEKEVENTREG(WEEKEVENTREG_TALKED_KOUME_INJURED);
        if(scenario==3){CLEAR_WEEKEVENTREG(WEEKEVENTREG_SAVED_KOUME);CLEAR_WEEKEVENTREG(WEEKEVENTREG_FAILED_RECEIVED_RED_POTION_FOR_KOUME_SHOP);}
        if(scenario==1 || scenario==2) SET_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_KOTAKE_BOTTLE);
        else CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_KOTAKE_BOTTLE);
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_RED_POTION_FOR_KOUME);
        for(int i=SLOT_BOTTLE_1;i<=SLOT_BOTTLE_6;++i) gSaveContext.save.saveInfo.inventory.items[i]=ITEM_NONE;
        if(scenario==1 || scenario==2)gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;
        play->nextEntrance=ENTRANCE(MAGIC_HAGS_POTION_SHOP,0);
        play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
        phase=1;log<<"enter shop\n"<<std::flush;return pad;
    }
    if(play->sceneId!=SCENE_WITCH_SHOP || play->transitionTrigger!=TRANS_TRIGGER_OFF || play->transitionMode!=TRANS_MODE_OFF)return pad;
    ++age;
    mmvr::ApplyViewMode(2);
    mmvr::SetNativeTestTracking(true);
    mmvr::TrackingFrame frame{};
    frame.head.orientation.w=frame.origin.orientation.w=1;
    frame.timeSeconds=tick/20.0;frame.epoch=1;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(frame));
    if(phase==1 && age>=45) {
        EnTrt* shop=nullptr;
        for(Actor* a=play->actorCtx.actorLists[ACTORCAT_NPC].first;a;a=a->next)if(a->id==ACTOR_EN_TRT)shop=(EnTrt*)a;
        if(!shop){log<<"FAIL no shopkeeper\n";Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
        shop->cursorIndex=0;
        if(scenario!=3)shop->actionFunc=scenario==2?EnTrt_BuyItemWithFanfare:EnTrt_GiveRedPotionForKoume;
        auto* p=GET_PLAYER(play);
        p->actor.world.pos={shop->actor.world.pos.x,shop->actor.world.pos.y,shop->actor.world.pos.z+80};
        if(scenario==3){
            // Native shop talk rectangle, not a direct reward/action injection.
            p->actor.world.pos.x=-37.f;p->actor.world.pos.z=0.f;
            p->actor.prevPos=p->actor.world.pos;
        }
        log<<"offer scenario="<<scenario<<"\n"<<std::flush;phase=2;age=0;
    }
    if(phase==2 && age%8==0)pad.buttons=BTN_A;
    if(age%20==0)log<<"age="<<age<<" item="<<int(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1])<<" msg="<<int(play->msgCtx.msgMode)<<" draw="<<int(GET_PLAYER(play)->getItemDrawIdPlusOne)<<"\n"<<std::flush;
    if(phase==2 && age>(scenario==3?800:220)){
        const bool received=CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_KOTAKE_BOTTLE)&&Inventory_HasItemInBottle(ITEM_POTION_RED);
        log<<(received?"PASS":"FAIL")<<" potion lifecycle scenario="<<scenario<<"\n"<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return pad;
}
