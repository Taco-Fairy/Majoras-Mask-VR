#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Stone_heishi/z_en_stone_heishi.h"
}
// Real Shiro scene/AI, including his conditional bottle display in the hand.
static mmvr::Pad NativeShiroHandoff(PlayState* play,unsigned tick) {
    static int phase=0,age=0,complete=-1;
    static bool sawFull=false,sawEmpty=false;
    static std::ofstream log("native-shiro-handoff.log");
    static const bool blue=std::getenv("MMVR_SHIRO_BLUE")!=nullptr;
    const int item=blue?ITEM_POTION_BLUE:ITEM_POTION_RED;
    mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
    if(!phase) {
        if(tick<30)return pad;
        gSaveContext.save.day=gSaveContext.save.eventDayCount=1;
        gSaveContext.save.time=CLOCK_TIME(12,0);gSaveContext.save.isNight=false;
        gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
        CLEAR_WEEKEVENTREG(WEEKEVENTREG_41_40);CLEAR_WEEKEVENTREG(WEEKEVENTREG_41_80);
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=item;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;
        C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
        gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN]=BTN_ENABLED;
        gSaveContext.nextCutsceneIndex=gSaveContext.cutsceneTrigger=gSaveContext.respawnFlag=0;
        gSaveContext.save.cutsceneIndex=0;
        play->nextEntrance=ENTRANCE(ROAD_TO_IKANA,0);
        play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
        phase=1;return pad;
    }
    if(play->sceneId!=SCENE_IKANAMAE||play->transitionTrigger!=TRANS_TRIGGER_OFF||play->transitionMode!=TRANS_MODE_OFF)return pad;
    EnStoneheishi* npc=nullptr;
    for(auto* a=play->actorCtx.actorLists[ACTORCAT_NPC].first;a;a=a->next)
        if(a->id==ACTOR_EN_STONE_HEISHI&&a->update)npc=(EnStoneheishi*)a;
    if(!npc){log<<"FAIL Shiro absent\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
    if(phase==1) {
        static int settle=0;if(++settle<45)return pad;
        p->actor.world.pos=npc->actor.world.pos;p->actor.world.pos.z+=45;
        p->actor.prevPos=p->actor.world.pos;p->actor.velocity={};p->actor.speed=0;
        npc->actor.world.rot.y=npc->actor.shape.rot.y=0;
        p->actor.shape.rot.y=p->actor.world.rot.y=static_cast<s16>(0x8000);
        mmvrgame::SelectItem(play,SLOT_BOTTLE_1,item);phase=2;
    }
    ++age;play->actorCtx.lensActive=true;play->actorCtx.lensMaskSize=100;
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    mmvr::TrackingFrame frame{};frame.head.orientation.w=frame.origin.orientation.w=1;
    frame.timeSeconds=tick/20.;frame.epoch=1;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(frame));
    if(age%8==0)pad.buttons=Message_GetState(&play->msgCtx)==TEXT_STATE_PAUSE_MENU?BTN_CDOWN:BTN_A;
    sawFull|=npc->bottleDisplay==(blue?3:1);sawEmpty|=npc->bottleDisplay==2;
    if(age%20==0)log<<"age="<<age<<" action="<<npc->action<<" drink="<<npc->drinkBottleState<<" bottle="<<int(npc->bottleDisplay)<<" distance="<<npc->actor.xzDistToPlayer<<" lens="<<int(play->actorCtx.lensMaskSize)<<" playerFlags="<<p->stateFlags1<<" text="<<play->msgCtx.currentTextId<<"\n"<<std::flush;
    if(sawFull&&sawEmpty&&CHECK_WEEKEVENTREG(WEEKEVENTREG_41_40)&&gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_BOTTLE&&complete<0)complete=age;
    if(complete>=0&&age-complete>80){log<<"PASS Shiro potion handoff and reward completed\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
    else if(age>1600){log<<"FAIL Shiro handoff timeout\n"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();}
    return pad;
}
